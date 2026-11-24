# SPDX-License-Identifier: GPL-2.0-or-later
#
# Bare metal tests for heterogeneous machine (aarch64 + riscv64 cpus).
#
# Each test is made of one program per cpu, both running at the same time.
# Output of both cpus goes to the shared UART, and is compared with
# <test>.expected (riscv64 cpu waits for aarch64 one, so output is ordered).
#
# Tests:
#   hello: print a message from each cpu
#   irq:   check interrupts (software, timer, shared UART) on each cpu
#
# Requires clang and ld.lld with aarch64 and riscv64 support.
#
# Usage (from build directory):
#   make -f ../tests/heterogeneous/Makefile         # build tests
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

ARCHS := aarch64 riscv64
TEST_NAMES := hello irq

# extra sources, per test and arch
EXTRA_irq_aarch64 := aarch64/vectors.S
EXTRA_irq_riscv64 := riscv64/trap.S

TESTS := $(foreach t,$(TEST_NAMES),$(foreach a,$(ARCHS),$(OUT)/$(t)-$(a).elf))

all: $(TESTS)

define test_rule
$(OUT)/$(1)-$(2).elf: $(SRC_DIR)$(2)/boot.S $(SRC_DIR)$(2)/$(1).c \
                      $(addprefix $(SRC_DIR),$(EXTRA_$(1)_$(2))) \
                      $(SRC_DIR)$(2)/link.ld $(SRC_DIR)heterogeneous.h
	$$(CLANG) $$(CFLAGS_COMMON) $$(CFLAGS_$(2)) -T $(SRC_DIR)$(2)/link.ld \
		-o $$@ $$(filter %.S %.c,$$^)
endef
$(foreach t,$(TEST_NAMES),$(foreach a,$(ARCHS),\
    $(eval $(call test_rule,$(t),$(a)))))

check: $(addprefix check-,$(TEST_NAMES))

check-%: $(OUT)/%-aarch64.elf $(OUT)/%-riscv64.elf
	timeout 30 $(QEMU) -target aarch64,riscv64 -M heterogeneous \
		-display none -monitor none -serial file:$(OUT)/$*.out \
		-semihosting-config enable=on,target=native \
		-device loader,file=$(OUT)/$*-aarch64.elf,cpu-num=0 \
		-device loader,file=$(OUT)/$*-riscv64.elf,cpu-num=1
	tr -d '\r' < $(OUT)/$*.out | diff -u $(SRC_DIR)$*.expected - && \
		echo "PASS: $*"

clean:
	rm -f $(TESTS) $(addprefix $(OUT)/,$(addsuffix .out,$(TEST_NAMES)))

.PHONY: all check clean
