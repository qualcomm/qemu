#!/usr/bin/env python3
#
# Copyright(c) 2024-2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
#
# SPDX-License-Identifier: GPL-2.0-or-later

import os
import re
import tempfile
from qemu_test import QemuSystemTest, Asset
from qemu_test import wait_for_console_pattern
from unittest import skipUnless
from hexagon.utils import run_tests, HexagonCheckError

class QURTTests(QemuSystemTest):

    QURT_TIMEOUT_SEC = 300

    REPO = 'https://gitlab.qualcomm.com/qqvp/testing/qemu-qurt-tests'
    GIT_REF = '670c85bef763e330c1f0805837e654c05b6cd714'
    ASSET_TARBALL = \
        Asset(f'{REPO}/-/archive/{GIT_REF}/qemu-qurt-tests-{GIT_REF}.tar.gz',
              '3144d153ec2f01751fd7cbc0df376d50a5531dee555c131a06f0d1de95bdff03')

    QURT_MACHINES = {
        "nspv79NA_1": "V79NA_1",
        "nspv81QA_1": "V81QA_1",
    }

    QURT_CPUS = {
        "nspv81QA_1": "v81",
    }

    def check(self, _):
        if self.vm.exitcode() != 0:
            raise HexagonCheckError(self.vm.get_log() or '')

    def run_tests_for_arch(self, arch_name):
        print(f'# RUNNING {arch_name}')
        try:
            self.set_vm_arg('-M', self.QURT_MACHINES[arch_name])
            if arch_name in self.QURT_CPUS:
                self.set_vm_arg('-cpu', self.QURT_CPUS[arch_name])
        except KeyError:
            self.fail(f'UNKNOWN arch {arch_name}')
        test_dir = f'{self.workdir}/qemu-qurt-tests-{self.GIT_REF}/{arch_name}/'
        return run_tests(self, test_dir, self.QURT_TIMEOUT_SEC, self.check)

    @skipUnless(os.getenv('QEMU_TEST_ALLOW_UNTRUSTED_CODE'), 'untrusted code')
    def test_qurt(self):
        self.archive_extract(self.ASSET_TARBALL)
        self.vm.add_args('-m', '4G', '-no-reboot',
                         '-global', 'qct-qtimer.freq-scale=100')
        results = [self.run_tests_for_arch(a) for a in self.QURT_MACHINES.keys()]
        self.assertTrue(all(results))

if __name__ == '__main__':
    QemuSystemTest.main()
