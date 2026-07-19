#!/usr/bin/env python3
#
# Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
#
# SPDX-License-Identifier: GPL-2.0-or-later

import os
from unittest import skip
from qemu_test import QemuSystemTest, Asset
from qemu_test.cmd import wait_for_console_pattern


class ArchTestsUart(QemuSystemTest):
    """
    Hexagon architecture verification tests

    These are bare-metal tests from hexagon-arch-tests that exercise
    system registers, interrupts, TLB/MMU, exceptions, threads, cache,
    HVX context, user mode, guest mode, L2VIC, and timers.

    Tests output results via UART.
    """
    timeout = 180

    ASSET_TARBALL = Asset(
        "https://github.com/qualcomm/qemu-hexagon-testing/releases/"
        "download/v0.2.5/arch_tests_uart.tar.gz",
        "edb4f37b877a3a72a0e10920477458a43b40045d34398fee8cb763fefd342f4f",
    )

    def run_uart_test(self, test_name: str,
                      machine: str = "virt") -> None:
        """
        Run an arch test binary and verify PASS via UART console output.

        These binaries write their results to a PL011 at 0x10000000, which is
        part of the virt machine's device window.  The DSP machines map DDR
        there instead, so virt is the machine that can carry them.  '-bios
        none' suppresses the default loadlinux firmware so -kernel boots
        directly.
        """
        self.set_machine(machine)
        self.archive_extract(self.ASSET_TARBALL)
        target_bin = os.path.join(self.workdir,
                                  'arch_tests_uart_package',
                                  'bin', test_name)
        self.vm.set_console()
        self.set_vm_arg("-display", "none")
        self.set_vm_arg("-bios", "none")
        self.set_vm_arg("-kernel", target_bin)
        self.vm.launch()
        wait_for_console_pattern(self, "PASS")

    @skip("dczeroa is a no-op in system mode to avoid code corruption")
    def test_cache(self) -> None:
        """Tests data cache operations: dczeroa, dccleana, dcinva,
        dccleaninva, and verifying cache line behavior.
        """
        self.run_uart_test("test_cache")

    def test_exceptions(self) -> None:
        """Tests exception delivery for trap instructions, privilege
        violations, and verifies SSR cause codes and ELR values.
        """
        self.run_uart_test("test_exceptions")

    def test_guest_mode(self) -> None:
        """Tests guest mode entry/exit via CCR configuration, verifying
        GSR fields, GELR, and guest event vector table dispatch.
        """
        self.run_uart_test("test_guest_mode")

    def test_hvx_context(self) -> None:
        """Tests HVX vector register context save/restore across
        multiple hardware threads with independent HVX state.
        """
        self.run_uart_test("test_hvx_context")

    def test_interrupts(self) -> None:
        """Tests software interrupt delivery, dispatch to registered
        handlers, masking via IMASK, and interrupt clear operations.
        """
        self.run_uart_test("test_interrupts")

    def test_int_steering(self) -> None:
        """Tests interrupt steering via priority-based routing to
        specific threads using STID priority and iassignw.
        """
        self.run_uart_test("test_int_steering")

    @skip("test binary has a defect")
    def test_isr_stress(self) -> None:
        """Tests high-volume interrupt stress with multiple threads
        handling rapid SWI delivery and concurrent interrupt processing.
        """
        self.run_uart_test("test_isr_stress")

    def test_l2vic(self) -> None:
        """Tests L2VIC interrupt controller: enable/disable/set/clear
        registers, software interrupt triggering, and status readback.
        """
        self.run_uart_test("test_l2vic")

    def test_pmu(self) -> None:
        """Tests performance monitoring unit: pcycle counter reads,
        cycle counting enable/disable via SYSCFG.
        """
        self.run_uart_test("test_pmu")

    def test_sys_regs(self) -> None:
        """Tests system register read/write: SSR, EVB, SYSCFG, IMASK,
        VID, MODECTL, cfgbase, FRAMEKEY, SGP0/1, STID, DIAG."""
        self.run_uart_test("test_sys_regs")

    def test_threads(self) -> None:
        """Tests multi-threaded execution: start/stop/wait/resume
        instructions, thread entry dispatch, and MODECTL state."""
        self.run_uart_test("test_threads")

    def test_timer(self) -> None:
        """Tests system timer: QTimer version register, TIMERLO/TIMERHI
        monotonic reads, pcycle-based timing.
        """
        self.run_uart_test("test_timer")

    def test_tlb_mmu(self) -> None:
        """Tests TLB/MMU operations: tlbw/tlbr/tlbp, page size mappings,
        ASID matching, permission enforcement, and TLB invalidation.
        """
        self.run_uart_test("test_tlb_mmu")

    def test_user_mode(self) -> None:
        """Tests user mode transitions: SSR.UM set/clear via rte,
        privilege exception on supervisor instructions in user mode.
        """
        self.run_uart_test("test_user_mode")


if __name__ == "__main__":
    QemuSystemTest.main()
