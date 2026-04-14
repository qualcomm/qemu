#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later

# set -x
set -euo pipefail

if [ $# -lt 2 ]; then
    echo "usage: rme_stack_data_dir qemu-system-aarch64"
    exit 1
fi

t=$1; shift
qemu=$*; shift
exec $qemu \
    -display none -vga none -machine virt -cpu max,x-rme=on,pauth-impdef=on \
    -m 2G -M virt,acpi=off,virtualization=on,secure=on,gic-version=3 \
    -bios $t/out/bin/flash.bin \
    -kernel $t/out/bin/Image \
    -drive format=raw,if=none,file=$t/out-br/images/rootfs.ext4,id=hd0,readonly=on \
    -device virtio-blk-pci,drive=hd0 \
    -device virtio-9p-device,fsdev=shr0,mount_tag=shr0 \
    -fsdev local,security_model=none,path=$t,id=shr0 \
    -device virtio-net-pci,netdev=net0 -netdev user,id=net0 \
    -append 'root=/dev/vda' -serial stdio
# fix issue by adding nokaslr to kernel command line
