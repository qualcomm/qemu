/*
 * Interrupts test for riscv64 cpu of heterogeneous machine
 *
 * Waits for aarch64 cpu to run its own test, then checks interrupts are
 * delivered through ACLINT and PLIC (private to this cpu):
 * - software interrupt (ACLINT MSWI)
 * - machine timer (ACLINT MTIMER)
 * - shared UART (THR empty interrupt, PLIC source 10)
 * Boot code exits QEMU (semihosting) once main returns.
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "heterogeneous.h"

/* must match hw/arm/heterogeneous.c (this cpu is first hart of each device) */
#define ACLINT_MSIP             0x02000000UL
#define ACLINT_MTIMECMP         0x02004000UL
#define ACLINT_MTIME            0x0200bff8UL
#define ACLINT_TIMEBASE         10000000
#define PLIC_BASE               0x0c000000UL
#define PLIC_PRIORITY(src)      (PLIC_BASE + 4 * (src))
/* context 0 is M-mode of this hart */
#define PLIC_ENABLE(ctx)        (PLIC_BASE + 0x2000 + 0x80 * (ctx))
#define PLIC_THRESHOLD(ctx)     (PLIC_BASE + 0x200000 + 0x1000 * (ctx))
#define PLIC_CLAIM(ctx)         (PLIC_THRESHOLD(ctx) + 4)
#define PLIC_CTX_M              0
#define PLIC_SOURCE_UART        10

#define MSTATUS_MIE             (1 << 3)
#define MCAUSE_INTERRUPT        (1UL << 63)
#define IRQ_M_SOFT              3
#define IRQ_M_TIMER             7
#define IRQ_M_EXT               11
#define IRQ_NONE                0xff

#define csr_read(csr) ({                                    \
    unsigned long __val;                                    \
    asm volatile("csrr %0, " #csr : "=r"(__val));           \
    __val;                                                  \
})
#define csr_write(csr, v) \
    asm volatile("csrw " #csr ", %0" : : "r"((unsigned long)(v)))
#define csr_set(csr, v) \
    asm volatile("csrs " #csr ", %0" : : "r"((unsigned long)(v)))

/* last interrupt handled, written by trap_handler */
static volatile uint32_t last_irq = IRQ_NONE;

void trap_handler(unsigned long mcause)
{
    uint32_t src;

    if (!(mcause & MCAUSE_INTERRUPT)) {
        uart_puts("riscv64: unexpected exception\n");
        for (;;) {
            asm volatile("wfi");
        }
    }

    switch (mcause & ~MCAUSE_INTERRUPT) {
    case IRQ_M_SOFT:
        mmio_write32(ACLINT_MSIP, 0);
        break;
    case IRQ_M_TIMER:
        /* timer won't fire again */
        mmio_write64(ACLINT_MTIMECMP, UINT64_MAX);
        break;
    case IRQ_M_EXT:
        src = mmio_read32(PLIC_CLAIM(PLIC_CTX_M));
        if (!src) {
            /* spurious */
            return;
        }
        if (src == PLIC_SOURCE_UART) {
            /* disable UART interrupt */
            mmio_write8(UART_BASE + UART_IER, 0);
        }
        mmio_write32(PLIC_CLAIM(PLIC_CTX_M), src);
        break;
    default:
        break;
    }
    last_irq = mcause & ~MCAUSE_INTERRUPT;
}

/*
 * Wait for interrupt @irq to be handled. Interrupts are globally disabled,
 * except right after WFI, so we can't miss the wake up (WFI only depends on
 * mip/mie).
 */
static void wait_irq(uint32_t irq)
{
    while (last_irq != irq) {
        asm volatile("wfi");
        asm volatile("csrs mstatus, %0\n\tcsrc mstatus, %0"
                     : : "r"(MSTATUS_MIE) : "memory");
    }
    last_irq = IRQ_NONE;
}

int main(void)
{
    while (__atomic_load_n(MAILBOX, __ATOMIC_ACQUIRE) != MAILBOX_ARM_DONE) {
        /* wait for aarch64 cpu */
    }

    csr_write(mtvec, (uintptr_t)trap_entry);
    /* mtimecmp is 0 at reset, so timer interrupt is pending: disarm it */
    mmio_write64(ACLINT_MTIMECMP, UINT64_MAX);
    csr_set(mie, (1 << IRQ_M_SOFT) | (1 << IRQ_M_TIMER) | (1 << IRQ_M_EXT));

    /* software interrupt */
    mmio_write32(ACLINT_MSIP, 1);
    wait_irq(IRQ_M_SOFT);
    uart_puts("riscv64: software interrupt received\n");

    /* timer, firing in 1ms */
    mmio_write64(ACLINT_MTIMECMP,
                 mmio_read64(ACLINT_MTIME) + ACLINT_TIMEBASE / 1000);
    wait_irq(IRQ_M_TIMER);
    uart_puts("riscv64: timer interrupt received\n");

    /* shared UART, raises an interrupt as soon as THR is empty */
    mmio_write32(PLIC_PRIORITY(PLIC_SOURCE_UART), 1);
    mmio_write32(PLIC_ENABLE(PLIC_CTX_M), 1 << PLIC_SOURCE_UART);
    mmio_write32(PLIC_THRESHOLD(PLIC_CTX_M), 0);
    mmio_write8(UART_BASE + UART_IER, UART_IER_THRI);
    wait_irq(IRQ_M_EXT);
    uart_puts("riscv64: uart interrupt received\n");

    return 0;
}
