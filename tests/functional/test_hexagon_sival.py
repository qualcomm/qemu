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

class SivalTests(QemuSystemTest):

    SIVAL_TIMEOUT_SEC = 100

    REPO = 'https://gitlab.qualcomm.com/qqvp/testing/qemu-sival-tests'
    GIT_REF = '4bac1e4a88b0592df3dbe48d0b7810a8abeff7df'
    ASSET_TARBALL = \
        Asset(f'{REPO}/-/archive/{GIT_REF}/qemu-sival-tests-{GIT_REF}.tar.gz',
              '9a8e9f093104b24ea64ab862b6ad597ec98efe65646c450902e1dcc04d47c5c8')

    SIVAL_MACHINES = {
        "V73": "V73NA_1024",
        "nordau_v1_diag_suite": "V81QA_1",
    }

    def check(self, test_name):
        fp = self.tempfile
        buf = fp.read().decode('utf-8')
        # Reset the tempfile for the next test
        fp.truncate(0); fp.flush(); fp.seek(0)

        reg_regex = re.compile(".*r01.: ([^,]*),")
        matches = [reg_regex.match(line) for line in buf.split("\n")]
        # Remove non-matching lines
        regvals = [match.group(1) for match in filter(None, matches)]
        err = None

        # Must have at least one thread with r01 == 0xe0fbeef, and all
        # others be 0xe0fbeef or 0x00000000 (unused thread).
        if not any([v for v in regvals if v == "0xe0f0beef"]):
            err = HexagonCheckError(f'missing 0xe0f0beef, regs: {regvals}')
        else:
            unknown_regs = [v for v in regvals if v not in ("0xe0f0beef", "0x00000000")]
            if any(unknown_regs):
                err = HexagonCheckError(f'invalid regs: {unknown_regs}')

        if "_fail_" in test_name:
            if err is None:
                raise HexagonCheckError(f'Expected failure, but test succeeded.')
        elif err is not None:
            raise err

    def run_tests_for_arch(self, arch_name):
        print(f'# RUNNING {arch_name}')
        try:
            self.set_vm_arg('-M', self.SIVAL_MACHINES[arch_name])
        except KeyError:
            self.fail(f'UNKNOWN arch {arch_name}')
        test_dir = f'{self.workdir}/qemu-sival-tests-{self.GIT_REF}/{arch_name}/'
        return run_tests(self, test_dir, self.SIVAL_TIMEOUT_SEC, self.check)

    @skipUnless(os.getenv('QEMU_TEST_ALLOW_UNTRUSTED_CODE'), 'untrusted code')
    def test_sival(self):
        file_path = self.ASSET_TARBALL.fetch()
        archive_extract(file_path, self.workdir)
        with tempfile.NamedTemporaryFile() as fp:
            self.tempfile = fp
            self.vm.add_args('-cpu', f'any,dump-json-reg-file={fp.name}')
            results = [self.run_tests_for_arch(a) for a in self.SIVAL_MACHINES.keys()]
            self.assertTrue(all(results))

if __name__ == '__main__':
    QemuSystemTest.main()
