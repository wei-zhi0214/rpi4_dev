obj-m += hello.o

ARCH := arm64
CROSS_COMPILE := aarch64-linux-gnu-

PWD := $(shell pwd)
KDIR := /home/william/rpi4_dev/linux

all:
	$(MAKE) -C $(KDIR) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) M=$(PWD) clean

