default: mt7620a-pcm
KERNELDIR ?= /lib/modules/$(shell uname -r)/build
PWD = `pwd`

ifeq ($(KERNELRELEASE),)

.PHONY: clean.
clean:
	rm -rf *.o *.~ *.ko *.mod.c *.symvers *.order *.mod .*

mt7620a-pcm:
	#$(MAKE) -C $(KERNELDIR) M=$(PWD) modules ARCH=mips CROSS_COMPILE=mipsel-openwrt-linux-
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules
else
	obj-m:= mt7620a-pcm.o
endif
