
KDIR ?= /lib/modules/`uname -r`/build

MODULES_DEP = /lib/modules/$(shell uname -r)/modules.dep

# VirtualBot Driver name
SERIALPROV_NAME = serialprov

# VirtualBot device name
SERIALPROV_DEVICE=/dev/$(SERIALPROV_NAME)

# Real Arduino device
# VIRTUALBOT_DEVICE=/dev/ttyACM0

.PHONY: all clean setup_dev_environment modules_install set_debug install modules_install tests

# setup-environment: configures environment for module development
# For Debian systems, start by using 'apt install make binutils'
# 1) It downloads the current system kernel headers 
setup_dev_environment:
	sudo apt install linux-headers-`uname -r` python3-serial make gcc binutils

all: 
	$(MAKE) -C $(KDIR) M=$$PWD modules
	
all-dev:
	$(MAKE) -C $(KDIR) M=$$PWD EXTRA_CFLAGS="-DDEBUG" modules

clean:
	$(MAKE) -C $(KDIR) M=$$PWD clean

modules_install:
	sudo $(MAKE) -C $(KDIR) \
		M=$$PWD \
		modules_install 
	sudo depmod -A

set_debug:
	sudo dmesg -C
	sudo sysctl -w kernel.printk=7
	sudo sysctl kernel.printk
	
check_device_logs:
	sudo dmesg

install:
	sudo modprobe $(SERIALPROV_NAME)

uninstall:
	sudo rm -f /dev/$(SERIALPROV_NAME)
	sudo modprobe -r $(SERIALPROV_NAME)
#	sudo rm -f /lib/modules/`uname -r`/extra/virtualbot.ko
	sudo depmod

tests:
	sudo dmesg -C
	sudo ./tests/tests_emulatedport_driver.py
