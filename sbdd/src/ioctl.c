#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/hdreg.h>
#include <ioctl.h>

static inline int ___ioctl_hdio_getgeo(struct sbdd *dev, unsigned long arg)
{
	struct hd_geometry geo = {0};

	geo.start = 0;
	if (dev->capacity > 63) 
    {
		sector_t quotient;

		geo.sectors = 63;
		quotient = (dev->capacity + (63 - 1)) / 63;

		if (quotient > 255) 
        {
			geo.heads = 255;
			geo.cylinders = (unsigned short)
				((quotient + (255 - 1)) / 255);
		} 
        else 
        {
			geo.heads = (unsigned char)quotient;
			geo.cylinders = 1;
		}
	} 
    else 
    {
		geo.sectors = (unsigned char)dev->capacity;
		geo.cylinders = 1;
		geo.heads = 1;
	}

	if (copy_to_user((void *)arg, &geo, sizeof(geo)))
		return -EINVAL;

	return 0;
}

int sbdd_ioctl(struct block_device *bdev, fmode_t mode, unsigned int cmd, unsigned long arg)
{
	struct sbdd *_dev = bdev->bd_disk->private_data;

	pr_debug("contol command [0x%x] received\n", cmd);

	switch (cmd) 
    {
	case HDIO_GETGEO:
		return ___ioctl_hdio_getgeo(_dev, arg);
	default:
		return -ENOTTY;
	}
}