#!/usr/bin/env python3
#
# Copyright(c) 2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
#
# SPDX-License-Identifier: GPL-2.0-or-later

import os
from glob import glob
from qemu_test import LinuxKernelTest, Asset
from qemu_test import exec_command_and_wait_for_pattern


class HexagonLinuxDevsTest(LinuxKernelTest):
    timeout = 120
    GUEST_ENTRY = 0xa0000000

    REPO = 'https://gitlab.qualcomm.com/qqvp/testing/qemu-linux-tests'
    GIT_REF = 'v0.1.0'
    ASSET_TARBALL = \
        Asset(f'{REPO}/-/archive/{GIT_REF}/qemu-linux-tests-{GIT_REF}.tar.gz',
              '598738ec39e718023c314bc22a8c185f2e72ebd1377f444e7e2aa0e0ea174e42')

    def test_linux_devs(self):
        self.set_machine('virt')
        self.require_netdev('user')

        kernel_path = self.archive_extract(self.ASSET_TARBALL,
            member=f'qemu-linux-tests-{self.GIT_REF}/vmlinux.bin')
        booter_path = self.archive_extract(self.ASSET_TARBALL,
            member=f'qemu-linux-tests-{self.GIT_REF}/loadlinux')
        disk_path = self.archive_extract(self.ASSET_TARBALL,
            member=f'qemu-linux-tests-{self.GIT_REF}/disk.qcow2')
        self.vm.set_console()
        self.vm.add_args(
            '-kernel', booter_path,
            '-device', f'loader,addr=0x{self.GUEST_ENTRY:08x},file={kernel_path}',
            '-m', '4G',
            '-accel', 'tcg,thread=multi',
            '-drive', f'if=none,file={disk_path},id=hd0',
            '-device', 'virtio-blk-device,drive=hd0',
            '-netdev', 'type=user,id=net0,hostfwd=tcp::10022-:22',
            '-device', 'virtio-net-device,netdev=net0',
        )
        self.vm.launch()

        self.wait_for_console_pattern(
            "clocksource: Switched to clocksource HVM timer")
        self.wait_for_console_pattern("bash-4.3#")

        # Test that virtio-net and virtio-blk devices are functional:
        exec_command_and_wait_for_pattern(self, "ip addr",
                                          "inet 10.0.2.15")
        exec_command_and_wait_for_pattern(self, "cat /mnt/persist/test.txt",
                                          "Welcome to hexagon linux!")


if __name__ == '__main__':
    LinuxKernelTest.main()
