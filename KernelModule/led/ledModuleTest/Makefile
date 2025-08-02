obj-m += led.o

CROSS_COMPILE := aarch64-linux-gnu-
ARCH := arm64
KDIR := /home/william/rpi4_dev/linux
PWD := $(shell pwd)

all:
	$(MAKE) -C $(KDIR) M=$(PWD) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

