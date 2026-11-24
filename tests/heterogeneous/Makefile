# SPDX-License-Identifier: GPL-2.0-or-later
#
# Bare metal tests for heterogeneous machine (aarch64 + riscv64 cpus).
#
# Requires clang and ld.lld with aarch64 and riscv64 support.
#
# Usage (from build directory):
#   make -f ../tests/heterogeneous/Makefile         # build both tests
#   make -f ../tests/heterogeneous/Makefile check   # build and run them
#
# Variables: QEMU (default: build/qemu-system), OUT (output dir, default: .)

SRC_DIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))

CLANG ?= clang
QEMU ?= $(SRC_DIR)../../build/qemu-system
OUT ?= .

CFLAGS_COMMON := -ffreestanding -nostdlib -fno-pic -O2 -g -Wall -Werror \
                 -I$(SRC_DIR) -fuse-ld=lld -static -Wl,--build-id=none
CFLAGS_aarch64 := --target=aarch64-none-elf -mgeneral-regs-only
CFLAGS_riscv64 := --target=riscv64-unknown-elf -march=rv64imac -mabi=lp64 \
                  -mcmodel=medany -mno-relax

TESTS := $(OUT)/hello-aarch64.elf $(OUT)/hello-riscv64.elf

all: $(TESTS)

$(OUT)/hello-%.elf: $(SRC_DIR)%/boot.S $(SRC_DIR)%/hello.c $(SRC_DIR)%/link.ld $(SRC_DIR)heterogeneous.h
	$(CLANG) $(CFLAGS_COMMON) $(CFLAGS_$*) -T $(SRC_DIR)$*/link.ld \
		-o $@ $(SRC_DIR)$*/boot.S $(SRC_DIR)$*/hello.c

# Output of both cpus goes to the same UART, test checks it contains both
# messages, in order (riscv64 cpu waits for aarch64 one).
check: $(TESTS)
	timeout 30 $(QEMU) -target aarch64,riscv64 -M heterogeneous \
		-display none -monitor none -serial file:$(OUT)/hello.out \
		-semihosting-config enable=on,target=native \
		-device loader,file=$(OUT)/hello-aarch64.elf \
		-device loader,file=$(OUT)/hello-riscv64.elf
	cat $(OUT)/hello.out
	printf 'Hello world from aarch64\r\nHello world from riscv64\r\n' | \
		cmp - $(OUT)/hello.out && echo PASS

clean:
	rm -f $(TESTS) $(OUT)/hello.out

.PHONY: all check clean
