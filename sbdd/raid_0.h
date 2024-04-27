#ifndef _SBDD_RAID_0_H_
#define _SBDD_RAID_0_H_

#include <linux/fs.h>
#include <linux/genhd.h>
#include <linux/blkdev.h>
#include <linux/wait.h>
#include <linux/types.h>
#include <linux/spinlock_types.h>
#include <linux/blk-mq.h>

#include <raid_0_cfg.h>

struct sbdd_raid_0_disk {
    struct block_device* bdev_raw;
    char name[DISK_NAME_LEN];
};
typedef struct sbdd_raid_0_disk sbdd_raid_0_disk_t;

struct sbdd_raid_0 {
    void*                   ctx;
    sbdd_raid_0_config_t    config;
    spinlock_t              disks_lock;
    sbdd_raid_0_disk_t**    disks;
};

int sbdd_raid_0_create(struct sbdd_raid_0* raid_0, char* cfg, void* ctx);
void sbdd_raid_0_destroy(struct sbdd_raid_0* raid_0);
blk_qc_t sbdd_raid_0_process_bio(struct bio* bio);

#endif