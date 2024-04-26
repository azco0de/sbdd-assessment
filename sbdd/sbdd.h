#ifndef _SBDD_DEVICE_H_
#define _SBDD_DEVICE_H_

#include <linux/fs.h>
#include <linux/genhd.h>
#include <linux/blkdev.h>
#include <linux/wait.h>
#include <linux/types.h>
#include <linux/spinlock_types.h>
#include <linux/blk-mq.h>

#include <raid_0.h>
#include <io.h>

#define SBDD_SECTOR_SHIFT      9
#define SBDD_SECTOR_SIZE       (1 << SBDD_SECTOR_SHIFT)
#define SBDD_MIB_SECTORS       (1 << (20 - SBDD_SECTOR_SHIFT))
#define SBDD_NAME              "sbdd"

struct sbdd {
	atomic_t                refs_cnt;
	sector_t 				capacity;
	struct sbdd_raid_0		raid_0;
	struct sbdd_io 			io;
	struct gendisk          *gd;
    struct blk_mq_tag_set   *tag_set;

};

#endif