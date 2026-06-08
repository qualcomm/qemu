/*
 * Self-checking test for RISC-V Xqccmt extension: qc.cm.jt and qc.cm.jalt.
 * RV32 only - qc_iu (the Xqccmt-capable QEMU CPU) is defined as MXL_RV32
 * and exists only for the riscv32 target in user mode.
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * =========================================================================
 * WHAT EACH CASE PROVES
 * =========================================================================
 *
 * Case A - qc.cm.jt (index 1, entry bit0 = 1 -> masked to 0):
 *   Verifies that the CPU loads JVT[1] and jumps to (entry & ~1).
 *   No link register is written.  A sentinel register (s2) is set before
 *   the jump and checked after return to confirm the target was reached.
 *   ra and t0 are zeroed before the jt; the target asserts both are still 0,
 *   proving jt writes no link register.  bit0 is set in JVT[1] to exercise
 *   the CPU's bit0 masking (entry & ~1) for the jump target address.
 *
 * Case B - qc.cm.jalt (index 32, entry bit0 = 0 -> link RA):
 *   Before the jalt, both ra and t0 are zeroed.  At the target, asserts
 *   ra != 0 (received the return address) and t0 == 0 (unchanged).
 *   Returns via ra.  Proves bit0=0 selects ra as the link register.
 *
 * Case C - qc.cm.jalt (index 33, entry bit0 = 1 -> link T0):
 *   Before the jalt, both ra and t0 are zeroed.  At the target, asserts
 *   t0 != 0 (received the return address) and ra == 0 (unchanged).
 *   Returns via t0.  Proves bit0=1 selects t0 as the link register.
 *
 * =========================================================================
 * INSTRUCTION ENCODING (raw .short - no assembler zcmt support needed)
 * =========================================================================
 *
 * 16-bit halfword layout (bit15..bit0):
 *   bits[15:13] = 0b101
 *   bits[12:5]  = index (8 bits)
 *   bits[4:2]   = 0b000
 *   bits[1:0]   = 0b10
 *
 * encoding = 0xA002 | (index << 5)
 *
 * Examples:
 *   qc.cm.jt  index=1  -> 0xA002 | (1<<5)  = 0xA022
 *   qc.cm.jalt index=32 -> 0xA002 | (32<<5) = 0xA402
 *   qc.cm.jalt index=33 -> 0xA002 | (33<<5) = 0xA422
 *
 * =========================================================================
 * JVT CSR (0x17) layout
 * =========================================================================
 *   mode = JVT & 0x3F  (must be 0 for table jumps)
 *   base = JVT & ~0x3F (table base; must be 64-byte aligned)
 */

#if __riscv_xlen != 32
# error "This test targets RV32 only (qc_iu is an RV32 CPU)."
#endif

/* -------------------------------------------------------------------------
 * Instruction encoding macros
 * -------------------------------------------------------------------------
 * qc.cm.jt  index  -> .short (0xA002 | ((index) << 5))
 * qc.cm.jalt index -> .short (0xA002 | ((index) << 5))
 * (same encoding; split at index < 32 vs >= 32 is done by the CPU)
 */
#define QC_CM_JT(idx)   .short (0xA002 | ((idx) << 5))
#define QC_CM_JALT(idx) .short (0xA002 | ((idx) << 5))

/* -------------------------------------------------------------------------
 * RV32 only: JVT entries are XLEN/8 = 4 bytes; qc_iu is an RV32 CPU.
 * -------------------------------------------------------------------------
 * ENTRY_BYTES : size of one JVT entry (4 bytes on RV32)
 * STORE_ENTRY : store a word (32-bit) value to memory
 */
#define ENTRY_BYTES  4
#define STORE_ENTRY  sw

/* -------------------------------------------------------------------------
 * Exit via Linux ecall (syscall 93 = exit)
 * -------------------------------------------------------------------------
 * a0 = exit code, a7 = 93
 */
#define EXIT_CODE(code) \
    li   a0, (code); \
    li   a7, 93;     \
    ecall

/* -------------------------------------------------------------------------
 * Failure codes (nonzero, distinct per case for easy diagnosis)
 * -------------------------------------------------------------------------
 */
#define FAIL_JT_NOT_REACHED   1   /* Case A: jt_target was never reached */
#define FAIL_JALT_RA_RA_ZERO  2   /* Case B: ra was not set (still 0) */
#define FAIL_JALT_RA_T0_DIRTY 3   /* Case B: t0 was modified (should be 0) */
#define FAIL_JALT_T0_T0_ZERO  4   /* Case C: t0 was not set (still 0) */
#define FAIL_JALT_T0_RA_DIRTY 5   /* Case C: ra was modified (should be 0) */
#define FAIL_JT_RA_DIRTY      6   /* Case A: jt wrote ra (must stay 0) */
#define FAIL_JT_T0_DIRTY      7   /* Case A: jt wrote t0 (must stay 0) */

/* =========================================================================
 * .data section - JVT table (populated at runtime)
 * =========================================================================
 *
 * The table must be 64-byte aligned so that the low 6 bits of the base
 * address are zero (mode = 0).  We reserve space for 34 entries (indices
 * 0..33); only indices 1, 32, 33 are used.
 */
    .section .data
    .balign 64
jvt_table:
    .fill (34 * ENTRY_BYTES), 1, 0   /* 34 zero-initialised entries */

/* =========================================================================
 * .text section - program entry and test cases
 * =========================================================================
 */
    .section .text
    .global _start
_start:
    /* ------------------------------------------------------------------
     * Populate the JVT table at runtime.
     *
     * t1 = base address of jvt_table
     * t2 = scratch for entry address computation
     * t3 = target address to store
     *
     * Use lla (local address, PC-relative auipc+addi) rather than la
     * (which would generate a GOT-indirect load and create a .got section
     * in the same page as .text, causing conflicting PT_LOAD permissions).
     * ------------------------------------------------------------------
     */
    lla  t1, jvt_table          /* t1 = JVT base (PC-relative, no GOT) */

    /* Entry 1: jt_target | 1 - bit0 set to verify qc.cm.jt masks it */
    lla  t3, jt_target
    /* set bit0; qc.cm.jt must mask it (target = entry & ~1) */
    ori  t3, t3, 1
    li   t2, (1 * ENTRY_BYTES)
    add  t2, t1, t2
    STORE_ENTRY t3, 0(t2)

    /* Entry 32: jalt_ra_target (bit0 = 0 -> link ra) */
    lla  t3, jalt_ra_target
    li   t2, (32 * ENTRY_BYTES)
    add  t2, t1, t2
    STORE_ENTRY t3, 0(t2)

    /* Entry 33: jalt_t0_target | 1 (bit0 = 1 -> link t0) */
    lla  t3, jalt_t0_target
    ori  t3, t3, 1              /* set bit0 to select t0 as link reg */
    li   t2, (33 * ENTRY_BYTES)
    add  t2, t1, t2
    STORE_ENTRY t3, 0(t2)

    /* ------------------------------------------------------------------
     * Write JVT CSR (0x17).
     * base = t1 (already 64-byte aligned, so low 6 bits = 0 = mode 0).
     * ------------------------------------------------------------------
     */
    csrw 0x17, t1

    /* ==================================================================
     * Case A: qc.cm.jt index=1
     *
     * s2 is used as a "reached" sentinel: 0 = not reached, 1 = reached.
     * ra and t0 are zeroed before the jt; the target asserts both are
     * still 0 (jt must not write any link register).
     * After the jump, execution continues at jt_cont.
     * ==================================================================
     */
    li   s2, 0                  /* sentinel: not yet reached */
    li   ra, 0                  /* clear ra - must be unchanged by jt */
    li   t0, 0                  /* clear t0 - must be unchanged by jt */

    /* qc.cm.jt index=1 -> encoding = 0xA002 | (1<<5) = 0xA022 */
    QC_CM_JT(1)
    /* NOTE: qc.cm.jt does NOT write a link register, so we cannot use
     * ra to return.  Instead jt_target jumps explicitly to jt_cont. */

jt_cont:
    /* Verify that jt_target was reached */
    li   t0, 1
    bne  s2, t0, case_a_fail
    j    case_b_start

case_a_fail:
    EXIT_CODE(FAIL_JT_NOT_REACHED)

    /* ==================================================================
     * Case B: qc.cm.jalt index=32 (entry bit0=0 -> link ra)
     *
     * Before the jalt: ra = 0, t0 = 0.
     * At jalt_ra_target: assert ra != 0 && t0 == 0.
     * Return via ra (which holds the return address).
     * ==================================================================
     */
case_b_start:
    li   ra, 0                  /* clear ra */
    li   t0, 0                  /* clear t0 */

    /* qc.cm.jalt index=32 -> encoding = 0xA002 | (32<<5) = 0xA402 */
    QC_CM_JALT(32)
    /* Execution resumes here after jalt_ra_target does `jr ra` */

case_b_cont:
    /* If we reach here, Case B returned correctly.  Fall through to C. */
    j    case_c_start

    /* ==================================================================
     * Case C: qc.cm.jalt index=33 (entry bit0=1 -> link t0)
     *
     * Before the jalt: ra = 0, t0 = 0.
     * At jalt_t0_target: assert t0 != 0 && ra == 0.
     * Return via t0 (which holds the return address).
     * ==================================================================
     */
case_c_start:
    li   ra, 0                  /* clear ra */
    li   t0, 0                  /* clear t0 */

    /* qc.cm.jalt index=33 -> encoding = 0xA002 | (33<<5) = 0xA422 */
    QC_CM_JALT(33)
    /* Execution resumes here after jalt_t0_target does `jr t0` */

case_c_cont:
    /* All three cases passed. */
    EXIT_CODE(0)

    /* ==================================================================
     * Jump targets
     * ==================================================================
     */

    /*
     * jt_target - reached by qc.cm.jt index=1.
     * Assert ra == 0 and t0 == 0 (jt must not write any link register).
     * Set the sentinel register and jump back to jt_cont.
     * (No link register was written by jt, so we must use an explicit j.)
     * Use t1/t2/t3 as scratch so we do not disturb ra, t0, or s2.
     */
    .balign 2
jt_target:
    /* Check ra == 0 (jt must not write ra) */
    bnez ra, jt_fail_ra_dirty
    /* Check t0 == 0 (jt must not write t0) */
    bnez t0, jt_fail_t0_dirty
    /* Both link-register checks passed; set sentinel and return */
    li   s2, 1                  /* mark as reached */
    j    jt_cont

jt_fail_ra_dirty:
    EXIT_CODE(FAIL_JT_RA_DIRTY)

jt_fail_t0_dirty:
    EXIT_CODE(FAIL_JT_T0_DIRTY)

    /*
     * jalt_ra_target - reached by qc.cm.jalt index=32 (entry bit0=0).
     * Assert: ra != 0 (return address written to ra)
     *         t0 == 0 (t0 unchanged)
     * Return via ra.
     */
    .balign 2
jalt_ra_target:
    /* Check ra != 0 */
    beqz ra, jalt_ra_fail_ra_zero
    /* Check t0 == 0 */
    bnez t0, jalt_ra_fail_t0_dirty
    /* Both checks passed; return via ra */
    jr   ra

jalt_ra_fail_ra_zero:
    EXIT_CODE(FAIL_JALT_RA_RA_ZERO)

jalt_ra_fail_t0_dirty:
    EXIT_CODE(FAIL_JALT_RA_T0_DIRTY)

    /*
     * jalt_t0_target - reached by qc.cm.jalt index=33 (entry bit0=1).
     * Assert: t0 != 0 (return address written to t0)
     *         ra == 0 (ra unchanged)
     * Return via t0.
     */
    .balign 2
jalt_t0_target:
    /* Check t0 != 0 */
    beqz t0, jalt_t0_fail_t0_zero
    /* Check ra == 0 */
    bnez ra, jalt_t0_fail_ra_dirty
    /* Both checks passed; return via t0 */
    jr   t0

jalt_t0_fail_t0_zero:
    EXIT_CODE(FAIL_JALT_T0_T0_ZERO)

jalt_t0_fail_ra_dirty:
    EXIT_CODE(FAIL_JALT_T0_RA_DIRTY)
