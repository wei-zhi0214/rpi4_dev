CROSS_COMPILE ?= aarch64-linux-gnu-
CC := $(CROSS_COMPILE)gcc
CFLAGS := -O2 -Wall -static
TARGET := enc_read

all: $(TARGET)

$(TARGET): enc_read.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(TARGET)

