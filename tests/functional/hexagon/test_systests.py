#!/usr/bin/env python3
#
# Copyright(c) 2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
#
# SPDX-License-Identifier: GPL-2.0-or-later

import os
import re
import time
import unittest

from qemu_test import QemuSystemTest, Asset, wait_for_console_pattern


_TARBALL_BIN_PATH = os.path.join(
    "systests_standalone_package",
    "StandaloneSysTests_6.4.0.2_v68",
    "bin",
)


class SysTestsStandaloneTests(QemuSystemTest):
    SYSTEST_TIMEOUT_SEC = 30

    ASSET_TARBALL = Asset(
        "https://github.com/qualcomm/qemu-hexagon-testing/releases/download/v0.2.9/systests_standalone.tar.gz",
        "db961ea3fcc389b478b3d5c2f3bac60bec87e7f3d7efef5e58a9ae8ee78d0e40",
    )

    def setUp(self):
        super().setUp()
        self.archive_extract(self.ASSET_TARBALL)
        self.bin_dir = os.path.join(self.workdir, _TARBALL_BIN_PATH)
        self._orig_cwd = os.getcwd()
        os.chdir(self.workdir)
        self.addCleanup(os.chdir, self._orig_cwd)

        # Smoke-test fixtures (consumed by tests below).
        # fopen: needs a file named dummy.so containing "valid\n"
        with open(os.path.join(self.workdir, "dummy.so"), "w") as f:
            f.write("valid\n")

        # ftrunc: needs _testfile_ftrunc containing "valid\n" (6 bytes)
        ftrunc_path = os.path.join(self.workdir, "_testfile_ftrunc")
        with open(ftrunc_path, "w") as f:
            f.write("valid\n")

        # dirent: needs _dirent_testdir/ with fileA and fileB
        dirent_dir = os.path.join(self.workdir, "_dirent_testdir")
        os.makedirs(dirent_dir, exist_ok=True)
        open(os.path.join(dirent_dir, "fileA"), "w").close()
        open(os.path.join(dirent_dir, "fileB"), "w").close()

        # access: needs _testfile_access containing "valid\n"
        with open(os.path.join(self.workdir, "_testfile_access"), "w") as f:
            f.write("valid\n")

        # semihost: needs _semihost_dir/ with fileA and fileB
        semihost_dir = os.path.join(self.workdir, "_semihost_dir")
        os.makedirs(semihost_dir, exist_ok=True)
        open(os.path.join(semihost_dir, "fileA"), "w").close()
        open(os.path.join(semihost_dir, "fileB"), "w").close()

    def tearDown(self):
        if hasattr(self, "_orig_cwd"):
            os.chdir(self._orig_cwd)
        super().tearDown()

    def binary(self, name):
        """Return the full path to binary *name* inside bin_dir."""
        return os.path.join(self.bin_dir, name)

    def run_exit_zero(self, binary_name, *extra_args, machine="sim"):
        """Launch *binary_name* and assert it exits with code 0.

        :param binary_name: name of the binary inside bin_dir.
        :param extra_args: optional pairs of (flag, value) strings passed
            to set_vm_arg(), e.g. ('-append', 'myarg').
        :param machine: QEMU machine type (default "sim").
        """
        self.set_machine(machine)
        self.set_vm_arg("-display", "none")
        self.set_vm_arg("-kernel", self.binary(binary_name))
        for flag, value in zip(extra_args[::2], extra_args[1::2]):
            self.set_vm_arg(flag, value)
        self.vm.launch()
        self.vm.wait(timeout=60.0)
        self.assertEqual(self.vm.exitcode(), 0,
                         f"Test {binary_name} exited with "
                         f"code {self.vm.exitcode()}, expected 0")

    def run_console_pattern(self, binary_name, pattern, *extra_args,
                            machine="sim"):
        """Launch *binary_name* and wait for *pattern* on the semihosting console.

        :param binary_name: name of the binary inside bin_dir.
        :param pattern: string pattern to wait for via wait_for_console_pattern.
        :param extra_args: optional pairs of (flag, value) strings passed
            to set_vm_arg(), e.g. ('-append', 'myarg').
        :param machine: QEMU machine type (default "sim").
        """
        self.set_machine(machine)
        self.set_vm_arg("-display", "none")
        self.set_vm_arg("-kernel", self.binary(binary_name))
        for flag, value in zip(extra_args[::2], extra_args[1::2]):
            self.set_vm_arg(flag, value)
        self.vm.set_console(semihosting=True)
        self.vm.launch()
        try:
            wait_for_console_pattern(self, pattern)
        finally:
            self.vm.kill()

    def run_expect_exit_nonzero(self, binary_name, log_pattern, *extra_args,
                                machine="sim"):
        """Launch *binary_name* and assert it crashes with exit 255 and a
        log line matching *log_pattern* (regex).

        Exit 255 is what the standalone CRT's unhandled-exception handler
        produces; *log_pattern* identifies which exception fired.
        """
        self.set_machine(machine)
        self.set_vm_arg("-display", "none")
        self.set_vm_arg("-kernel", self.binary(binary_name))
        self.set_vm_arg("-d", "guest_errors")
        for flag, value in zip(extra_args[::2], extra_args[1::2]):
            self.set_vm_arg(flag, value)
        self.vm.launch()
        self.vm.wait(timeout=60.0)
        exit_code = self.vm.exitcode()
        self.assertEqual(exit_code, 255,
                         f"{binary_name} exited with {exit_code}, expected 255")
        log = self.vm.get_log() or ""
        self.assertTrue(
            re.search(log_pattern, log),
            f"Pattern {log_pattern!r} not found in QEMU log for "
            f"{binary_name}.\nLog:\n{log}"
        )

    def _wait_for_log_pattern(self, pattern, timeout=30.0):
        """Poll vm._qemu_log_path until *pattern* appears or timeout expires.

        The "Double k0lock/tlblock at PC" and "Starting thread N" messages are
        emitted by QEMU itself to its own stdout/stderr (captured in the log
        file), not to the semihosting console.  This helper reads the log file
        directly while the VM is still running.
        """
        deadline = time.monotonic() + timeout
        log_path = self.vm._qemu_log_path
        while time.monotonic() < deadline:
            try:
                with open(log_path, "r", errors="replace") as f:
                    if pattern in f.read():
                        return
            except FileNotFoundError:
                pass
            time.sleep(0.1)
        self.fail(f"Timed out waiting for pattern {pattern!r} in QEMU log")

    def test_badva(self) -> None:
        """Tests bad virtual address register handling during dual memory
        operations and TLB exceptions."""
        self.run_console_pattern("badva", "PASS")

    def test_bestwait(self) -> None:
        """Tests the bestwait instruction for thread synchronization by having
        one thread wait for an interrupt and another thread wake it up."""
        self.run_exit_zero("bestwait")

    def test_checkforpriv(self) -> None:
        """Tests privilege violation exceptions by attempting to execute
        privileged instructions in user mode."""
        self.run_exit_zero("checkforpriv")

    def test_ciad_siad(self) -> None:
        """Tests ciad and siad instructions."""
        self.run_exit_zero("ciad-siad")

    def test_double_ex(self) -> None:
        """Tests double exception handling by triggering an exception within
        an exception handler."""
        self.run_exit_zero("double_ex")

    def test_fastint(self) -> None:
        """Tests the fast L2VIC interface by enabling, triggering, and
        handling a large number of interrupts."""
        self.run_exit_zero("fastint")

    def test_fastl2vic(self) -> None:
        """Tests the fast L2VIC interface for setting and clearing interrupt
        enable bits."""
        self.run_exit_zero("fastl2vic")

    def test_float_excp(self) -> None:
        """Tests floating point exception handling by triggering invalid
        floating point operations and divide-by-zero exceptions."""
        self.run_exit_zero("float_excp")

    def test_framelimit(self) -> None:
        """Tests stack frame limit functionality by setting framelimit register
        and attempting stack allocations that exceed the limit."""
        self.run_exit_zero("framelimit")

    def test_getcwd(self) -> None:
        """Tests the getcwd system call by calling it with a buffer and
        verifying it returns a valid path pointer."""
        self.run_exit_zero("getcwd")

    def test_gregs(self) -> None:
        """Tests general register (g0-g31) read/write behavior, verifying that
        some registers retain written values while others are read-only."""
        self.run_console_pattern("gregs", "PASS")

    def test_hvx_multi(self) -> None:
        """Tests HVX multi-context functionality by verifying that different
        HVX contexts maintain independent vector register state."""
        self.run_exit_zero("hvx-multi")

    def test_hvx_64b(self) -> None:
        """Tests HVX 64-bit mode support by attempting HVX instructions in
        64-bit mode and verifying proper exception handling."""
        self.run_exit_zero("hvx_64b")

    def test_hvx_ext(self) -> None:
        """Tests HVX extension bits and qfloat operations across different
        architecture revisions."""
        self.run_exit_zero("hvx_ext")

    def test_int_range(self) -> None:
        """Tests the L2VIC interrupt range by setting up, enabling, and clearing
        interrupts across the full range (1-1024)."""
        self.run_exit_zero("int_range")

    def test_invalid_opcode(self) -> None:
        """Tests invalid opcode exception handling by executing a malformed
        instruction and verifying proper exception response."""
        self.run_exit_zero("invalid_opcode")

    def test_k0lock(self) -> None:
        """Tests k0lock/k0unlock instruction functionality for kernel
        synchronization by having multiple threads perform lock/unlock
        operations."""
        self.run_exit_zero("k0lock")

    def test_k0lock_icount(self):
        self.run_exit_zero("k0lock", "-icount", "auto")

    def test_k0lock_syscfg(self) -> None:
        """Tests interaction between k0lock operations and syscfg register
        modifications to ensure proper synchronization."""
        self.run_exit_zero("k0lock-syscfg", "-accel", "tcg,thread=multi")

    def test_levelint(self) -> None:
        """Tests level-triggered interrupts by setting up interrupts as
        level-triggered rather than edge-triggered."""
        self.run_exit_zero("levelint")

    def test_llsc_on_excp(self) -> None:
        """Tests that load-linked/store-conditional state is properly cleared
        when an exception occurs between the linked load and conditional
        store."""
        self.run_exit_zero("llsc_on_excp")

    def test_mmu_asids(self) -> None:
        """Tests MMU Address Space Identifier (ASID) functionality by creating
        TLB entries with different ASIDs and verifying proper address
        translation isolation."""
        self.run_exit_zero("mmu_asids")

    def test_mmu_multi_tlb(self) -> None:
        """Tests MMU behavior when multiple TLB entries match the same virtual
        address, verifying proper exception generation for ambiguous
        translations."""
        self.run_exit_zero("mmu_multi_tlb")

    def test_mmu_overlap(self) -> None:
        """Tests MMU TLB overlap detection by creating overlapping page mappings
        and verifying conditional TLB write behavior."""
        self.run_exit_zero("mmu_overlap")

    def test_mmu_page_size(self) -> None:
        """Tests MMU support for various page sizes (4KB to 1GB) by creating
        mappings, performing loads/stores, and executing code through different
        page size translations."""
        self.run_exit_zero("mmu_page_size")

    def test_multiple_writes(self) -> None:
        """Tests detection of register write conflicts by executing packets that
        attempt to write the same register multiple times."""
        self.run_exit_zero("multiple_writes")

    def test_pendalot(self) -> None:
        """Tests high-volume interrupt processing by triggering and handling
        1023 different interrupts and logging their completion."""
        self.run_exit_zero("pendalot")

    def test_pend_wake_wait(self) -> None:
        """Tests thread synchronization using software interrupts with multiple
        worker threads performing tasks while interrupt cycling occurs."""
        self.run_exit_zero("pend_wake_wait")

    def test_qfloat_test(self) -> None:
        """Tests HVX qfloat operations including arithmetic, comparisons,
        min/max, and conversions between different qfloat formats."""
        self.run_exit_zero("qfloat_test")

    def test_qtimer(self) -> None:
        """Tests QTimer functionality by setting up multiple timer interrupts
        at different frequencies and handling them properly in wait mode."""
        self.run_exit_zero("qtimer")

    def test_qtimer_test(self) -> None:
        """Tests QTimer version register reads and basic timer interrupt
        functionality with a simplified version of the qtimer test."""
        self.run_exit_zero("qtimer_test")

    def test_reg_reads(self) -> None:
        """Tests reading from performance monitoring unit (PMU) registers and
        HVX packet counting functionality during vector operations."""
        self.run_exit_zero("reg-reads")

    def test_rev(self) -> None:
        """Tests reading the processor revision register to verify the
        architecture version is properly reported."""
        self.run_console_pattern("rev", "0x81")

    def test_single_step(self) -> None:
        """Tests single-step debugging functionality by enabling single-step
        mode and verifying debug exceptions are generated for each
        instruction."""
        self.run_exit_zero("single_step")

    def test_standalone_vec(self) -> None:
        """Tests HVX vector scatter/gather operations with various data types
        and addressing modes, including predicated operations and different
        offset sizes."""
        self.run_exit_zero("standalone_vec")

    def test_start(self) -> None:
        """Tests the start instruction by verifying it properly resets specified
        threads while preserving the current thread's state and SSR cause
        field."""
        self.run_console_pattern("start", "PASS")

    def test_swi2(self) -> None:
        """Tests software interrupt handling under high load with multiple
        threads cycling interrupt enables/disables and task priorities."""
        self.run_exit_zero("swi2", machine="V81QA_1")

    def test_swi_fs(self) -> None:
        """Tests software interrupt functionality combined with file system
        operations by performing file I/O while handling software interrupts."""
        self.run_exit_zero("swi_fs")

    def test_swi_wait(self) -> None:
        """Tests software interrupt delivery to threads in wait state by having
        multiple threads wait for interrupts and verifying proper wake-up and
        interrupt distribution."""
        self.run_exit_zero("swi_wait")

    def test_sys_atomics(self) -> None:
        """Tests atomic memory operations (load-linked/store-conditional) with
        multiple threads to verify proper atomic increment behavior and
        contention handling."""
        self.run_exit_zero("sys_atomics")

    def test_sys_reg_mut(self) -> None:
        """Tests system register mutation behavior by writing to various system
        control registers and verifying which bits are writable versus
        read-only or reserved."""
        self.run_exit_zero("sys_reg_mut")

    def test_tlblock(self) -> None:
        """Tests TLB lock/unlock functionality by having multiple threads
        perform TLB lock/unlock operations to verify proper TLB
        synchronization."""
        self.run_exit_zero("tlblock")

    def test_udma(self) -> None:
        """Tests user-mode DMA operations by setting up DMA descriptors for
        memory transfers and verifying successful data movement between
        source and destination buffers."""
        self.run_exit_zero("udma")

    def test_vid_group(self) -> None:
        """Tests VID group assignment
        functionality by configuring interrupt groups and verifying proper
        interrupt routing to specific threads."""
        self.run_exit_zero("vid-group")

    def test_vid_reg(self) -> None:
        """Tests VID register read/write functionality by writing test values
        and verifying the register properly updates while rejecting invalid
        values."""
        self.run_exit_zero("vid_reg")

    def test_dtg_interrupt(self) -> None:
        """Tests direct-to-guest interrupt delivery by configuring VIC1
        virtualization (CCR.VV1), entering guest mode, and verifying that a
        pending interrupt is delivered via the Guest Event Vector Table
        with correct GSR fields (CAUSE, UM, GIE)."""
        self.run_exit_zero("dtg_interrupt")

    def test_invalid_insn_for_rev_v66(self) -> None:
        """Tests that instructions invalid for V66 architecture revision
        properly trigger invalid instruction exceptions."""
        self.run_exit_zero("invalid_insn_for_rev", machine="V66G_1024")

    def test_invalid_insn_for_rev_v68(self) -> None:
        """Tests that instructions invalid for V68 architecture revision
        properly trigger invalid instruction exceptions."""
        self.run_exit_zero("invalid_insn_for_rev", machine="V68N_1024")

    def test_fopen(self):
        """fopen reads a file passed via --append and verifies its contents."""
        self.run_exit_zero("fopen", "-append", "dummy.so")

    def test_ftrunc(self):
        """ftrunc truncates _testfile_ftrunc from 6 bytes to 1 byte."""
        ftrunc_path = os.path.join(self.workdir, "_testfile_ftrunc")
        # Sleep 1 s so mtime change is observable (mirrors the Makefile recipe)
        time.sleep(1)
        self.run_exit_zero("ftrunc")
        self.assertEqual(
            os.path.getsize(ftrunc_path),
            1,
            "_testfile_ftrunc should be 1 byte after ftrunc",
        )

    def test_dirent(self):
        """dirent lists a directory passed via --append; output must be
        '. .. fileA fileB'."""
        self.run_console_pattern(
            "dirent", ". .. fileA fileB", "-append", "_dirent_testdir"
        )

    def test_access(self):
        """access checks R_OK|W_OK on _testfile_access."""
        self.run_exit_zero("access")

    def test_semihost(self):
        self.run_console_pattern("semihost", "PASS")

    def test_mmu_permissions(self):
        """MMU permission bits: TLB miss and permission-error handlers."""
        self.run_exit_zero("mmu_permissions")

    def test_mmu_cacheops(self):
        """Cache operations raise proper TLB-miss / permission exceptions."""
        self.run_exit_zero("mmu_cacheops",
                           "-cpu", "any,cacheop-exceptions=true")

    def test_hsv39_tlb(self):
        """HSV39 TLB page-size test; requires machine V81QA_1."""
        self.run_exit_zero("hsv39_tlb", machine="V81QA_1")

    def test_swi(self):
        self.run_exit_zero("swi", machine="V81QA_1")

    def test_swi_icount(self):
        self.run_exit_zero("swi", "-icount", "auto", machine="V81QA_1")

    def test_swi2_icount(self):
        self.run_exit_zero("swi2", "-icount", "auto", machine="V81QA_1")

    def test_thread_scheduling(self):
        self.run_exit_zero("thread_scheduling")

    def test_thread_scheduling_icount(self):
        self.run_exit_zero("thread_scheduling", "-icount", "auto")

    def test_test_thread(self):
        self.run_exit_zero("test-thread", "-smp", "cpus=8")

    def test_test_thread_sttcg(self):
        self.run_exit_zero("test-thread",
                           "-smp", "cpus=8",
                           "-accel", "tcg,thread=single")

    def test_nmi_cross_thread(self):
        self.run_exit_zero("nmi_cross_thread")

    def test_k0locklock(self):
        self.set_machine("sim")
        self.set_vm_arg("-display", "none")
        self.set_vm_arg("-kernel", self.binary("k0locklock"))
        self.set_vm_arg("-d", "guest_errors")
        self.vm.launch()
        try:
            self._wait_for_log_pattern("Double k0lock at PC")
        finally:
            self.vm.kill()

    def test_tlblocklock(self):
        self.set_machine("sim")
        self.set_vm_arg("-display", "none")
        self.set_vm_arg("-kernel", self.binary("tlblocklock"))
        self.set_vm_arg("-d", "guest_errors")
        self.vm.launch()
        try:
            self._wait_for_log_pattern("Double tlblock at PC")
        finally:
            self.vm.kill()

    def test_inf_loop(self):
        """inf-loop: 4 threads start, survive system_reset, start again."""
        self.set_machine("sim")
        self.set_vm_arg("-display", "none")
        self.set_vm_arg("-kernel", self.binary("inf-loop"))
        self.vm.launch()
        try:
            for i in range(4):
                self._wait_for_log_pattern(f"Starting thread {i}")
            self.vm.qmp("system_reset")
            for i in range(4):
                self._wait_for_log_pattern(f"Starting thread {i}")
        finally:
            self.vm.kill()

    def test_pmu(self):
        self.run_exit_zero("pmu", "-smp", "cpus=8")

    def test_pcycle(self):
        self.run_exit_zero("pcycle",
                           "-cpu", "any,count-gcycle-xt=on")

    def test_timer_reg(self):
        self.run_exit_zero("timer_reg")

    def test_timer_reg_icount(self):
        self.run_exit_zero("timer_reg", "-icount", "auto")

    @unittest.skip(
        "vwctrl is disabled in qemu-hexagon-testing "
        "pending Q6_mxmem2_bias_A intrinsic support"
    )
    def test_vwctrl(self):
        self.run_console_pattern("vwctrl", "PASS", machine="sim_coproc")

    def test_invalid_hmx(self):
        self.run_exit_zero("invalid_hmx", machine="sim_coproc")

    def test_neg_unaligned(self):
        self.run_expect_exit_nonzero("unaligned", r"0x20|Misaligned Load")

    def test_neg_vtcm_error(self):
        self.run_expect_exit_nonzero("vtcm_error", r"0x26")

    def test_neg_no_hmx(self):
        self.run_expect_exit_nonzero("neg-no-hmx", r"0x18")

    def test_neg_hvx_nocoproc(self):
        self.run_expect_exit_nonzero("hvx_nocoproc", r"0x16")

    def test_memcpy(self):
        self.run_exit_zero("memcpy", machine="V73NA_1024")

    def test_hvx_64b_v66(self):
        self.run_exit_zero("hvx_64b", machine="V66G_1024")

    def test_hvx_64b_v68(self):
        self.run_exit_zero("hvx_64b", machine="V68N_1024")


if __name__ == "__main__":
    QemuSystemTest.main()
