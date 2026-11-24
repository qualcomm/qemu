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
#define UART_IER        0x1
#define UART_IER_THRI   0x02
#define UART_LSR        0x5
#define UART_LSR_THRE   0x20

/*
 * Mailbox in shared SRAM, used to serialize output between cpus.
 * SRAM is zeroed at reset.
 */
#define MAILBOX         ((uint32_t *)SRAM_BASE) /* accessed with atomics */
#define MAILBOX_ARM_DONE 1

/* MMIO accessors: volatile, so accesses are not optimized out */
static inline uint8_t mmio_read8(uintptr_t addr)
{
    return *(volatile uint8_t *)addr; /* MMIO access */
}

static inline void mmio_write8(uintptr_t addr, uint8_t val)
{
    *(volatile uint8_t *)addr = val; /* MMIO access */
}

static inline uint32_t mmio_read32(uintptr_t addr)
{
    return *(volatile uint32_t *)addr; /* MMIO access */
}

static inline void mmio_write32(uintptr_t addr, uint32_t val)
{
    *(volatile uint32_t *)addr = val; /* MMIO access */
}

static inline uint64_t mmio_read64(uintptr_t addr)
{
    return *(volatile uint64_t *)addr; /* MMIO access */
}

static inline void mmio_write64(uintptr_t addr, uint64_t val)
{
    *(volatile uint64_t *)addr = val; /* MMIO access */
}

static inline void uart_putc(char c)
{
    while (!(mmio_read8(UART_BASE + UART_LSR) & UART_LSR_THRE)) {
        /* wait for transmit holding register to be empty */
    }
    mmio_write8(UART_BASE + UART_THR, c);
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

/* interface between assembly and C code of tests */
#if defined(__aarch64__)
extern char vectors[];
void irq_handler(void);
void unexpected_exception(uint64_t esr);
#elif defined(__riscv)
extern char trap_entry[];
void trap_handler(unsigned long mcause);
#endif

#endif /* HETEROGENEOUS_H */
