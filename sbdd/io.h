#ifndef _SBDD_IO_H_
#define _SBDD_IO_H_


#include <linux/fs.h>
#include <linux/genhd.h>
#include <linux/blkdev.h>
#include <linux/wait.h>
#include <linux/types.h>
#include <linux/list.h>
#include <linux/spinlock_types.h>
#include <linux/blk-mq.h>

typedef blk_qc_t (*process_bio_t) (struct bio *bio);

struct sbdd_io {
    void*                   ctx;
	wait_queue_head_t       events;	
	process_bio_t           process_bio;
	struct task_struct*     io_thread;
    atomic_t 				is_io_thread_active;
	atomic_t 				is_io_active;
	spinlock_t              bio_list_lock;
	struct bio_list 		bio_list;
};

int sbdd_io_create(struct sbdd_io* io, process_bio_t process_bio, void* ctx);
void sbdd_io_destroy(struct sbdd_io* io);

int sbdd_io_start(struct sbdd_io* io);
void sbdd_io_stop(struct sbdd_io* io);

int sbdd_io_is_active(struct sbdd_io* io);
int sbdd_io_is_empty(struct sbdd_io* io);

blk_qc_t sbdd_io_submit_bio(struct bio *bio);

blk_status_t sbdd_io_queue_rq(struct blk_mq_hw_ctx *hctx, struct blk_mq_queue_data const *bd);

blk_qc_t sbdd_io_make_request(struct request_queue *q, struct bio *bio);

#endif