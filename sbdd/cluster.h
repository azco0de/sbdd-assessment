#ifndef _SBDD_CLUSTER_H_
#define _SBDD_CLUSTER_H_

#include <linux/fs.h>
#include <linux/genhd.h>
#include <linux/blkdev.h>
#include <linux/wait.h>
#include <linux/types.h>
#include <linux/list.h>
#include <linux/spinlock_types.h>
#include <linux/blk-mq.h>

struct sbdd_cluster_disk {
	struct list_head link;
    struct block_device* bdev_raw;
    char name[DISK_NAME_LEN];
};

struct sbdd_cluster {
    void*                   ctx;
    spinlock_t              cluster_disks_lock;
    struct list_head        cluster_disks;
};

int sbdd_cluster_create(struct sbdd_cluster* cluster, void* ctx);
void sbdd_cluster_destroy(struct sbdd_cluster* cluster);

int sbdd_cluster_is_empty(struct sbdd_cluster* cluster);

int sbdd_cluster_add_disk(struct sbdd_cluster* cluster, const char* name);
int sbdd_cluster_rem_disk(struct sbdd_cluster* cluster, const char* name);

#endif