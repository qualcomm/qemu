#!/usr/bin/env python3
#
# Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
#
# SPDX-License-Identifier: GPL-2.0-or-later

import os
from qemu_test import QemuSystemTest, Asset
from unittest import skip

class SDKTests(QemuSystemTest):
    REPO = 'https://gitlab.qualcomm.com/qqvp/testing/qemu-qurt-tests'
    GIT_REF = 'bc94e62a20370dfe405220898fc2a64127fd64a6'
    ASSET_TARBALL = \
        Asset(f'{REPO}/-/archive/{GIT_REF}/qemu-qurt-tests-{GIT_REF}.tar.gz',
          'd13ec572e1ef237b2a7c8e311e5f55ee8cd8d006ecb414b6da3fd795b4179832')

    def test_qprintf_v81(self):
        self.run_qprintf_case('V81DGB_1')

    @skip("V66G_1024 default machine does not support V81 QuRT")
    def test_qprintf_default(self):
        self.run_qprintf_case()

    def test_qprintf_v81_bios(self):
        """Test using -bios instead of -kernel for V81DGB_1 machine."""
        self.run_qprintf_case('V81DGB_1', use_bios=True)

    def test_qprintf_default_bios(self):
        """Test using -bios instead of -kernel for default machine."""
        self.run_qprintf_case(use_bios=True)

    def run_qprintf_case(self, machine=None, use_bios=False):
        """
        qprintf example dumps the contents of scalar and HVX registers via
        semihosting, and this particular version of the test case is built with
        QuRT V81, expecting support for the new TLB extension.

        Args:
            machine: Optional machine type to use (e.g., 'V81DGB_1')
            use_bios: If True, use -bios to load the bootloader ELF
        """
        self.archive_extract(self.ASSET_TARBALL)
        if machine:
            self.set_machine(machine)

        sdk_test_path = \
            f'{self.workdir}/qemu-qurt-tests-{self.GIT_REF}/sdk/V81QA_1'
        kernel = os.path.join(sdk_test_path, 'runelf.pbn')
        vm = self.get_vm()
        if use_bios:
            vm.add_args('-m', '4G', '-bios', kernel,
                '-kernel', kernel, '-append',
                './run_main_on_hexagon_sim -- ./libqprintf_example_q.so')
        else:
            vm.add_args('-m', '4G', '-kernel', kernel, '-append',
                './run_main_on_hexagon_sim -- ./libqprintf_example_q.so')
        vm.launch()
        # TODO: Check semihosting output for "PASSED" indication.
        #   the 'wait_for_console_pattern()' function expects to
        #   use the system console.  For now, Hexagon semihosting
        #   prints directly to stdout/stderr.  When we leverage the
        #   target independent semihosting, we can take advantage of
        #   the console redirection and then 'wait_for_console_pattern()'
        #   should work.
        vm.wait()
        self.assertEqual(vm.exitcode(), 0, "QEMU failed")

if __name__ == '__main__':
    QemuSystemTest.main()
