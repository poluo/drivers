#KERNELDIR = /home/poluo/linux-2.6.22.6
KERNELDIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)
default:
	make -C $(KERNELDIR) M=$(PWD) modules

clean:
	make -C $(KERNELDIR) M=$(PWD) clean
ioctl-objs := main.o fops.o
obj-m := ioctl.o 
