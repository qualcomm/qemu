#!/usr/bin/env python3
#
# Copyright(c) 2024 Qualcomm Innovation Center, Inc. All Rights Reserved.
#
# SPDX-License-Identifier: BSD-3-Clause

import os
import re
import tempfile
import glob
from qemu_test import QemuSystemTest, Asset
from qemu_test import wait_for_console_pattern
from unittest import skipUnless
from qemu_test.utils import archive_extract

class QURTTests(QemuSystemTest):

    QURT_TIMEOUT_SEC = 300

    REPO = 'https://gitlab.qualcomm.com/qqvp/testing/qemu-qurt-tests'
    GIT_REF = '9cd4b3283920c52111032252e4ead0ee8c657a2d'
    ASSET_TARBALL = \
        Asset(f'{REPO}/-/archive/{GIT_REF}/qemu-qurt-tests-{GIT_REF}.tar.gz',
              '8ba5191ba01df3603831aeead163d90206837c13788a35480bb7e0160e353f04')

    QURT_MACHINES = {
        "nspv68": "V68N_1024",
        "nspv79NA_1": "V79NA_1",
    }

    def get_testdir(self, arch_name):
        return f'{self.workdir}/qemu-qurt-tests-{self.GIT_REF}/{arch_name}/'

    def list_test_cases(self, arch_name):
        return glob.glob(f'{self.get_testdir(arch_name)}/*.pbn')

    def read_skip_file(self, arch_name):
        skip = set()
        with open(f'{self.get_testdir(arch_name)}/SKIP') as f:
            for line in f.readlines():
                line = line.strip()
                if not line.startswith("#"):
                    skip.add(line)
        return skip

    def run_tests_for_arch(self, arch_name):
        print(f'# RUNNING {arch_name}')
        try:
            self.set_vm_arg('-M', self.QURT_MACHINES[arch_name])
        except KeyError:
            self.fail(f'UNKNOWN arch {arch_name}')

        skip = self.read_skip_file(arch_name)
        for test_bin_path in self.list_test_cases(arch_name):
            test_name = os.path.basename(test_bin_path)
            if test_name in skip:
                print(f'#   SKIP {test_name}')
                continue
            print(f'#   Testing {test_name}')
            self.vm.launch()
            self.vm.wait(timeout=self.QURT_TIMEOUT_SEC)
            if self.vm.exitcode() != 0:
                err_msg = '\n----------\n' + (self.vm.get_log() or '') + '\n----------\n'
                err_msg += f'FAILED: {test_name}: exit code {vm.exitcode()}'
                self.fail(err_msg)

    @skipUnless(os.getenv('QEMU_TEST_ALLOW_UNTRUSTED_CODE'), 'untrusted code')
    def test_qurt(self):
        file_path = self.ASSET_TARBALL.fetch()
        archive_extract(file_path, self.workdir)
        self.vm.add_args('-m', '4G', '-no-reboot')
        self.run_tests_for_arch("nspv68")
        self.run_tests_for_arch("nspv79NA_1")

if __name__ == '__main__':
    QemuSystemTest.main()
