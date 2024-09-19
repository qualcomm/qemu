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
from qemu_test.utils import archive_extract
from hexagon.utils import run_tests, HexagonCheckError

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

    def check(self):
        if self.vm.exitcode() != 0:
            raise HexagonCheckError(self.vm.get_log() or '')

    def run_tests_for_arch(self, arch_name):
        print(f'# RUNNING {arch_name}')
        try:
            self.set_vm_arg('-M', self.QURT_MACHINES[arch_name])
        except KeyError:
            self.fail(f'UNKNOWN arch {arch_name}')
        test_dir = f'{self.workdir}/qemu-qurt-tests-{self.GIT_REF}/{arch_name}/'
        return run_tests(self, test_dir, self.QURT_TIMEOUT_SEC, self.check)

    @skipUnless(os.getenv('QEMU_TEST_ALLOW_UNTRUSTED_CODE'), 'untrusted code')
    def test_qurt(self):
        file_path = self.ASSET_TARBALL.fetch()
        archive_extract(file_path, self.workdir)
        self.vm.add_args('-m', '4G', '-no-reboot')
        results = [self.run_tests_for_arch(a) for a in self.QURT_MACHINES.keys()]
        self.assertTrue(all(results))

if __name__ == '__main__':
    QemuSystemTest.main()
