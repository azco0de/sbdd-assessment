#include <linux/fs.h>
#include <linux/genhd.h>
#include <linux/blkdev.h>
#include <linux/wait.h>
#include <linux/types.h>
#include <linux/spinlock_types.h>
#include <linux/blk-mq.h>

#define SBDD_SECTOR_SHIFT      9
#define SBDD_SECTOR_SIZE       (1 << SBDD_SECTOR_SHIFT)
#define SBDD_MIB_SECTORS       (1 << (20 - SBDD_SECTOR_SHIFT))
#define SBDD_NAME              "sbdd"

struct sbdd {
	wait_queue_head_t       exitwait;
	spinlock_t              datalock;
	atomic_t                deleting;
	atomic_t                refs_cnt;
	sector_t                capacity;
	u8                      *data;
	struct gendisk          *gd;
    struct blk_mq_tag_set   *tag_set;
};