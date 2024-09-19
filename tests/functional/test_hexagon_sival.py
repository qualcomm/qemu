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
    GIT_REF = '712b33af9f25568a726caf48d3ccdb5753236e4c'
    ASSET_TARBALL = \
        Asset(f'{REPO}/-/archive/{GIT_REF}/qemu-sival-tests-{GIT_REF}.tar.gz',
              '2e4b15be48e5799f15686d6c47dc063ace8eade27d36d4d1aecc525c24e66db7')

    def check(self):
        fp = self.tempfile
        buf = fp.read().decode('utf-8')
        # Reset the tempfile for the next test
        fp.truncate(0); fp.flush(); fp.seek(0)

        reg_regex = re.compile(".*r01.: ([^,]*),")
        matches = [reg_regex.match(line) for line in buf.split("\n")]
        # Remove non-matching lines
        regvals = [match.group(1) for match in filter(None, matches)]

        # Must have at least one thread with r01 == 0xe0fbeef, and all
        # others be 0xe0fbeef or 0x00000000 (unused thread).
        if not any(filter(lambda v: v == "0xe0f0beef", regvals)):
            raise HexagonCheckError(f'missing 0xe0f0beef, regs: {regvals}')

        unknown_regs = list(filter(lambda v: v not in ("0xe0f0beef", "0x00000000"),
                                   regvals))
        if any(unknown_regs):
            raise HexagonCheckError(f'invalid regs: {unknown_regs}')

    @skipUnless(os.getenv('QEMU_TEST_ALLOW_UNTRUSTED_CODE'), 'untrusted code')
    def test_sival(self):
        file_path = self.ASSET_TARBALL.fetch()
        archive_extract(file_path, self.workdir)
        self.set_machine('SA8775P_CDSP0')
        test_dir = f'{self.workdir}/qemu-sival-tests-{self.GIT_REF}/bins/'
        with tempfile.NamedTemporaryFile() as fp:
            self.tempfile = fp
            self.vm.add_args('-cpu', f'any,dump-json-reg-file={fp.name}')
            result = run_tests(self, test_dir, self.SIVAL_TIMEOUT_SEC, self.check)
            self.assertTrue(result)

if __name__ == '__main__':
    QemuSystemTest.main()
