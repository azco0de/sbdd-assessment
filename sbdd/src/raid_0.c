
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <kernel_version.h>
#include <linux/string.h>
#include <linux/parser.h>
#include <trace/events/block.h>
#include <sbdd.h>
#include <raid_0.h>
#include <raid_0_cfg.h>

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

    _disk->bdev_raw = blkdev_get_by_path(name, FMODE_READ | FMODE_WRITE,  NULL);
    if(IS_ERR(_disk->bdev_raw))
    {
	    pr_err("raid_0:: cannot open block device '%s' \n", name);
        kfree(_disk);
	    return NULL;
    }

    return _disk;
}

static int __sbdd_raid_0_destroy_disk(struct sbdd_raid_0_disk* disk)
{
    if(disk)
    {
        blkdev_put(disk->bdev_raw, FMODE_READ | FMODE_WRITE);
        kfree(disk);

        return 0;
    }

    return -EINVAL;
}

static struct sbdd_raid_0_disk* __sbdd_raid_0_map_sector_to_disk(struct sbdd_raid_0* raid_0, sector_t source_sector,sector_t* mapped_sector)
{
    struct sbdd_raid_0_disk* _disk = NULL;

    __u64 _chunks_in_sectors = 0;
	__u64 _chunk_index = 0;
	__u64 _target_sectors = 0;
	__u32 _target_disk = 0;

    _chunks_in_sectors = raid_0->strip_size << 1;

	_chunk_index = source_sector / _chunks_in_sectors;

    _target_disk = (source_sector % (_chunks_in_sectors * raid_0->disks_count)) / _chunks_in_sectors;

    _target_sectors = source_sector;
	_target_sectors /= _chunks_in_sectors * raid_0->disks_count;
	_target_sectors *= _chunks_in_sectors;
	_target_sectors += source_sector % _chunks_in_sectors;

    return _disk;
}

static blk_qc_t __sbdd_raid_0_process_bio(struct sbdd_raid_0* raid_0, struct bio* bio)
{
    struct sbdd_raid_0_disk*    _target_disk = NULL;
    struct sbdd*                _dev = raid_0->ctx;
    sector_t                    _source_sector = bio->bi_iter.bi_sector;
    sector_t                    _target_sector = 0;

    _target_disk = __sbdd_raid_0_map_sector_to_disk(raid_0, _source_sector, &_target_sector);
    if(_target_disk == NULL)
    {
        pr_err("raid_0:: can't map disk \n");
        return BLK_STS_TARGET;
    }

    bio_set_dev(bio, _target_disk->bdev_raw);
	bio->bi_iter.bi_sector = _target_sector;

#if (BUILT_KERNEL_VERSION > KERNEL_VERSION(5, 14, 0))
	trace_block_bio_remap(bio, disk_devt(_dev->gd), _source_sector);
#endif

#if (BUILT_KERNEL_VERSION < KERNEL_VERSION(5, 9, 0))
    generic_make_request(bio);
#else
	return submit_bio_noacct(bio);
#endif

}

int sbdd_raid_0_create(struct sbdd_raid_0* raid_0, char* cfg, void* ctx)
{
    int _ret = 0;
    int _idx = 0;
    sbdd_raid_0_config_t _cfg;

    _ret = sbdd_raid_0_create_config(cfg, &_cfg);
    if(_ret)
    {
        pr_err("raid_0:: parsing config error: %d \n", _ret);
        return _ret;
    }

    spin_lock_init(&raid_0->disks_lock);

    /* create raid disks*/

    raid_0->disks = kzalloc(sizeof(struct sbdd_raid_0_disk*) * _cfg.disks_count, GFP_KERNEL);
    if(!raid_0->disks)
    {
        return -ENOMEM;
    }

    for(_idx = 0; _idx < _cfg.disks_count; ++ _idx)
    {
        raid_0->disks[_idx] = __sbdd_raid_0_create_disk(_cfg.disks[_idx]);
        if (!raid_0->disks[_idx]) 
        {
            sbdd_raid_0_destroy_config(&_cfg);
            return -ENOMEM;
        }
    }

    raid_0->ctx = ctx;

    raid_0->strip_size = _cfg.strip_size;
    raid_0->disks_count = _cfg.disks_count;

    sbdd_raid_0_destroy_config(&_cfg);

    return 0;
}

void sbdd_raid_0_destroy(struct sbdd_raid_0* raid_0)
{
    int                         _ret = 0;
    int                         _disk_idx = 0;
    struct sbdd_raid_0_disk*    _disk = NULL;

    for (; _disk_idx < raid_0->disks_count; ++_disk_idx) 
    {
		_disk = raid_0->disks[_disk_idx];
        if(_disk)
        {
            _ret = __sbdd_raid_0_destroy_disk(_disk);
            if(_ret)
            {
                pr_err("raid_0:: delete disk '%s' error:%d\n", _disk->name, _ret);
            }
        }
    }

    if(raid_0->disks)
    {
        kfree(raid_0->disks);
    }
}

blk_qc_t sbdd_raid_0_process_bio(struct bio* bio)
{
    blk_qc_t _ret = BLK_STS_OK;

#if (BUILT_KERNEL_VERSION > KERNEL_VERSION(5, 14, 0))
	struct sbdd* _dev = bio->bi_bdev->bd_disk->private_data;
#else
	struct sbdd* _dev = bio->bi_disk->private_data;
#endif

   _ret = __sbdd_raid_0_process_bio(&_dev->raid_0, bio);
   if(_ret != BLK_STS_OK)
   {
        pr_err("raid_0:: processing bio error: %d",_ret);
   }

    return _ret;

}