#!/usr/bin/env python3
#
# Copyright(c) 2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
#
# SPDX-License-Identifier: GPL-2.0-or-later

import os
from glob import glob
from qemu_test import LinuxKernelTest, Asset
from qemu_test import exec_command_and_wait_for_pattern
from unittest import skipUnless
import logging

class HexagonLinuxDevsTest(LinuxKernelTest):
    GUEST_ENTRY = 0xa0000000

    REPO = 'https://gitlab.qualcomm.com/qqvp/testing/qemu-linux-tests'
    GIT_REF = 'buildroot-v0.4'
    ASSET_TARBALL = \
        Asset(f'{REPO}/-/archive/{GIT_REF}/qemu-linux-tests-{GIT_REF}.tar.gz',
              'fc4b79fe5bdf07bffefbf6d75e80febe27ea1d498971e33ff726e977a5410a89')

    @skipUnless(os.getenv('QEMU_TEST_ALLOW_UNTRUSTED_CODE'), 'untrusted code')
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

        # Workaround: guest limitation -- linux kernel in use doesn't
        # consume the generated fdt.  So when we have more devices
        # available, we have to push extra ones in so that the devices
        # below appear at the expected address.
        self.vm.add_args(
            '-netdev', 'type=user,id=net1,hostfwd=tcp::10023-:23',
            '-device', 'virtio-net-device,netdev=net1',
            '-netdev', 'type=user,id=net2,hostfwd=tcp::10024-:24',
            '-device', 'virtio-net-device,netdev=net2',
            '-netdev', 'type=user,id=net3,hostfwd=tcp::10025-:25',
            '-device', 'virtio-net-device,netdev=net3',
            '-netdev', 'type=user,id=net4,hostfwd=tcp::10026-:26',
            '-device', 'virtio-net-device,netdev=net4',
            '-netdev', 'type=user,id=net5,hostfwd=tcp::10027-:27',
            '-device', 'virtio-net-device,netdev=net5',
            '-netdev', 'type=user,id=net6,hostfwd=tcp::10028-:28',
            '-device', 'virtio-net-device,netdev=net6',
            )
        self.vm.add_args(
            '-kernel', booter_path,
            '-device',
                f'loader,addr=0x{self.GUEST_ENTRY:08x},file={kernel_path}',
            '-m', '4G',
            '-accel', 'tcg,thread=multi',
            '-global', 'qct-qtimer.freq-scale=100',
            '-drive', f'if=none,file={disk_path},id=hd0',
            '-device', 'virtio-blk-device,drive=hd0',
            '-netdev', 'type=user,id=net0,hostfwd=tcp::10022-:22',
            '-device', 'virtio-net-device,netdev=net0',
        )
        self.vm.launch()

        self.wait_for_console_pattern(
            "clocksource: Switched to clocksource HVM timer")
        self.wait_for_console_pattern("bash-5.2#")

        # Small sanity check
        exec_command_and_wait_for_pattern(self, "ls", "bin")

        # Test that virtio-net and virtio-blk devices are functional:
        exec_command_and_wait_for_pattern(self, "ip addr",
                                          "inet 10.0.2.15")
        exec_command_and_wait_for_pattern(self, "cat /mnt/persist/test.txt",
                                          "Welcome to hexagon linux!")


if __name__ == '__main__':
    if os.getenv("HEXAGON_TEST_DEBUG"):
        logging.basicConfig(level=logging.DEBUG)
    LinuxKernelTest.main()
