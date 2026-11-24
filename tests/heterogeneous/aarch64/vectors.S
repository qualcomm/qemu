/*
 * Exception vectors for aarch64 cpu of heterogeneous machine
 *
 * Only IRQ taken from current EL (using SP_ELx) is expected, and handled by
 * irq_handler(). Any other exception calls unexpected_exception().
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

    .macro  vector, handler
    .balign 0x80
    b       \handler
    .endm

    .section .text.vectors, "ax"
    .balign 2048
    .global vectors
vectors:
    /* current EL, SP_EL0 */
    vector  unexpected
    vector  unexpected
    vector  unexpected
    vector  unexpected
    /* current EL, SP_ELx */
    vector  unexpected
    vector  irq_entry
    vector  unexpected
    vector  unexpected
    /* lower EL, aarch64 */
    vector  unexpected
    vector  unexpected
    vector  unexpected
    vector  unexpected
    /* lower EL, aarch32 */
    vector  unexpected
    vector  unexpected
    vector  unexpected
    vector  unexpected

/* save caller-saved registers, call irq_handler, restore and return */
irq_entry:
    sub     sp, sp, #176
    stp     x0, x1, [sp, #0]
    stp     x2, x3, [sp, #16]
    stp     x4, x5, [sp, #32]
    stp     x6, x7, [sp, #48]
    stp     x8, x9, [sp, #64]
    stp     x10, x11, [sp, #80]
    stp     x12, x13, [sp, #96]
    stp     x14, x15, [sp, #112]
    stp     x16, x17, [sp, #128]
    stp     x18, x29, [sp, #144]
    str     x30, [sp, #160]
    bl      irq_handler
    ldp     x0, x1, [sp, #0]
    ldp     x2, x3, [sp, #16]
    ldp     x4, x5, [sp, #32]
    ldp     x6, x7, [sp, #48]
    ldp     x8, x9, [sp, #64]
    ldp     x10, x11, [sp, #80]
    ldp     x12, x13, [sp, #96]
    ldp     x14, x15, [sp, #112]
    ldp     x16, x17, [sp, #128]
    ldp     x18, x29, [sp, #144]
    ldr     x30, [sp, #160]
    add     sp, sp, #176
    eret

unexpected:
    mrs     x0, esr_el1
    bl      unexpected_exception
1:
    wfe
    b       1b
