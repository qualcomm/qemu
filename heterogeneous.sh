#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Build qemu-system and bare metal tests, then run them on heterogeneous
# machine (aarch64 + riscv64 cpus sharing a UART).
# Extra arguments are passed to QEMU (e.g. -accel tcg,thread=single).

set -euo pipefail

src=$(readlink -f "$(dirname "$0")")
build=$src/build
out=$build/heterogeneous-tests

if [ ! -f "$build/build.ninja" ]; then
    echo "build directory is not configured, run ./build.sh first"
    exit 1
fi

ninja -C "$build" qemu-system

mkdir -p "$out"
make -s -C "$out" -f "$src/tests/heterogeneous/Makefile"

set -x
timeout 30 "$build/qemu-system" \
    -target aarch64,riscv64 -M heterogeneous \
    -display none -monitor none -serial stdio \
    -semihosting-config enable=on,target=native \
    -device loader,file="$out/hello-aarch64.elf" \
    -device loader,file="$out/hello-riscv64.elf" \
    "$@"
