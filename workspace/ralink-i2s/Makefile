default: ralink-i2s
KERNELDIR ?= /lib/modules/$(shell uname -r)/build
PWD = `pwd`

ifeq ($(KERNELRELEASE),)

.PHONY: clean.
clean:
	rm -rf *.o *.~ *.ko *.mod.c modules.order Module.symvers *.mod

ralink-i2s:
	#$(MAKE) -C $(KERNELDIR) M=$(PWD) modules ARCH=mips CROSS_COMPILE=mipsel-openwrt-linux-
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules
else
	obj-m:= ralink-i2s.o
endif
