#!/usr/bin/env python3
#
# Copyright(c) 2024 Qualcomm Innovation Center, Inc. All Rights Reserved.
#
# SPDX-License-Identifier: BSD-3-Clause

import os
from qemu_test import QemuSystemTest, Asset
from qemu_test import wait_for_console_pattern

class MiniVMTest(QemuSystemTest):

    GUEST_ENTRY = 0xc0000000

    REPO = 'https://gitlab.qualcomm.com/qqvp/qemu/hexagonMVM'
    ASSETS_BASE_URL = f'{REPO}/-/jobs/3637047/artifacts/raw/'

    ASSET_KERNEL = Asset(f'{ASSETS_BASE_URL}/minivm',
              'c055a891b9d776ec80d425f0de347b0c6f58591bc6cf2d77663e3bd3af4ec6b8')

    ASSET_TEST_BINS = [
        Asset(f'{ASSETS_BASE_URL}/tests_bin/first',
              '2cff31059c3ad3f1c280945b276c5da8fd351b56a2986e039da30097394cc9af'),
        Asset(f'{ASSETS_BASE_URL}/tests_bin/test_mmu',
              '16b4c0c9c013fbad23f1c068a919d6ece1ad691bfe02a3fe71511613bc79ca26'),
        Asset(f'{ASSETS_BASE_URL}/tests_bin/test_interrupts',
              '7700ac70df4e18012eebf01ce2b47d7db6232a5ff7ade7aeebccedbd44deab1f'),
        Asset(f'{ASSETS_BASE_URL}/tests_bin/test_processors',
              '12fd619b3a0d45014264a6f55fd257f251f3f2a42928db408f1d60cb34b458a6'),
    ]

    def test_minivm(self):
        kernel_path = self.ASSET_KERNEL.fetch()
        self.set_machine('SA8775P_CDSP0')

        for bin_asset in self.ASSET_TEST_BINS:
            test_bin_path = bin_asset.fetch()
            print(f'# Testing {os.path.basename(bin_asset.url)}')

            vm = self.get_vm()
            vm.add_args('-kernel', kernel_path,
                        '-device', f'loader,addr={self.GUEST_ENTRY},file={test_bin_path}')
            vm.launch()
            vm.wait()
            self.assertRegex(vm.get_log(), 'PASS')
            self.assertEqual(vm.exitcode(), 0)

if __name__ == '__main__':
    QemuSystemTest.main()
