
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <kernel_version.h>
#include <linux/string.h>
#include <linux/parser.h>
#include <trace/events/block.h>
#include <sbdd.h>
#include <raid_0.h>

static struct sbdd_raid_0_disk* __sbdd_raid_0_create_disk(const char* name)
{
    struct sbdd_raid_0_disk* _disk = NULL;

	_disk = kzalloc(sizeof(struct sbdd_raid_0_disk), GFP_KERNEL);
	if (!_disk) 
    {
        pr_err("raid_0:: cannot alloc block device '%s' \n", name);
		return NULL;
	}

    scnprintf(_disk->name, DISK_NAME_LEN, name);

    _disk->bdev_raw = blkdev_get_by_path(name, SBDD_RAID_0_FMODE,  NULL);
    if(IS_ERR(_disk->bdev_raw))
    {
	    pr_err("raid_0:: cannot open block device '%s' \n", name);
        kfree(_disk);
	    return NULL;
    }

    _disk->capacity = bdev_nr_sectors(_disk->bdev_raw);
    _disk->max_sectors = queue_max_hw_sectors(bdev_get_queue(_disk->bdev_raw));

    pr_info("raid_0:: allocate disk name: %s, capacity: %llu, max_sectors: %u \n", _disk->name, _disk->capacity, _disk->max_sectors);

    return _disk;
}

static int __sbdd_raid_0_destroy_disk(struct sbdd_raid_0_disk* disk)
{
    if(disk)
    {
        blkdev_put(disk->bdev_raw, SBDD_RAID_0_FMODE);
        kfree(disk);

        return 0;
    }

    return -EINVAL;
}

static struct sbdd_raid_0_disk* __sbdd_raid_0_map_sector_to_disk(struct sbdd_raid_0* raid_0, sector_t source_sector, sector_t* mapped_sector)
{
    struct sbdd_raid_0_disk* _disk = NULL;

    __u64 _chunks_in_sectors = 0;
	__u64 _chunk_index = 0;
	__u64 _target_sectors = 0;
	__u32 _target_disk = 0;

    _chunks_in_sectors = raid_0->config.strip_size << 1;

	_chunk_index = source_sector / _chunks_in_sectors;

    _target_disk = (source_sector % (_chunks_in_sectors * raid_0->config.disks_count)) / _chunks_in_sectors;

    _target_sectors = source_sector;
	_target_sectors /= _chunks_in_sectors * raid_0->config.disks_count;
	_target_sectors *= _chunks_in_sectors;
	_target_sectors += source_sector % _chunks_in_sectors;

    *mapped_sector = _target_sectors;

    _disk = raid_0->disks[_target_disk];

    pr_info("raid_0:: chunks_in_sectors:%llu, chunk_index:%llu, source_sector:%llu, target_sectors:%llu, disk:%s \n",
    _chunks_in_sectors, _chunk_index, source_sector, _target_sectors, _disk->name);

    return _disk;
}

static blk_qc_t __sbdd_raid_0_process_bio(struct sbdd_raid_0* raid_0, struct bio* bio)
{
    struct sbdd_raid_0_disk*    _target_disk = NULL;
    struct sbdd*                _dev = raid_0->ctx;
    struct bio*                 _split_bio = NULL;
    int                         _chunks_in_sectors = raid_0->config.strip_size << 1;
    sector_t                    _source_sector = bio->bi_iter.bi_sector;
    sector_t                    _target_sector = 0;

    pr_info("raid_0_process_bio:: bi_sector=%llu, bio_sectors=%u, chunks_in_sector=%d \n", 
                bio->bi_iter.bi_sector, bio_sectors(bio), _chunks_in_sectors);

    if(bio_sectors(bio) > _chunks_in_sectors)
    {
        _split_bio = bio_split(bio, _chunks_in_sectors, GFP_NOIO, &raid_0->bio_set);
		bio_chain(_split_bio, bio);
		submit_bio_noacct(bio);
		bio = _split_bio;
    }

    _target_disk = __sbdd_raid_0_map_sector_to_disk(raid_0, _source_sector, &_target_sector);
    if(_target_disk == NULL)
    {
        pr_err("raid_0:: can't map disk \n");
        return BLK_STS_TARGET;
    }

    pr_info("raid_0_process_bio:: dir=%d, source_sector=%llu, target_sector=%llu, disk=%s", 
                bio_data_dir(bio), _source_sector, _target_sector, _target_disk->name);

    bio_set_dev(bio, _target_disk->bdev_raw);
	bio->bi_iter.bi_sector = _target_sector;

#if (BUILT_KERNEL_VERSION > KERNEL_VERSION(5, 14, 0))
	trace_block_bio_remap(bio, disk_devt(_dev->gd), _source_sector);
#else
    trace_block_bio_remap(bdev_get_queue(_target_disk->bdev_raw), bio, bio->bi_bdev->bd_dev, bio->bi_sector);
#endif

#if (BUILT_KERNEL_VERSION < KERNEL_VERSION(5, 9, 0))
    generic_make_request(bio);
#else
	return submit_bio_noacct(bio);
#endif

}

int sbdd_raid_0_create(struct sbdd_raid_0* raid_0, char* cfg, void* ctx)
{
    int     _ret = 0;
    __u32   _idx = 0;

    _ret = bioset_init(&raid_0->bio_set, BIO_POOL_SIZE, 0, 0);
	if (_ret)
    {
        pr_err("raid_0:: bioset_init error: %d \n", _ret);
        return _ret;
    }

    _ret = sbdd_raid_0_create_config(cfg, &raid_0->config);
    if(_ret)
    {
        pr_err("raid_0:: parsing config error: %d \n", _ret);
        sbdd_raid_0_destroy_config(&raid_0->config);
        return _ret;
    }

    spin_lock_init(&raid_0->disks_lock);

    /* create raid disks*/

    raid_0->disks = kzalloc(sizeof(struct sbdd_raid_0_disk*) * raid_0->config.disks_count, GFP_KERNEL);
    if(!raid_0->disks)
    {
        pr_err("raid_0:: can't alloc disks with count: %d \n", raid_0->config.disks_count);
        return -ENOMEM;
    }

    for(_idx = 0; _idx < raid_0->config.disks_count; ++ _idx)
    {
        raid_0->disks[_idx] = __sbdd_raid_0_create_disk(raid_0->config.disks[_idx]);
        if (!raid_0->disks[_idx]) 
        {
            return -ENOMEM;
        }
    }

    raid_0->ctx = ctx;

    pr_info("raid_0:: disks count: %d, stripe size: %d \n", raid_0->config.disks_count, raid_0->config.strip_size);

    return 0;
}

void sbdd_raid_0_destroy(struct sbdd_raid_0* raid_0)
{
    int                         _ret = 0;
    __u32                       _disk_idx = 0;
    struct sbdd_raid_0_disk*    _disk = NULL;

    for (; _disk_idx < raid_0->config.disks_count; ++_disk_idx) 
    {
		_disk = raid_0->disks[_disk_idx];
        if(_disk)
        {
            _ret = __sbdd_raid_0_destroy_disk(_disk);
            if(_ret)
            {
                pr_err("raid_0:: delete disk '%s' error:%d \n", _disk->name, _ret);
            }
        }
    }

    if(raid_0->disks)
    {
        kfree(raid_0->disks);
    }

    bioset_exit(&raid_0->bio_set);

    sbdd_raid_0_destroy_config(&raid_0->config);
}

__u32 sbdd_raid_0_get_capacity(struct sbdd_raid_0* raid_0)
{
    __u32                       _capacity = 0;
    __u32                       _disk_idx = 0;
    struct sbdd_raid_0_disk*    _disk = NULL;

    for (; _disk_idx < raid_0->config.disks_count; ++_disk_idx) 
    {
        _disk = raid_0->disks[_disk_idx];

        if (_disk_idx == 0) 
        {
			_capacity = _disk->capacity;
		} 
        else 
        {
			if (_disk->capacity < _capacity) 
            {
				_capacity = _disk->capacity;
			}
		}
    }

    return _capacity * raid_0->config.disks_count;
}

__u64 sbdd_raid_0_get_max_sectors(struct sbdd_raid_0* raid_0)
{
    __u64                       _max_sectors = 0;
    __u32                       _disk_idx = 0;
    struct sbdd_raid_0_disk*    _disk = NULL;

    for (; _disk_idx < raid_0->config.disks_count; ++_disk_idx) 
    {
        _disk = raid_0->disks[_disk_idx];

        if (_disk_idx == 0) 
        {
			_max_sectors = _disk->max_sectors;
		} 
        else 
        {
			if (_disk->max_sectors > _max_sectors) 
            {
				_max_sectors = _disk->max_sectors;
			}
		}
    }

    return _max_sectors;

}

blk_qc_t sbdd_raid_0_process_bio(struct bio* bio)
{
    blk_qc_t _ret = BLK_STS_OK;

#if (BUILT_KERNEL_VERSION > KERNEL_VERSION(5, 14, 0))
	struct sbdd* _dev = bio->bi_bdev->bd_disk->private_data;
#else
	struct sbdd* _dev = bio->bi_disk->private_data;
#endif

   return __sbdd_raid_0_process_bio(&_dev->raid_0, bio);

}