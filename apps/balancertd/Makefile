# ================== Toolchain ==================
CC := aarch64-linux-gnu-gcc

# ================== Flags ==================
CFLAGS  := -O2 -Wall -Wextra -pthread
LDFLAGS := -pthread -lm -static

# ================== Source ==================
SRC := $(wildcard *.c)
OUT_DIR := output
TARGETS := $(patsubst %.c,$(OUT_DIR)/%,$(SRC))

# ================== Build ==================
all: $(TARGETS)

$(OUT_DIR)/%: %.c
	@mkdir -p $(OUT_DIR)
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -rf $(OUT_DIR)

