/*
 * Bare metal test for aarch64 cpu of heterogeneous machine
 *
 * Prints a message on the shared UART, then signals riscv64 cpu it can print
 * its own.
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "heterogeneous.h"

int main(void)
{
    uart_puts("Hello world from aarch64\n");

    /* make sure output is done before signaling */
    __atomic_store_n(MAILBOX, MAILBOX_ARM_DONE, __ATOMIC_RELEASE);
    return 0;
}
