#!/usr/bin/env python3
#
# Copyright(c) 2024 Qualcomm Innovation Center, Inc. All Rights Reserved.
#
# SPDX-License-Identifier: BSD-3-Clause

import os
import re
import tempfile
from qemu_test import QemuSystemTest, Asset
from qemu_test import wait_for_console_pattern
from unittest import skipUnless

class SivalTests(QemuSystemTest):

    SIVAL_TIMEOUT_SEC = 100

    REPO = 'https://gitlab.qualcomm.com/qqvp/testing/qemu-sival-tests'
    GIT_REF = '7a8536822af7ee1dac0fa26e4d675ce90eea7607'
    ASSETS_BASE_URL = f'{REPO}/-/raw/{GIT_REF}/bins/'

    ASSET_TEST_BINS = [
        Asset(f'{ASSETS_BASE_URL}/Q6V73QA_1_0_vtcm_thrasher_4T_i1_0x14680000.elf',
              'b06f517aa0467678e21d41c3d423481181ac59378b88d609ddf87df20dd5e5ae'),
        Asset(f'{ASSETS_BASE_URL}/Q6V73QA_1_0_vtcm_thrasher_4T_i1_0x80000000.elf',
              '7135dce8d485405cfa3023f860767d4e5c4f83f0f183fa2c590ad4bada5e8d71'),
    ]

    def assert_final_regdump(self, buf):
        reg_regex = re.compile(".*r01.: ([^,]*),")
        matches = [reg_regex.match(line) for line in buf.split("\n")]
        # Remove non-matching lines
        regvals = [match.group(1) for match in filter(None, matches)]

        # Must have at least one thread with r01 == 0xe0fbeef, and all
        # others be 0xe0fbeef or 0x00000000 (unused thread).
        self.assertTrue(any(filter(lambda v: v == "0xe0f0beef", regvals)))
        self.assertFalse(any(filter(lambda v: v not in ("0xe0f0beef", "0x00000000"),
                                    regvals)))

    @skipUnless(os.getenv('QEMU_TEST_ALLOW_UNTRUSTED_CODE'), 'untrusted code')
    def test_sival(self):
        self.set_machine('SA8775P_CDSP0')
        for bin_asset in self.ASSET_TEST_BINS:
            test_bin_path = bin_asset.fetch()
            print(f'# Testing {os.path.basename(bin_asset.url)}')

            vm = self.get_vm()
            with tempfile.NamedTemporaryFile() as fp:
                vm.add_args('-cpu', f'any,dump-json-reg-file={fp.name}',
                            '-kernel', test_bin_path)
                vm.launch()
                vm.wait(timeout=self.SIVAL_TIMEOUT_SEC)
                self.assert_final_regdump(fp.read().decode('utf-8'))

if __name__ == '__main__':
    QemuSystemTest.main()
