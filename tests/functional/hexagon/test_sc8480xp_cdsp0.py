#!/usr/bin/env python3
#
# Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
#
# SPDX-License-Identifier: GPL-2.0-or-later

import os
from qemu_test import QemuSystemTest, Asset
from qemu_test import wait_for_console_pattern
from unittest import skipUnless

class SC8480XP_Cdsp0BootTest(QemuSystemTest):
    ASSET_CDSP0_MBN = Asset(
        'https://gitlab.com/kernel-firmware/linux-firmware/-/raw/'
        'dd571d3fe15f472f2cb23a243b4105f122710545/qcom/glymur/cdsp.mbn',
        '7b22f33ae273d3dfc5f741a967831e7d4fd79ad141e6a1ecee17509eac207109')

    ASSET_CDSP0_DTB = Asset(
        'https://gitlab.com/kernel-firmware/linux-firmware/-/raw/'
        'dd571d3fe15f472f2cb23a243b4105f122710545/qcom/glymur/cdsp_dtb.mbn',
        '0ec3e4030de1ad3d5f561238481aee82f31de052eaa08a2737f7f5faba561de7')

    BOOT_STRING = ('DSP Image Creation Date: '
                   'ENGG time:Q6_BUILD_TS_Thu_May_14_06:32:39_PST_2026_glymur.cdsp.prodQ')

    @skipUnless(os.getenv('QEMU_TEST_ALLOW_UNTRUSTED_CODE'), 'untrusted code')
    def test_cdsp0_boot(self):
        kernel_path = self.ASSET_CDSP0_MBN.fetch()
        dtb_path = self.ASSET_CDSP0_DTB.fetch()
        self.set_machine('SC8480XP_NSP0')
        self.vm.set_console(semihosting=True)
        self.vm.add_args('-kernel', kernel_path)
        self.vm.add_args('-device', 'loader,addr=0x91900000,file=' + dtb_path)
        self.vm.launch()
        wait_for_console_pattern(self, self.BOOT_STRING)
        self.vm.kill()

if __name__ == '__main__':
    QemuSystemTest.main()
