#ifndef _SBDD_IOCTL_H_
#define _SBDD_IOCTL_H_

#include <sbdd.h>

int sbdd_ioctl(struct block_device *bdev, fmode_t mode, unsigned int cmd, unsigned long arg);

#endif