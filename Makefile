######## Documentation (1/2)

# Kernel build system requires makefiles that do not look like traditional ones.
# The said system hadles all this stuff. See for more:
# https://www.kernel.org/doc/Documentation/kbuild/
# (makefiles.txt and modules.txt should be the main ones)

# This line states that there is one module to be built from obj file <name>.o.
# The resulting module will be named <name>.ko after being built:
# obj-m := sbdd.o

# The command to build a module is the following:
# $ make -C <path_to_kernel_source_tree> M=`pwd` modules
# In the '<path_...>' it finds kernel's top-level makefile.
# 'M=...' option sets the path to module's files.
# 'modules' is the target of make. It refers to the list of modules found
# in the obj-m variable.


######## Documentation (2/2)

# There is an idiom on creating makefiles for kernel developers.
# If KERNELRELEASE is defined, we've been invoked from the kernel build system
# (we get here the 2nd time when 'modules' target is processed):
# ifneq ($(KERNELRELEASE),)
# 	# It is actually a Kbuild part of makefile (should be placed in different file)
# 	# and will only be processed by kbuild system, not make.
# 	obj-m := sbdd.o
# # Otherwise we were called directly from the command line and should invoke kbuild.
# else
# default:
# 	$(MAKE) -C /lib/modules/$(shell uname -r)/build M=$(shell pwd) modules
# endif


######## Makefile

DEVICE_NAME		?= sbdd
MODULE_NAME		?= $(DEVICE_NAME).ko
MAKEFILE_PATH	?= $(PWD)/build/Makefile
BUILD_DIR 		?= $(PWD)/build
KERNEL_DIR		?= /lib/modules/$(shell uname -r)/build
KERNEL_VER		?= $(shell uname -r | cut -f1 -d'-')


split-string-dot = $(word $2,$(subst ., ,$1))

k_ver_major := $(call split-string-dot,$(KERNEL_VER:%=%),1)
k_ver_minor := $(call split-string-dot,$(KERNEL_VER),2)
k_ver_patch := $(call split-string-dot,$(KERNEL_VER),3)

build: $(MAKEFILE_PATH)
	make -C $(KERNEL_DIR) M=$(BUILD_DIR) src=$(PWD) modules

$(KERNEL_VER):
	touch "$(PWD)/sbdd/kernel_version.h"
	echo "#include <linux/version.h> \n\n"\
		"#define BUILT_KERNEL_VERSION KERNEL_VERSION($(k_ver_major),$(k_ver_minor),$(k_ver_patch))" > $(PWD)/sbdd/kernel_version.h

$(BUILD_DIR): $(KERNEL_VER)
	mkdir -p "$@"

$(MAKEFILE_PATH): $(BUILD_DIR)
	touch "$@"

clean:
	make -C $(KERNEL_DIR) M=$(BUILD_DIR) src=$(PWD) clean
