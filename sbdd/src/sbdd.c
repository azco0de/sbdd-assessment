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
#include <open.h>
#include <ioctl.h>
#include <io.h>

static struct sbdd      __sbdd;
static int              __sbdd_major = 0;
static unsigned long    __sbdd_capacity_mib = 100;


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
	.queue_rq = sbdd_queue_rq,
};
#endif

/*
There are no read or write operations. These operations are performed by
the request() function associated with the request queue of the disk.
*/
static struct block_device_operations const __sbdd_bdev_ops = {
	.owner = THIS_MODULE,
	.open = sbdd_open,
	.release = sbdd_release,
	.ioctl = sbdd_ioctl,
#ifndef BLK_MQ_MODE
#if (BUILT_KERNEL_VERSION > KERNEL_VERSION(5, 8, 0))
	.submit_bio = sbdd_submit_bio,
#endif
#endif
};

static int sbdd_create(void)
{
	int ret = 0;

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

	__sbdd.capacity = (sector_t)__sbdd_capacity_mib * SBDD_MIB_SECTORS;

	pr_info("allocating data\n");
	__sbdd.data = vzalloc(__sbdd.capacity << SBDD_SECTOR_SHIFT);
	if (!__sbdd.data) {
		pr_err("unable to alloc data\n");
		return -ENOMEM;
	}

	spin_lock_init(&__sbdd.datalock);
	init_waitqueue_head(&__sbdd.exitwait);

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

	blk_queue_logical_block_size(__sbdd.gd->queue, SBDD_SECTOR_SIZE);
#if (BUILT_KERNEL_VERSION < KERNEL_VERSION(5, 14, 0))
	blk_queue_make_request(__sbdd.gd->queue, sbdd_make_request);
#endif

	/* Configure gendisk */
	__sbdd.gd->private_data = &__sbdd;

	__sbdd.gd->major = __sbdd_major;
	__sbdd.gd->first_minor = 0;
	__sbdd.gd->minors = 1;
	__sbdd.gd->fops = &__sbdd_bdev_ops;

	/* Represents name in /proc/partitions and /sys/block */
	scnprintf(__sbdd.gd->disk_name, DISK_NAME_LEN, SBDD_NAME);
	set_capacity(__sbdd.gd, __sbdd.capacity);
	atomic_set(&__sbdd.refs_cnt, 1);

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
	}
#else
	add_disk(__sbdd.gd);
#endif

	return ret;
}

static void sbdd_delete(void)
{
	atomic_set(&__sbdd.deleting, 1);
	atomic_dec(&__sbdd.refs_cnt);
	wait_event(__sbdd.exitwait, !atomic_read(&__sbdd.refs_cnt));

	sbdd_free_disk(&__sbdd);

	if (__sbdd.data) {
		pr_info("freeing data\n");
		vfree(__sbdd.data);
	}

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

/* Set desired capacity with insmod */
module_param_named(capacity_mib, __sbdd_capacity_mib, ulong, S_IRUGO);

/* Note for the kernel: a free license module. A warning will be outputted without it. */
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Simple Block Device Driver");