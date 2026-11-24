/*
 * Definitions shared by bare metal tests for heterogeneous machine
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef HETEROGENEOUS_H
#define HETEROGENEOUS_H

#include <stdint.h>

/* must match hw/arm/heterogeneous.c memory map */
#define UART_BASE       0x10000000UL
#define SRAM_BASE       0x20000000UL

/* 16550 registers */
#define UART_THR        0x0
#define UART_LSR        0x5
#define UART_LSR_THRE   0x20

/*
 * Mailbox in shared SRAM, used to serialize output between cpus.
 * SRAM is zeroed at reset.
 */
#define MAILBOX         ((uint32_t *)SRAM_BASE) /* accessed with atomics */
#define MAILBOX_ARM_DONE 1

static inline void uart_putc(char c)
{
    /* volatile: MMIO accesses must not be optimized out */
    volatile uint8_t *uart = (volatile uint8_t *)UART_BASE;

    while (!(uart[UART_LSR] & UART_LSR_THRE)) {
        /* wait for transmit holding register to be empty */
    }
    uart[UART_THR] = c;
}

static inline void uart_puts(const char *s)
{
    while (*s) {
        if (*s == '\n') {
            uart_putc('\r');
        }
        uart_putc(*s++);
    }
}

#endif /* HETEROGENEOUS_H */
