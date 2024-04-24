#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <open.h>

int sbdd_open(struct block_device *bdev, fmode_t mode)
{
	struct sbdd* _dev = bdev->bd_disk->private_data;

	if (!_dev) {
		pr_err("empty disk private_data\n");
		return -ENXIO;
	}

	pr_info("device was opened\n");

	return 0;
}

void sbdd_release(struct gendisk *disk, fmode_t mode)
{
	struct sbdd* _dev = disk->private_data;

	if (!_dev) {
		pr_err("Invalid disk private_data\n");
		return;
	}

	pr_info("device was closed\n");
}