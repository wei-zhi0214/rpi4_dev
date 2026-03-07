obj-m += quad_encoder.o
KDIR ?= /home/william/rpi4_dev/linux
PWD  := $(shell pwd)

all:
	$(MAKE) -C $(KDIR) M=$(PWD) ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

