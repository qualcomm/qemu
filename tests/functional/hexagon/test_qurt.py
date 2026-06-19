#!/usr/bin/env python3
#
# Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
#
# SPDX-License-Identifier: GPL-2.0-or-later

import os
from qemu_test import QemuSystemTest, Asset
from unittest import skipUnless
from hexagon.utils import run_tests, HexagonCheckError

class QURTTests(QemuSystemTest):

    QURT_TIMEOUT_SEC = 300

    REPO = 'https://gitlab.qualcomm.com/qqvp/testing/qemu-qurt-tests'
    GIT_REF = '79ab6880bd4eb893439af73331355c3dc2a241b4'
    ASSET_TARBALL = \
        Asset(f'{REPO}/-/archive/{GIT_REF}/qemu-qurt-tests-{GIT_REF}.tar.gz',
              '96672ff657464afd7cd3b7755c832f2332b642d9aae338d6edda044d0bae602a')

    QURT_MACHINES = {
        "nspv81QA_1": "V81QA_1",
    }

    QURT_CPUS = {
        "nspv81QA_1": "v81",
    }

    # Tests that hang or fail on hex-next due to missing features
    EXTRA_SKIP = [
        "mp_err_hndlr_bootimg.pbn",
        "mp_err_hndlr_reaper_bootimg.pbn",
        "qurt_dm_suspend_t3_bootimg.pbn",
        "qurt_dm_suspend_t4_bootimg.pbn",
        "qurt_hmx_t10_bootimg.pbn",
        "qurt_hmx_t11_bootimg.pbn",
        "qurt_hvx_t9_bootimg.pbn",
        "qurt_int_l2vic_config_check_bootimg.pbn",
        "qurt_isr_t4_bootimg.pbn",
        "qurt_isr_t5_bootimg.pbn",
        "qurt_isr_t6_bootimg.pbn",
        "qurt_mailbox_t1_bootimg.pbn",
        "qurt_mailbox_t3_bootimg.pbn",
        "qurt_mlog_srm_bootimg.pbn",
        "qurt_mp_mq_1_bootimg.pbn",
        "qurt_mp_mq_2_bootimg.pbn",
        "qurt_mp_mq_3_bootimg.pbn",
        "qurt_mp_mq_4_bootimg.pbn",
        "qurt_mp_mq_bootimg.pbn",
        "qurt_mp_mq_unsigned_bootimg.pbn",
        "qurt_mprotect_t1_bootimg.pbn",
        "qurt_pimutex_timed_t2_bootimg.pbn",
        "qurt_safe_copy_bootimg.pbn",
        "qurt_trap_mini_unused_bootimg.pbn",
        "qurt_usr_suspend_t1_bootimg.pbn",
        "qurt_usr_suspend_t2_bootimg.pbn",
        "qurt_usr_suspend_t3_bootimg.pbn",
    ]

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
        return run_tests(self, test_dir, self.QURT_TIMEOUT_SEC, self.check,
                         extra_skip=self.EXTRA_SKIP)

    @skipUnless(os.getenv('QEMU_TEST_ALLOW_UNTRUSTED_CODE'), 'untrusted code')
    def test_qurt(self):
        self.archive_extract(self.ASSET_TARBALL)
        self.vm.add_args('-m', '4G', '-no-reboot')
        results = [self.run_tests_for_arch(a) for a in self.QURT_MACHINES]
        self.assertTrue(all(results))

if __name__ == '__main__':
    QemuSystemTest.main()
