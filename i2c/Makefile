ifeq ($(DEBUG),y)
  DEBFLAGS = -O -g -DENABLE_DEBUG # "-O" is needed to expand inlines
else
  DEBFLAGS = -O2
endif

EXTRA_CFLAGS  += $(DEBFLAGS)

ifneq ($(KERNELRELEASE),)
 	ccflags-y := -DENABLE_DEBUG
	obj-m := pt100.o
else
	KERNELDIR ?= /lib/modules/$(shell uname -r)/build
	PWD := $(shell pwd)
default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules
endif

clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) clean
