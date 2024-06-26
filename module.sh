#!/bin/sh -e
DEVICE_NAME=sbdd
MODULE_NAME=$DEVICE_NAME.ko
MODULE_DIR=$(pwd)/build
MODULE_PATH=/lib/modules/$(uname -r)/kernel/drivers/block
MODULE_RAID_TYPE_PARAM=raid_type=0
MODULE_RAID_0_CFG_PARAM=raid_config="stripe=1;disks=/dev/sbdev1,/dev/sbdev2"

case "$1" in
	build)
		make
		;;
	install)
		mkdir -p $MODULE_PATH
		cp $MODULE_DIR/$MODULE_NAME $MODULE_PATH
		depmod
		;;
	uninstall)
		rm -f $MODULE_PATH/$MODULE_NAME
		depmod
		;;
	load)
		modprobe ${DEVICE_NAME} ${MODULE_RAID_TYPE_PARAM} ${MODULE_RAID_0_CFG_PARAM}
		;;
	unload)
		modprobe -r ${DEVICE_NAME}
		;;
	*)
		exit 1
esac
