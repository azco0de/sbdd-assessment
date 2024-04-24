# Simple Block Device Driver
Implementation of Linux Kernel 5.4.X - 6.8.X simple block device.

## Build
- regular:
`$ make build`
- with blk_mq support:
uncomment `ccflags-y += -DBLK_MQ_MODE` in `Kbuild`
- with requests debug info:
uncomment `CFLAGS_sbdd.o := -DDEBUG` in `Kbuild`

## Clean
`$ make clean`

## Usage
- build:
`./module.sh build`
- install:
`./module.sh install`
- uninstall:
`./module.sh uninstall`
- load:
`./module.sh load`
- unload:
`./module.sh unload`

## References
- [Linux Device Drivers](https://lwn.net/Kernel/LDD3/)
- [Linux Kernel Development](https://rlove.org)
- [Linux Kernel Teaching](https://linux-kernel-labs.github.io/refs/heads/master/labs/block_device_drivers.html)
- [Linux Kernel Sources](https://github.com/torvalds/linux)
