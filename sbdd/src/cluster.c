
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <kernel_version.h>
#include <cluster.h>

static struct sbdd_cluster_disk* __sbdd_cluster_alloc_disk(const char* name)
{
    struct sbdd_cluster_disk* _disk = NULL;

	_disk = kzalloc(sizeof(struct sbdd_cluster_disk), GFP_KERNEL);
	if (!_disk) 
    {
        pr_err("__sbdd_cluster_alloc_disk:: cannot alloc block device '%s' \n", name);
		return NULL;
	}

	INIT_LIST_HEAD(&_disk->link);

    scnprintf(_disk->name, DISK_NAME_LEN, name);

    _disk->bdev_raw = blkdev_get_by_path(name, FMODE_READ | FMODE_WRITE,  NULL);
    if(IS_ERR(_disk->bdev_raw))
    {
	    pr_err("__sbdd_cluster_alloc_disk:: cannot open block device '%s' \n", name);
        kfree(_disk);
	    return NULL;
    }

    return _disk;
}

static int __sbdd_cluster_free_disk(struct sbdd_cluster_disk* disk)
{
    if(disk)
    {
        blkdev_put(disk->bdev_raw, FMODE_READ | FMODE_WRITE);
        kfree(disk);

        return 0;
    }

    return -EINVAL;
}

int sbdd_cluster_create(struct sbdd_cluster* cluster, void* ctx)
{
    INIT_LIST_HEAD(&cluster->cluster_disks);

    spin_lock_init(&cluster->cluster_disks_lock);

    cluster->ctx = ctx;

    return 0;
}

void sbdd_cluster_destroy(struct sbdd_cluster* cluster)
{
    int                         _ret = 0;
    struct sbdd_cluster_disk*   _disk = NULL;
    struct list_head*           _entry = NULL;

    spin_lock_irq(&cluster->cluster_disks_lock);

    list_for_each(_entry, &cluster->cluster_disks)
    {
        _disk = list_entry(_entry, struct sbdd_cluster_disk, link);

        _ret = __sbdd_cluster_free_disk(_disk);
        if(_ret)
        {
            pr_err("sbdd_cluster_destroy:: delete disk '%s' error:%d\n", _disk->name, _ret);
        }
    }

    spin_unlock_irq(&cluster->cluster_disks_lock);
}

int sbdd_cluster_is_empty(struct sbdd_cluster* cluster)
{
    int _is_empty = 1;

    spin_lock_irq(&cluster->cluster_disks_lock);

    _is_empty = list_empty(&cluster->cluster_disks);

    spin_unlock_irq(&cluster->cluster_disks_lock);

    return _is_empty;
}

int sbdd_cluster_add_disk(struct sbdd_cluster* cluster, const char* name)
{
    struct sbdd_cluster_disk* _disk = __sbdd_cluster_alloc_disk(name);

	if (!_disk) {
		return -ENOMEM;
	}

    spin_lock_irq(&cluster->cluster_disks_lock);

    list_add(&_disk->link, &cluster->cluster_disks);

    spin_unlock_irq(&cluster->cluster_disks_lock);

    pr_info("sbdd_cluster_add_disk:: add disk '%s' successful \n", name);

    return 0;
}

int sbdd_cluster_rem_disk(struct sbdd_cluster* cluster, const char* name)
{
    struct sbdd_cluster_disk*   _disk = NULL;
    struct list_head*           _entry = NULL;

    spin_lock_irq(&cluster->cluster_disks_lock);

    list_for_each(_entry, &cluster->cluster_disks)
    {
        _disk = list_entry(_entry, struct sbdd_cluster_disk, link);

        if(strncmp(name, _disk->name, DISK_NAME_LEN) == 0)
        {
            list_del(&_disk->link);
            break;
        }
        
        _disk = NULL;
    }

    spin_unlock_irq(&cluster->cluster_disks_lock);

    return _disk ? __sbdd_cluster_free_disk(_disk) : -ENODEV;
}