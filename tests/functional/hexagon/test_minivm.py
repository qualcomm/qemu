#!/usr/bin/env python3
#
# Copyright(c) 2024-2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
#
# SPDX-License-Identifier: GPL-2.0-or-later

import os
from os.path import join
from qemu_test import QemuSystemTest, Asset
from qemu_test import wait_for_console_pattern
from unittest import skip

class MiniVMTest(QemuSystemTest):
    '''
    minivm is a Hexagon hypervisor that implements the Hexagon VM
    specification.  These test cases boot minivm and then load test cases
    to the address specified by MiniVMTest.GUEST_ENTRY and
    execute a minvm-guest program to exercise minivm virtualization
    features.
    '''
    timeout = 180
    GUEST_ENTRY = 0xc0000000

    REPO = 'https://artifacts.codelinaro.org/artifactory'
    ASSET_TARBALL = \
        Asset(f'{REPO}/codelinaro-toolchain-for-hexagon/'
               '19.1.5/hexagon_minivm_2024_Dec_15.tar.gz',
        'd7920b5ff14bed5a10b23ada7d4eb927ede08635281f25067e0d5711feee2c2a')

    def test_minivm_mmu(self):
        self.common_hexagon_minivm('test_mmu')

    def test_minivm_interrupts(self):
        self.common_hexagon_minivm('test_interrupts')

    def test_minivm_processors(self):
        self.common_hexagon_minivm('test_processors')

    def common_hexagon_minivm(self, test_case):
        """
        Common code to launch a basic machine with minivm and a guest
        test case.
        """
        self.set_machine('sim')
        self.archive_extract(self.ASSET_TARBALL)
        rootfs_path = f'{self.workdir}/hexagon-unknown-linux-musl-rootfs'
        kernel_path = f'{rootfs_path}/boot/minivm'

        assert(os.path.exists(kernel_path))
        test_bin_path = join(f'{rootfs_path}/boot', test_case)
        vm = self.get_vm()
        vm.add_args('-cpu', 'v73')
        vm.add_args('-kernel', kernel_path, '-device',
              f'loader,addr={hex(self.GUEST_ENTRY)},file={test_bin_path}')
        vm.launch()
        vm.wait()
        self.assertEqual(vm.exitcode(), 0)

if __name__ == '__main__':
    QemuSystemTest.main()
