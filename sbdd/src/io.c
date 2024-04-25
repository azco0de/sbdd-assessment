#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <kernel_version.h>
#include <linux/kthread.h>
#include <sbdd.h>
#include <io.h>

static int __io_io_routine(void* data)
{
    struct bio* _bio = NULL;

    struct sbdd_io* _io = data;

    set_user_nice(current, -20);

    while (!kthread_should_stop())
    {
        wait_event_interruptible(_io->events, kthread_should_stop() || !bio_list_empty(&_io->bio_list));

        spin_lock_irq(&_io->bio_list_lock);

        if (bio_list_empty(&_io->bio_list))
        {
            spin_unlock_irq(&_io->bio_list_lock);
            continue;
        }

        _bio = bio_list_pop(&_io->bio_list);

        spin_unlock_irq(&_io->bio_list_lock);

        _io->process_bio(_bio);
    }

    return 0;
}

static int __sbdd_io_add_bio(struct sbdd_io* io, struct bio* bio)
{
    if(sbdd_io_is_empty(io))
    {
        pr_err("sbdd_io_add_bio:: io is empty");

        bio_io_error(bio);

        return -EINVAL;
    }
    
    if(!sbdd_io_is_active(io))
    {
        pr_err("sbdd_io_add_bio:: io is not active");

        bio_io_error(bio);

        return -EINVAL;
    }

    spin_lock_irq(&io->bio_list_lock);

    bio_list_add(&io->bio_list, bio);

    wake_up(&io->events);
    
    spin_unlock_irq(&io->bio_list_lock);

    return 0;
}

static int __sbdd_xfer_bio(struct sbdd* dev, struct bio *bio)
{
	return __sbdd_io_add_bio(&dev->io, bio);
}

static int __sbdd_xfer_rq(struct sbdd* dev, struct request *req)
{
    int         _ret = 0;
    struct bio* _bio = NULL;
    
	__rq_for_each_bio(_bio, req)
    {
		_ret = __sbdd_xfer_bio(dev, _bio);
        if(_ret)
            return _ret;
	}

    return 0;
}

blk_qc_t sbdd_io_submit_bio(struct bio *bio)
{
    int _ret = 0;
#if (BUILT_KERNEL_VERSION > KERNEL_VERSION(5, 14, 0))
	struct sbdd* _dev = bio->bi_bdev->bd_disk->private_data;
#else
	struct sbdd* _dev = bio->bi_disk->private_data;
#endif

    if(!sbdd_io_is_active(&_dev->io))
    {
        pr_err("sbdd_io_submit_bio:: io is not active");
        bio_io_error(bio);
        return BLK_STS_IOERR;
    }

	_ret = __sbdd_xfer_bio(_dev, bio);
    if(_ret)
    {
        pr_err("sbdd_io_submit_bio:: xfer_bio error:%d \n", _ret);
        bio_io_error(bio);
		return BLK_STS_IOERR;
    }

	bio_endio(bio);

	return BLK_STS_OK;
}

blk_status_t sbdd_io_queue_rq(struct blk_mq_hw_ctx *hctx, struct blk_mq_queue_data const *bd)
{
    struct sbdd *_dev = bd->rq->q->queuedata;

    struct request *_req = bd->rq;

    if(!sbdd_io_is_active(&_dev->io))
    {
        pr_err("sbdd_io_queue_rq:: io is not active");
        return BLK_STS_IOERR;
    }

	blk_mq_start_request(_req);

	__sbdd_xfer_rq(_dev, _req);

	blk_mq_end_request(_req, BLK_STS_OK);

	return BLK_STS_OK;
}

blk_qc_t sbdd_io_make_request(struct request_queue *q, struct bio *bio)
{
	return sbdd_io_submit_bio(bio);
}

int sbdd_io_create(struct sbdd_io* io, process_bio_t process_bio, void* ctx)
{

    bio_list_init(&io->bio_list);

    spin_lock_init(&io->bio_list_lock);

    init_waitqueue_head(&io->events);

    io->process_bio = process_bio;
    io->ctx = ctx;

    return 0;
}

void sbdd_io_destroy(struct sbdd_io* io)
{
    struct bio* _bio = NULL;

    /* clearing bio list */
    spin_lock_irq(&io->bio_list_lock);

    pr_info("sbdd_io_destroy:: bio list size= %d \n", bio_list_size(&io->bio_list));

    while(!bio_list_empty(&io->bio_list))
    {
        _bio = bio_list_pop(&io->bio_list);
        bio_io_error(_bio);
    }

    spin_unlock_irq(&io->bio_list_lock);
}

int sbdd_io_start(struct sbdd_io* io)
{
    io->io_thread = kthread_create(__io_io_routine, NULL, "io_bio_thread");
    if (IS_ERR(io->io_thread))
    {
        pr_err("sbdd_io_init:: cannot create io thread: %ld \n", PTR_ERR(io->io_thread));
        return PTR_ERR(io->io_thread);
    }

    atomic_set(&io->is_io_thread_active, 1);

    wake_up_process(io->io_thread);

    return 0;
}

void sbdd_io_stop(struct sbdd_io* io)
{
    int _ret = 0;

    if(atomic_dec_if_positive(&io->is_io_thread_active))
    {
        pr_info("sbdd_io_destroy:: stopping thread \n");

        _ret = kthread_stop(io->io_thread);
        if(_ret)
        {
            pr_err("sbdd_io_destroy:: stopping thread error:%d\n", _ret);
        }
    }
}

int sbdd_io_is_active(struct sbdd_io* io)
{
    return atomic_read(&io->is_io_thread_active);
}

int sbdd_io_is_empty(struct sbdd_io* io)
{
    int _is_empty = 1;

    spin_lock_irq(&io->bio_list_lock);

    _is_empty = bio_list_empty(&io->bio_list);

    spin_unlock_irq(&io->bio_list_lock);

    return _is_empty;
}