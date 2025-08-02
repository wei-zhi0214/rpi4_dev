# Makefile for cross-compiling led_app for Raspberry Pi 4

CC = arm-linux-gnueabihf-gcc
SRC = led_app.c
TARGET = led_app
OUT_DIR = output

$(OUT_DIR)/$(TARGET): $(SRC)
	@mkdir -p $(OUT_DIR)
	$(CC) -static -o $@ $<

all: $(OUT_DIR)/$(TARGET)

clean:
	rm -f $(OUT_DIR)/$(TARGET)

