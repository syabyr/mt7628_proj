default: hello
KERNELDIR ?= /lib/modules/$(shell uname -r)/build
PWD = `pwd`

ifeq ($(KERNELRELEASE),)

.PHONY: clean.
clean:
	rm -rf *.o *.~ *.ko *.mod.c

hello:
	#$(MAKE) -C $(KERNELDIR) M=$(PWD) modules ARCH=mips CROSS_COMPILE=mipsel-openwrt-linux-
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules
else
	obj-m:= hello.o
endif
