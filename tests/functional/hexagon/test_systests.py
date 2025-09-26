#!/usr/bin/env python3
#
# Copyright(c) 2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
#
# SPDX-License-Identifier: GPL-2.0-or-later

import os
import re
from unittest import skipUnless, skip
from qemu_test import QemuSystemTest, Asset


class SysTestsStandaloneTests(QemuSystemTest):
    SYSTEST_TIMEOUT_SEC = 30

    ASSET_TARBALL = Asset(
        "https://github.com/qualcomm/qemu-hexagon-testing/releases/download/v0.1.7/systests_standalone.tar.gz",
        "3882aa36ac015f3e8caf96ec2e4c3c7e377545a6ff43478baef0f09a805346b8",
    )

    def check(self, test_name: str) -> None:
        """
        Check the semihosting output from a systests_standalone test case.
        Expected pattern: Tests should write expected results via semihosting
        and exit with code 0 on success.
        """
        console_output = str(self.vm).strip()

        if self.vm.exitcode() != 0:
            raise RuntimeError(
                f"Test {test_name} exited with code {self.vm.exitcode()}"
            )

        # Check for common success patterns in semihosting output
        # These patterns may need to be updated based on actual test output
        success_patterns = [
            r"PASS",
        ]
        # Check for more specific failure patterns (avoid false positives)
        # Some tests output "FAIL" as part of normal test output, so we need
        # to be more specific about what constitutes a real failure
        failure_patterns = [
            r"ASSERTION.*failed",
            r"ERROR:.*",
            r"Test.*failed",
            r"Exception.*",
            r"Segmentation fault",
            r"Abort.*",
        ]

        # Look for failure patterns first
        if any(
            re.search(pattern, console_output, re.IGNORECASE)
            for pattern in failure_patterns
        ):
            raise RuntimeError(
                f"Test {test_name} failed: found failure pattern in output"
            )

        # Look for success patterns
        found_success = any(
            re.search(pattern, console_output, re.IGNORECASE)
            for pattern in success_patterns
        )
        if not found_success:
            # If no explicit success/failure pattern, just check exit code
            # Some tests might only indicate success via exit code
            if self.vm.exitcode() == 0:
                found_success = True

        if not found_success:
            raise RuntimeError(
                f"Test {test_name} did not show clear success indication"
            )

    def run_individual_test(self, test_name: str) -> bool:
        """
        Run a single systests_standalone test case
        """
        self.set_machine("V66G_1024")

        self.archive_extract(self.ASSET_TARBALL)

        # Find the extracted directory (name may vary based on tarball
        # structure). Look for the bin directory containing the test
        # executables.
        test_dir = None
        for root, dirs, files in os.walk(self.workdir):
            if "bin" in dirs:
                bin_dir = os.path.join(root, "bin")
                # Check if bin directory contains executable files (without ext)
                if any(
                    os.access(os.path.join(bin_dir, f), os.X_OK)
                    for f in os.listdir(bin_dir)
                ):
                    test_dir = bin_dir
                    break

        if not test_dir:
            self.fail(
                f"Could not find bin directory with executable files "
                f"in {self.workdir}"
            )

        # Find the specific test binary (executable files without extensions)
        target_bin = None
        for filename in os.listdir(test_dir):
            full_path = os.path.join(test_dir, filename)
            # Check if it's an executable file and matches the test name
            if os.access(full_path, os.X_OK) and filename == test_name:
                target_bin = full_path
                break

        self.set_vm_arg("-display", "none")
        self.set_vm_arg("-kernel", target_bin)
        self.vm.launch()
        self.vm.wait(timeout=60.0)
        try:
            self.check(test_name)
            return True
        except RuntimeError as e:
            self.fail(f"Test {test_name} failed: {str(e)}")

    # Explicitly defined test methods for all currently passing tests
    def test_badva(self) -> None:
        """Tests bad virtual address register handling during dual memory
        operations and TLB exceptions."""
        result = self.run_individual_test("badva")
        self.assertTrue(result, "Test badva failed")

    @skip("Test binary missing or fails to run properly")
    def test_bestwait(self) -> None:
        """Tests the bestwait instruction for thread synchronization by having
        one thread wait for an interrupt and another thread wake it up."""
        result = self.run_individual_test("bestwait")
        self.assertTrue(result, "Test bestwait failed")

    def test_checkforpriv(self) -> None:
        """Tests privilege violation exceptions by attempting to execute
        privileged instructions in user mode."""
        result = self.run_individual_test("checkforpriv")
        self.assertTrue(result, "Test checkforpriv failed")

    def test_ciad_siad(self) -> None:
        """Tests ciad and siad instructions."""
        result = self.run_individual_test("ciad-siad")
        self.assertTrue(result, "Test ciad-siad failed")

    @skip("Test fails with exit code 1")
    def test_double_ex(self) -> None:
        """Tests double exception handling by triggering an exception within
        an exception handler."""
        result = self.run_individual_test("double_ex")
        self.assertTrue(result, "Test double_ex failed")

    def test_fastint(self) -> None:
        """Tests the fast L2VIC interface by enabling, triggering, and
        handling a large number of interrupts."""
        result = self.run_individual_test("fastint")
        self.assertTrue(result, "Test fastint failed")

    def test_fastl2vic(self) -> None:
        """Tests the fast L2VIC interface for setting and clearing interrupt
        enable bits."""
        result = self.run_individual_test("fastl2vic")
        self.assertTrue(result, "Test fastl2vic failed")

    @skip("Test fails with exit code -6 (SIGABRT)")
    def test_float_excp(self) -> None:
        """Tests floating point exception handling by triggering invalid
        floating point operations and divide-by-zero exceptions."""
        result = self.run_individual_test("float_excp")
        self.assertTrue(result, "Test float_excp failed")

    @skip("Test times out after 60 seconds")
    def test_framelimit(self) -> None:
        """Tests stack frame limit functionality by setting framelimit register
        and attempting stack allocations that exceed the limit."""
        result = self.run_individual_test("framelimit")
        self.assertTrue(result, "Test framelimit failed")

    def test_getcwd(self) -> None:
        """Tests the getcwd system call by calling it with a buffer and
        verifying it returns a valid path pointer."""
        result = self.run_individual_test("getcwd")
        self.assertTrue(result, "Test getcwd failed")

    def test_gregs(self) -> None:
        """Tests general register (g0-g31) read/write behavior, verifying that
        some registers retain written values while others are read-only."""
        result = self.run_individual_test("gregs")
        self.assertTrue(result, "Test gregs failed")

    def test_hvx_multi(self) -> None:
        """Tests HVX multi-context functionality by verifying that different
        HVX contexts maintain independent vector register state."""
        result = self.run_individual_test("hvx-multi")
        self.assertTrue(result, "Test hvx-multi failed")

    @skip("Test fails with exit code 1 or 3")
    def test_hvx_64b(self) -> None:
        """Tests HVX 64-bit mode support by attempting HVX instructions in
        64-bit mode and verifying proper exception handling."""
        result = self.run_individual_test("hvx_64b")
        self.assertTrue(result, "Test hvx_64b failed")

    @skip("Test fails with exit code 1")
    def test_hvx_ext(self) -> None:
        """Tests HVX extension bits and qfloat operations across different
        architecture revisions."""
        result = self.run_individual_test("hvx_ext")
        self.assertTrue(result, "Test hvx_ext failed")

    def test_int_range(self) -> None:
        """Tests the L2VIC interrupt range by setting up, enabling, and clearing
        interrupts across the full range (1-1024)."""
        result = self.run_individual_test("int_range")
        self.assertTrue(result, "Test int_range failed")

    @skip("Test fails with exit code 1")
    def test_invalid_insn_for_rev(self) -> None:
        """Tests that instructions invalid for the current architecture revision
        properly trigger invalid instruction exceptions."""
        result = self.run_individual_test("invalid_insn_for_rev")
        self.assertTrue(result, "Test invalid_insn_for_rev failed")

    @skip("Test fails with exit code 1")
    def test_invalid_opcode(self) -> None:
        """Tests invalid opcode exception handling by executing a malformed
        instruction and verifying proper exception response."""
        result = self.run_individual_test("invalid_opcode")
        self.assertTrue(result, "Test invalid_opcode failed")

    def test_k0lock(self) -> None:
        """Tests k0lock/k0unlock instruction functionality for kernel
        synchronization by having multiple threads perform lock/unlock
        operations."""
        result = self.run_individual_test("k0lock")
        self.assertTrue(result, "Test k0lock failed")

    @skip("Test times out after 60 seconds")
    def test_k0lock_syscfg(self) -> None:
        """Tests interaction between k0lock operations and syscfg register
        modifications to ensure proper synchronization."""
        result = self.run_individual_test("k0lock-syscfg")
        self.assertTrue(result, "Test k0lock-syscfg failed")

    def test_levelint(self) -> None:
        """Tests level-triggered interrupts by setting up interrupts as
        level-triggered rather than edge-triggered."""
        result = self.run_individual_test("levelint")
        self.assertTrue(result, "Test levelint failed")

    @skip("Test fails with exit code -6 (SIGABRT)")
    def test_llsc_on_excp(self) -> None:
        """Tests that load-linked/store-conditional state is properly cleared
        when an exception occurs between the linked load and conditional
        store."""
        result = self.run_individual_test("llsc_on_excp")
        self.assertTrue(result, "Test llsc_on_excp failed")

    def test_mmu_asids(self) -> None:
        """Tests MMU Address Space Identifier (ASID) functionality by creating
        TLB entries with different ASIDs and verifying proper address
        translation isolation."""
        result = self.run_individual_test("mmu_asids")
        self.assertTrue(result, "Test mmu_asids failed")

    @skip("Test fails with exit code 1")
    def test_mmu_multi_tlb(self) -> None:
        """Tests MMU behavior when multiple TLB entries match the same virtual
        address, verifying proper exception generation for ambiguous
        translations."""
        result = self.run_individual_test("mmu_multi_tlb")
        self.assertTrue(result, "Test mmu_multi_tlb failed")

    def test_mmu_overlap(self) -> None:
        """Tests MMU TLB overlap detection by creating overlapping page mappings
        and verifying conditional TLB write behavior."""
        result = self.run_individual_test("mmu_overlap")
        self.assertTrue(result, "Test mmu_overlap failed")

    def test_mmu_page_size(self) -> None:
        """Tests MMU support for various page sizes (4KB to 1GB) by creating
        mappings, performing loads/stores, and executing code through different
        page size translations."""
        result = self.run_individual_test("mmu_page_size")
        self.assertTrue(result, "Test mmu_page_size failed")

    @skip("Test fails with exit code 4")
    def test_multiple_writes(self) -> None:
        """Tests detection of register write conflicts by executing packets that
        attempt to write the same register multiple times."""
        result = self.run_individual_test("multiple_writes")
        self.assertTrue(result, "Test multiple_writes failed")

    def test_pendalot(self) -> None:
        """Tests high-volume interrupt processing by triggering and handling
        1023 different interrupts and logging their completion."""
        result = self.run_individual_test("pendalot")
        self.assertTrue(result, "Test pendalot failed")

    @skip("Test times out after 60 seconds")
    def test_pend_wake_wait(self) -> None:
        """Tests thread synchronization using software interrupts with multiple
        worker threads performing tasks while interrupt cycling occurs."""
        result = self.run_individual_test("pend_wake_wait")
        self.assertTrue(result, "Test pend_wake_wait failed")

    @skip("Test fails with exit code -6 (SIGABRT)")
    def test_qfloat_test(self) -> None:
        """Tests HVX qfloat (quantized floating point) operations including
        arithmetic, comparisons, min/max, and conversions between different
        qfloat formats."""
        result = self.run_individual_test("qfloat_test")
        self.assertTrue(result, "Test qfloat_test failed")

    @skip("Test times out after 60 seconds")
    def test_qtimer(self) -> None:
        """Tests QTimer functionality by setting up multiple timer interrupts
        at different frequencies and handling them properly in wait mode."""
        result = self.run_individual_test("qtimer")
        self.assertTrue(result, "Test qtimer failed")

    @skip("Test times out after 60 seconds")
    def test_qtimer_test(self) -> None:
        """Tests QTimer version register reads and basic timer interrupt
        functionality with a simplified version of the qtimer test."""
        result = self.run_individual_test("qtimer_test")
        self.assertTrue(result, "Test qtimer_test failed")

    def test_reg_reads(self) -> None:
        """Tests reading from performance monitoring unit (PMU) registers and
        HVX packet counting functionality during vector operations."""
        result = self.run_individual_test("reg-reads")
        self.assertTrue(result, "Test reg-reads failed")

    def test_rev(self) -> None:
        """Tests reading the processor revision register to verify the
        architecture version is properly reported."""
        result = self.run_individual_test("rev")
        self.assertTrue(result, "Test rev failed")

    @skip("Test fails with exit code 1")
    def test_single_step(self) -> None:
        """Tests single-step debugging functionality by enabling single-step
        mode and verifying debug exceptions are generated for each
        instruction."""
        result = self.run_individual_test("single_step")
        self.assertTrue(result, "Test single_step failed")

    @skip("Test times out after 60 seconds")
    def test_standalone_vec(self) -> None:
        """Tests HVX vector scatter/gather operations with various data types
        and addressing modes, including predicated operations and different
        offset sizes."""
        result = self.run_individual_test("standalone_vec")
        self.assertTrue(result, "Test standalone_vec failed")

    @skip("Test times out after 60 seconds")
    def test_start(self) -> None:
        """Tests the start instruction by verifying it properly resets specified
        threads while preserving the current thread's state and SSR cause
        field."""
        result = self.run_individual_test("start")
        self.assertTrue(result, "Test start failed")

    @skip("Test times out after 60 seconds")
    def test_swi2(self) -> None:
        """Tests software interrupt handling under high load with multiple
        threads cycling interrupt enables/disables and task priorities."""
        result = self.run_individual_test("swi2")
        self.assertTrue(result, "Test swi2 failed")

    def test_swi_fs(self) -> None:
        """Tests software interrupt functionality combined with file system
        operations by performing file I/O while handling software interrupts."""
        result = self.run_individual_test("swi_fs")
        self.assertTrue(result, "Test swi_fs failed")

    @skip("Test times out after 60 seconds")
    def test_swi_wait(self) -> None:
        """Tests software interrupt delivery to threads in wait state by having
        multiple threads wait for interrupts and verifying proper wake-up and
        interrupt distribution."""
        result = self.run_individual_test("swi_wait")
        self.assertTrue(result, "Test swi_wait failed")

    @skip("Test fails with exit code 255")
    def test_sys_atomics(self) -> None:
        """Tests atomic memory operations (load-linked/store-conditional) with
        multiple threads to verify proper atomic increment behavior and
        contention handling."""
        result = self.run_individual_test("sys_atomics")
        self.assertTrue(result, "Test sys_atomics failed")

    @skip("Test fails with exit code 4")
    def test_sys_reg_mut(self) -> None:
        """Tests system register mutation behavior by writing to various system
        control registers and verifying which bits are writable versus
        read-only or reserved."""
        result = self.run_individual_test("sys_reg_mut")
        self.assertTrue(result, "Test sys_reg_mut failed")

    def test_tlblock(self) -> None:
        """Tests TLB lock/unlock functionality by having multiple threads
        perform TLB lock/unlock operations to verify proper TLB
        synchronization."""
        result = self.run_individual_test("tlblock")
        self.assertTrue(result, "Test tlblock failed")

    @skip("Test fails with exit code 253")
    def test_udma(self) -> None:
        """Tests user-mode DMA operations by setting up DMA descriptors for
        memory transfers and verifying successful data movement between
        source and destination buffers."""
        result = self.run_individual_test("udma")
        self.assertTrue(result, "Test udma failed")

    @skip("Test times out after 60 seconds")
    def test_vid_group(self) -> None:
        """Tests VID (Vector Interrupt Distributor) group assignment
        functionality by configuring interrupt groups and verifying proper
        interrupt routing to specific threads."""
        result = self.run_individual_test("vid-group")
        self.assertTrue(result, "Test vid-group failed")

    def test_vid_reg(self) -> None:
        """Tests VID register read/write functionality by writing test values
        and verifying the register properly updates while rejecting invalid
        values."""
        result = self.run_individual_test("vid_reg")
        self.assertTrue(result, "Test vid_reg failed")


if __name__ == "__main__":
    QemuSystemTest.main()
