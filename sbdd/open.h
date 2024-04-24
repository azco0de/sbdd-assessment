#ifndef _SBDD_OPEN_H_
#define _SBDD_OPEN_H_

#include <sbdd.h>

int sbdd_open(struct block_device *bdev, fmode_t mode);
void sbdd_release(struct gendisk *disk, fmode_t mode);

#endif