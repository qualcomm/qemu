/*
 * Bare metal test for riscv64 cpu of heterogeneous machine
 *
 * Waits for aarch64 cpu to print its message, then prints its own on the
 * shared UART. Boot code exits QEMU (semihosting) once main returns.
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "heterogeneous.h"

int main(void)
{
    while (__atomic_load_n(MAILBOX, __ATOMIC_ACQUIRE) != MAILBOX_ARM_DONE) {
        /* wait for aarch64 cpu */
    }

    uart_puts("Hello world from riscv64\n");
    return 0;
}
