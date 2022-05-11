default: ralink-pcm
KERNELDIR ?= /lib/modules/$(shell uname -r)/build
PWD = `pwd`

ifeq ($(KERNELRELEASE),)

.PHONY: clean.
clean:
	rm -rf *.o *.~ *.ko *.mod.c modules.order Module.symvers *.mod

ralink-pcm:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules
else
	obj-m:= ralink-pcm.o
endif
