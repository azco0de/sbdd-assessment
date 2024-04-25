#ifndef _SBDD_IOCTL_H_
#define _SBDD_IOCTL_H_

#include <sbdd.h>

#define SBDD_CREATE_CLUSTER_IOCTL _IOW( 0xad, 0, char* )

int sbdd_ioctl(struct block_device *bdev, fmode_t mode, unsigned int cmd, unsigned long arg);

#endif