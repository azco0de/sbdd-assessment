#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <kernel_version.h>
#include <disk.h>

int sbdd_alloc_disk(struct sbdd* device, const struct blk_mq_ops* mqops)
{
  	int _ret = 0;

    if(mqops != NULL)
    {
        pr_info("allocating tag_set\n");
        
        device->tag_set = kzalloc(sizeof(struct blk_mq_tag_set), GFP_KERNEL);
        if (!device->tag_set) {
            pr_err("unable to alloc tag_set\n");
            return -ENOMEM;
        }

        /* Number of hardware dispatch queues */
        device->tag_set->nr_hw_queues = 1;
        /* Depth of hardware dispatch queues */
        device->tag_set->queue_depth = 128;
        device->tag_set->numa_node = NUMA_NO_NODE;
        device->tag_set->ops = mqops;

        _ret = blk_mq_alloc_tag_set(device->tag_set);
        if (_ret) {
            pr_err("call blk_mq_alloc_tag_set() failed with %d\n", _ret);
            return _ret;
        }

#if (BUILT_KERNEL_VERSION < KERNEL_VERSION(5, 14, 0))

        /* Creates both the hardware and the software queues and initializes structs */
        pr_info("initing mq queue\n");
        device->gd->queue = blk_mq_init_queue(device->tag_set);
        if (IS_ERR(device->gd->queue)) {
            _ret = (int)PTR_ERR(device->gd->queue);
            pr_err("call blk_mq_init_queue() failed witn %d\n", _ret);
            device->gd->queue = NULL;
            return _ret;
        }
#endif

#if (BUILT_KERNEL_VERSION > KERNEL_VERSION(5, 14, 0))
        pr_info("allocating mq disk\n"); 
        device->gd = blk_mq_alloc_disk(device->tag_set, NULL);
        if (!device->gd) {
            pr_err("call alloc_disk() failed\n");
            return -EINVAL;
        }
#endif
    }
    else
    {
#if (BUILT_KERNEL_VERSION < KERNEL_VERSION(5, 14, 0))
        pr_info("allocating bio disk\n"); 
        device->gd = alloc_disk(1);
        if (!device->gd) {
            pr_err("call alloc_disk() failed\n");
            return -EINVAL;
        }

        pr_info("allocating queue\n");

        device->gd->queue = blk_alloc_queue(GFP_KERNEL);
        if (!device->gd->queue) {
            pr_err("call blk_alloc_queue() failed\n");
            return -EINVAL;
        }

        blk_queue_make_request(device->gd->queue, sbdd_io_make_request);
#endif

#if (BUILT_KERNEL_VERSION > KERNEL_VERSION(5, 14, 0))
        pr_info("allocating bio disk\n"); 
        device->gd = blk_alloc_disk(1);
        if (!device->gd) {
            pr_err("call blk_alloc_disk() failed\n");
            return -EINVAL;
        }
#endif
    }

	return _ret;  
}

void sbdd_free_disk(struct sbdd* device)
{
    /* gd will be removed only after the last reference put */
	if (device->gd) {
		pr_info("deleting disk\n");
		del_gendisk(device->gd);
	}

	if (device->gd->queue) {
		pr_info("cleaning up queue\n");
		blk_cleanup_queue(device->gd->queue);
	}

	if (device->gd)
		put_disk(device->gd);


	if (device->tag_set && device->tag_set->tags) {
		pr_info("freeing tag_set\n");
		blk_mq_free_tag_set(device->tag_set);
	}

	if (device->tag_set)
		kfree(device->tag_set);
}