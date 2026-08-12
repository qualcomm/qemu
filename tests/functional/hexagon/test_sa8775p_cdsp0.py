#!/usr/bin/env python3
#
# Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
#
# SPDX-License-Identifier: GPL-2.0-or-later

import os
from qemu_test import QemuSystemTest, Asset
from qemu_test import wait_for_console_pattern
from unittest import skipUnless

class SA8775PCdsp0BootTest(QemuSystemTest):
    ASSET_CDSP0_MBN = Asset(
        'https://gitlab.com/kernel-firmware/linux-firmware/-/raw/'
        '599764611a8ac213c6aa6dad17c941c2f46b53cb/qcom/sa8775p/cdsp0.mbn',
        '756ef6fa531454e593bd078c0bfa240fb97ff8740dd35fd91231074214d41660')

    BOOT_STRING = ('DSP Image Creation Date: ENGG time:'
                   'Q6_BUILD_TS_Tue_Feb_10_05:37:44_PST_2026_'
                   'lemans.cdsp0.prodQ')

    @skipUnless(os.getenv('QEMU_TEST_ALLOW_UNTRUSTED_CODE'), 'untrusted code')
    def test_cdsp0_boot(self):
        kernel_path = self.ASSET_CDSP0_MBN.fetch()
        self.set_machine('SA8775P_CDSP0')
        self.vm.set_console(semihosting=True)
        self.vm.add_args('-kernel', kernel_path)
        self.vm.add_args('-cpu', 'any,virtual-platform-mode=true,coproc=')
        self.vm.launch()
        wait_for_console_pattern(self, self.BOOT_STRING)
        self.vm.kill()

if __name__ == '__main__':
    QemuSystemTest.main()
