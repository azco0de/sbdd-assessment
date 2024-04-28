# Simple Block Device Driver
Implementation of Linux Kernel 5.4.X - 6.8.X simple block device
This implementation in particular was tested on LK 5.15.105

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

## Configuration
Configuration of the sbdd gets with help of module parameters and has next view:
`raid_type=T raid_config="oprtion0=value0;....optionN=valueN"`
where:
- raid_type : 0 - raid0 (at that moment only simple raid0 is supported)
- raid_config : specific config for raid of raid_type in form of opt=val separated by ';'

raid config for raid0:
`raid_config="stripe=S;disks=D1,D2"`
- stripe : size of raid0 stripe in 1024-bytes-units
- disks : disks to build raid

example of the raid0 module parameters:
`raid_type=0 raid_config="stripe=1;disks=/dev/sbdev1,/dev/sbdev2"`

## References
- [Linux Device Drivers](https://lwn.net/Kernel/LDD3/)
- [Linux Kernel Development](https://rlove.org)
- [Linux Kernel Teaching](https://linux-kernel-labs.github.io/refs/heads/master/labs/block_device_drivers.html)
- [Linux Kernel Sources](https://github.com/torvalds/linux)
