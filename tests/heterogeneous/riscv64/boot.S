/*
 * Minimal boot code for riscv64 cpu of heterogeneous machine
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

    .section .text.boot, "ax"
    .global _start
_start:
    la      sp, __stack_top
    call    main
    /* exit QEMU with main return value */
    call    semihosting_exit
1:
    wfi
    j       1b

/*
 * Semihosting SYS_EXIT, with status in a0.
 * For RV64, parameter is a pointer to a block {reason, subcode}.
 */
    .equ    SYS_EXIT, 0x18
    .equ    ADP_Stopped_ApplicationExit, 0x20026
    .global semihosting_exit
semihosting_exit:
    la      t0, exit_block
    li      t1, ADP_Stopped_ApplicationExit
    sd      t1, 0(t0)
    sd      a0, 8(t0)
    li      a0, SYS_EXIT
    mv      a1, t0
    /* semihosting trap sequence, must be in the same page, uncompressed */
    .balign 16
    .option push
    .option norvc
    slli    zero, zero, 0x1f
    ebreak
    srai    zero, zero, 7
    .option pop
    ret

    .section .bss
    .balign 8
exit_block:
    .skip 16
