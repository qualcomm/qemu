/*
 * Minimal boot code for aarch64 cpu of heterogeneous machine
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

    .section .text.boot, "ax"
    .global _start
_start:
    ldr     x0, =__stack_top
    mov     sp, x0
    bl      main
1:
    wfe
    b       1b
