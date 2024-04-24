#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <kernel_version.h>
#include <io.h>

static sector_t __sbdd_xfer(struct sbdd *dev, struct bio_vec* bvec, sector_t pos, int dir)
{
	void *buff = kmap_atomic(bvec->bv_page) + bvec->bv_offset;
	sector_t len = bvec->bv_len >> SBDD_SECTOR_SHIFT;
	size_t offset;
	size_t nbytes;

	if (pos + len > dev->capacity)
		len = dev->capacity - pos;

	offset = pos << SBDD_SECTOR_SHIFT;
	nbytes = len << SBDD_SECTOR_SHIFT;

	spin_lock(&dev->datalock);

	if (dir)
		memcpy(dev->data + offset, buff, nbytes);
	else
		memcpy(buff, dev->data + offset, nbytes);

	spin_unlock(&dev->datalock);

	pr_debug("pos=%6llu len=%4llu %s\n", pos, len, dir ? "written" : "read");

	kunmap_atomic(buff);
	return len;
}

static void __sbdd_xfer_bio(struct sbdd* dev, struct bio *bio)
{
	struct bvec_iter iter;
	struct bio_vec bvec;
	
    int dir = bio_data_dir(bio);
	sector_t pos = bio->bi_iter.bi_sector;

	bio_for_each_segment(bvec, bio, iter)
		pos += __sbdd_xfer(dev, &bvec, pos, dir);
}

void __sbdd_xfer_rq(struct sbdd* dev, struct request *rq)
{
	struct req_iterator iter;
	struct bio_vec bvec;
    
    int dir = rq_data_dir(rq);
	sector_t pos = blk_rq_pos(rq);

	rq_for_each_segment(bvec, rq, iter)
		pos += __sbdd_xfer(dev, &bvec, pos, dir);
}

blk_qc_t sbdd_submit_bio(struct bio *bio)
{
#if (BUILT_KERNEL_VERSION > KERNEL_VERSION(5, 14, 0))
	struct sbdd* _dev = bio->bi_bdev->bd_disk->private_data;
#else
	struct sbdd* _dev = bio->bi_disk->private_data;
#endif

	if (atomic_read(&_dev->deleting)) {
		bio_io_error(bio);
		return BLK_STS_IOERR;
	}

	if (!atomic_inc_not_zero(&_dev->refs_cnt)) {
		bio_io_error(bio);
		return BLK_STS_IOERR;
	}

	__sbdd_xfer_bio(_dev, bio);

	bio_endio(bio);

	if (atomic_dec_and_test(&_dev->refs_cnt))
		wake_up(&_dev->exitwait);

	return BLK_STS_OK;
}

blk_status_t sbdd_queue_rq(struct blk_mq_hw_ctx *hctx, struct blk_mq_queue_data const *bd)
{
    struct sbdd *_dev = bd->rq->q->queuedata;

	if (atomic_read(&_dev->deleting))
		return BLK_STS_IOERR;

	if (!atomic_inc_not_zero(&_dev->refs_cnt))
		return BLK_STS_IOERR;

	blk_mq_start_request(bd->rq);

	__sbdd_xfer_rq(_dev, bd->rq);
    
	blk_mq_end_request(bd->rq, BLK_STS_OK);

	if (atomic_dec_and_test(&_dev->refs_cnt))
		wake_up(&_dev->exitwait);

	return BLK_STS_OK;
}

blk_qc_t sbdd_make_request(struct request_queue *q, struct bio *bio)
{
	return sbdd_submit_bio(bio);
}