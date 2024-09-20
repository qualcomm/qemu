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

    GUEST_ENTRY = 0xc0000000

    REPO = 'https://gitlab.qualcomm.com/qqvp/qemu/hexagonMVM'
    ASSET_TARBALL = \
        Asset(f'{REPO}/-/jobs/3664197/artifacts/raw/artifacts.tar.gz',
              '27d7090744fd69e35bd6b891d484d8d48aa5277c1719be7c584b674b204f00ed')

    def test_minivm(self):
        tarball_path = self.ASSET_TARBALL.fetch()
        archive_extract(tarball_path, self.workdir)
        kernel_path = f'{self.workdir}/artifacts/minivm'

        self.set_machine('SA8775P_CDSP0')

        for test_bin_path in glob(f'{self.workdir}/artifacts/tests_bin/*'):
            print(f'# Testing {os.path.basename(test_bin_path)}')

            vm = self.get_vm()
            vm.add_args('-kernel', kernel_path,
                        '-device', f'loader,addr={self.GUEST_ENTRY},file={test_bin_path}')
            vm.launch()
            vm.wait()
            self.assertRegex(vm.get_log(), 'PASS')
            self.assertEqual(vm.exitcode(), 0)

if __name__ == '__main__':
    QemuSystemTest.main()
