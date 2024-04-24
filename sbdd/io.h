#ifndef _SBDD_IO_H_
#define _SBDD_IO_H_

#include <sbdd.h>

blk_qc_t sbdd_submit_bio(struct bio *bio);

blk_status_t sbdd_queue_rq(struct blk_mq_hw_ctx *hctx, struct blk_mq_queue_data const *bd);

blk_qc_t sbdd_make_request(struct request_queue *q, struct bio *bio);

#endif