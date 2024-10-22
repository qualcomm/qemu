#!/usr/bin/env python3
#
# Copyright(c) 2024 Qualcomm Innovation Center, Inc. All Rights Reserved.
#
# SPDX-License-Identifier: BSD-3-Clause

import os
from glob import glob
from qemu_test import QemuSystemTest, Asset
from qemu_test import wait_for_console_pattern
from qemu_test.utils import archive_extract


class MiniVMTest(QemuSystemTest):

    timeout = 180
    GUEST_ENTRY = 0xc0000000

    REPO = 'https://artifacts.codelinaro.org/artifactory'
    ASSET_TARBALL = \
        Asset(f'{REPO}/codelinaro-toolchain-for-hexagon/19.1.2/'
                'hexagon_rootfs_2024_Oct_22.tar.xz',
        'a5c2bc8c1dddbe5ef4c375c84f0145ad61d116bb465669fc30690839720e6904')

    def test_minivm(self):
        tarball_path = self.ASSET_TARBALL.fetch()
        contents = ('boot/minivm', 'boot/test_mmu', 'boot/test_processors',
            'boot/test_interrupts',
            )
        for f in contents:
            f = os.path.join('hexagon-unknown-linux-musl-rootfs', f)
            archive_extract(tarball_path, self.workdir, member=f)
        rootfs_path = f'{self.workdir}/hexagon-unknown-linux-musl-rootfs'
        kernel_path = f'{rootfs_path}/boot/minivm'

        self.set_machine('SA8775P_CDSP0')

        for test_bin_path in glob(f'{rootfs_path}/boot/test_*'):
            print(f'# Testing "{os.path.basename(test_bin_path)}"')

            vm = self.get_vm()
            vm.add_args('-kernel', kernel_path,
                        '-device', f'loader,addr={self.GUEST_ENTRY},file={test_bin_path}')
            vm.launch()
            vm.wait()
            self.assertRegex(vm.get_log(), 'PASS')
            self.assertEqual(vm.exitcode(), 0)

if __name__ == '__main__':
    QemuSystemTest.main()
