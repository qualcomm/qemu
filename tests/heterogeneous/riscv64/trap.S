/*
 * Trap entry for riscv64 cpu of heterogeneous machine (M-mode, direct mode)
 *
 * Saves caller-saved registers and calls trap_handler(mcause).
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

    .section .text
    .balign 4
    .global trap_entry
trap_entry:
    addi    sp, sp, -128
    sd      ra, 0(sp)
    sd      t0, 8(sp)
    sd      t1, 16(sp)
    sd      t2, 24(sp)
    sd      t3, 32(sp)
    sd      t4, 40(sp)
    sd      t5, 48(sp)
    sd      t6, 56(sp)
    sd      a0, 64(sp)
    sd      a1, 72(sp)
    sd      a2, 80(sp)
    sd      a3, 88(sp)
    sd      a4, 96(sp)
    sd      a5, 104(sp)
    sd      a6, 112(sp)
    sd      a7, 120(sp)
    csrr    a0, mcause
    call    trap_handler
    ld      ra, 0(sp)
    ld      t0, 8(sp)
    ld      t1, 16(sp)
    ld      t2, 24(sp)
    ld      t3, 32(sp)
    ld      t4, 40(sp)
    ld      t5, 48(sp)
    ld      t6, 56(sp)
    ld      a0, 64(sp)
    ld      a1, 72(sp)
    ld      a2, 80(sp)
    ld      a3, 88(sp)
    ld      a4, 96(sp)
    ld      a5, 104(sp)
    ld      a6, 112(sp)
    ld      a7, 120(sp)
    addi    sp, sp, 128
    mret
