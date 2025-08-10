CC = arm-linux-gnueabihf-gcc
SRC := $(wildcard *.c)
OUT_DIR := output
TARGETS := $(patsubst %.c,$(OUT_DIR)/%,$(SRC))

# 針對每個 .c 編譯成 output/下的獨立執行檔
$(OUT_DIR)/%: %.c
	@mkdir -p $(OUT_DIR)
	$(CC) -static -o $@ $<

all: $(TARGETS)

clean:
	rm -f $(TARGETS)

