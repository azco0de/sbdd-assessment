#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <kernel_version.h>

#include <linux/mm.h>
#include <linux/bio.h>
#include <linux/bvec.h>
#include <linux/init.h>
#include <linux/stat.h>
#include <linux/slab.h>
#include <linux/numa.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/vmalloc.h>
#include <linux/moduleparam.h>

#include <disk.h>
#include <io.h>

static struct sbdd      __sbdd;
static int              __sbdd_major = 0;
static unsigned long    __sbdd_raid_type = 0;
static char*			__sbdd_raid_config = NULL;

static int __sbdd_create_raid(__u32* raid_capacity, __u64* max_raid_sectors)
{
	int ret = 0;

	process_bio_t _process_bio = NULL;

	/* Check if raid type is supported*/
	if(__sbdd_raid_type > 1)
	{
		pr_err("wrong raid type: %lu\n", __sbdd_raid_type);
		return -ENAVAIL;
	}

	if(__sbdd_raid_type == 0)
	{
		ret = sbdd_raid_0_create(&__sbdd.raid_0, __sbdd_raid_config, &__sbdd);
		if(ret)
		{
			pr_err("creating raid_0 error=%d\n", ret);
			return ret;
		}

		_process_bio = sbdd_raid_0_process_bio;
	}

	*raid_capacity		= sbdd_raid_0_get_capacity(&__sbdd.raid_0);
	*max_raid_sectors	= sbdd_raid_0_get_max_sectors(&__sbdd.raid_0);

	/* Create raid io */
	ret = sbdd_io_create(&__sbdd.io, _process_bio, &__sbdd);
	if(ret)
	{
		pr_err("creating io error=%d\n", ret);
		return ret;
	}

	ret = sbdd_io_start(&__sbdd.io);
	if(ret)
	{
		pr_err("starting io error=%d\n", ret);
		return ret;
	}

	pr_info("created raid type: %lu, capacity: %u, sectors: %llu \n", __sbdd_raid_type, *raid_capacity, *max_raid_sectors);

	return 0;
}

static void __sbdd_destroy_raid(void)
{
	/* Blocking call to io */
	sbdd_io_stop(&__sbdd.io);

	sbdd_io_destroy(&__sbdd.io);

	if(__sbdd_raid_type == 0)
	{
		sbdd_raid_0_destroy(&__sbdd.raid_0);
	}
}

#ifdef BLK_MQ_MODE
static struct blk_mq_ops const __sbdd_blk_mq_ops = {
	/*
	The function receives requests for the device as arguments
	and can use various functions to process them. The functions
	used to process requests in the handler are described below:

	blk_mq_start_request()   - must be called before processing a request
	blk_mq_requeue_request() - to re-send the request in the queue
	blk_mq_end_request()     - to end request processing and notify upper layers
	*/
	.queue_rq = sbdd_io_queue_rq,
};
#endif

/*
There are no read or write operations. These operations are performed by
the request() function associated with the request queue of the disk.
*/
static struct block_device_operations const __sbdd_bdev_ops = {
	.owner = THIS_MODULE,
#ifndef BLK_MQ_MODE
#if (BUILT_KERNEL_VERSION > KERNEL_VERSION(5, 8, 0))
	.submit_bio = sbdd_io_submit_bio,
#endif
#endif
};

static int sbdd_create(void)
{
	int ret = 0;
	__u32 _raid_capacity = 0;
	__u64 _raid_sectors = 0;

	/*
	This call is somewhat redundant, but used anyways by tradition.
	The number is to be displayed in /proc/devices (0 for auto).
	*/
	pr_info("registering blkdev\n");
	__sbdd_major = register_blkdev(0, SBDD_NAME);
	if (__sbdd_major < 0) {
		pr_err("call register_blkdev() failed with %d\n", __sbdd_major);
		return -EBUSY;
	}

	memset(&__sbdd, 0, sizeof(struct sbdd));

	/* Create raid */
	ret = __sbdd_create_raid(&_raid_capacity, &_raid_sectors);
	if(ret)
	{
		pr_err("creating raid_error=%d\n", ret);
		return ret;
	}

	pr_info("allocating disk\n");

#ifdef BLK_MQ_MODE
	ret = sbdd_alloc_disk(&__sbdd, &__sbdd_blk_mq_ops);
#else
	ret = sbdd_alloc_disk(&__sbdd, NULL);
#endif
	if (ret) {
		pr_warn("disk allocation failed\n");
		return ret;
	}

	/* Configure queue */
	__sbdd.gd->queue->queuedata = &__sbdd;
	blk_queue_max_hw_sectors(__sbdd.gd->queue, _raid_sectors);
#if (BUILT_KERNEL_VERSION < KERNEL_VERSION(5, 14, 0))
	blk_queue_make_request(__sbdd.gd->queue, sbdd_io_make_request);
#endif

	/* Configure gendisk */
	__sbdd.gd->private_data = &__sbdd;
	__sbdd.gd->major = __sbdd_major;
	__sbdd.gd->first_minor = 0;
	__sbdd.gd->minors = 1;
	__sbdd.gd->fops = &__sbdd_bdev_ops;
	/* Represents name in /proc/partitions and /sys/block */
	scnprintf(__sbdd.gd->disk_name, DISK_NAME_LEN, SBDD_NAME);
	set_capacity(__sbdd.gd, _raid_capacity);
	
	/*
	Allocating gd does not make it available, add_disk() required.
	After this call, gd methods can be called at any time. Should not be
	called before the driver is fully initialized and ready to process reqs.
	*/
	pr_info("adding disk\n");
#if (BUILT_KERNEL_VERSION > KERNEL_VERSION(5, 14, 0))
	ret = add_disk(__sbdd.gd);
	if(ret)
	{
		pr_err("adding disk error=%d\n", ret);
		return ret;
	}
#else
	add_disk(__sbdd.gd);
#endif

	return 0;
}

static void sbdd_delete(void)
{
	__sbdd_destroy_raid();

	sbdd_free_disk(&__sbdd);

	memset(&__sbdd, 0, sizeof(struct sbdd));

	if (__sbdd_major > 0) {
		pr_info("unregistering blkdev\n");
		unregister_blkdev(__sbdd_major, SBDD_NAME);
		__sbdd_major = 0;
	}
}

/*
Note __init is for the kernel to drop this function after
initialization complete making its memory available for other uses.
There is also __initdata note, same but used for variables.
*/
static int __init sbdd_init(void)
{
	int ret = 0;

	pr_info("###starting initialization...\n");
	ret = sbdd_create();
	if (ret) {
		pr_warn("initialization failed\n");
		sbdd_delete();
	} else {
		pr_info("initialization complete\n");
	}

	return ret;
}

/*
Note __exit is for the compiler to place this code in a special ELF section.
Sometimes such functions are simply discarded (e.g. when module is built
directly into the kernel). There is also __exitdata note.
*/
static void __exit sbdd_exit(void)
{
	pr_info("exiting...\n");
	sbdd_delete();
	pr_info("exiting complete\n");
}

/* Called on module loading. Is mandatory. */
module_init(sbdd_init);

/* Called on module unloading. Unloading module is not allowed without it. */
module_exit(sbdd_exit);

/* Set raid type */
module_param_named(raid_type, __sbdd_raid_type, ulong, S_IRUGO);
/* Set raid type */
module_param_named(raid_config, __sbdd_raid_config, charp, S_IRUGO);

/* Note for the kernel: a free license module. A warning will be outputted without it. */
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Simple Block Device Driver");