#!/bin/sh -e
DEVICE_NAME=sbdd
MODULE_NAME=$DEVICE_NAME.ko
MODULE_DIR=$(pwd)/build
MODULE_PATH=/lib/modules/$(uname -r)/kernel/drivers/block
MODULE_PARAM=capacity_mib=100

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
		modprobe ${DEVICE_NAME} ${MODULE_PARAM} 
		;;
	unload)
		modprobe -r ${DEVICE_NAME}
		;;
	*)
		exit 1
esac
