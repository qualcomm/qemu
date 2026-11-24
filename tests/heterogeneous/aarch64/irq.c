/*
 * Interrupts test for aarch64 cpu of heterogeneous machine
 *
 * Checks interrupts are delivered through GICv3 (private to this cpu):
 * - software generated interrupt (SGI 0, sent to self)
 * - generic timer (EL1 physical timer, PPI 30)
 * - shared UART (THR empty interrupt, SPI 1 / INTID 33)
 * Then signals riscv64 cpu it can run its own test.
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "heterogeneous.h"

/* must match hw/arm/heterogeneous.c */
#define GICD_BASE               0x08000000UL
#define GICR_BASE               0x080a0000UL
#define GICR_SGI_BASE           (GICR_BASE + 0x10000)

#define GICD_CTLR               0x0
#define GICD_CTLR_ENABLE_GRP1   (1 << 1)
#define GICD_CTLR_ARE           (1 << 4)
#define GICD_IGROUPR(n)         (0x080 + 4 * ((n) / 32))
#define GICD_ISENABLER(n)       (0x100 + 4 * ((n) / 32))
#define GICD_ICENABLER(n)       (0x180 + 4 * ((n) / 32))
#define GICD_IPRIORITYR(n)      (0x400 + (n))
#define GICD_IROUTER(n)         (0x6000 + 8 * (n))
#define GICR_WAKER              0x14
#define GICR_WAKER_SLEEP        (1 << 1)
#define GICR_WAKER_ASLEEP       (1 << 2)
#define GICR_IGROUPR0           0x080
#define GICR_ISENABLER0         0x100
#define GICR_IPRIORITYR(n)      (0x400 + (n))

#define INTID_SGI               0
#define INTID_TIMER             30
#define INTID_UART              33
#define INTID_SPURIOUS          1023

#define read_sysreg(r) ({                                   \
    uint64_t __val;                                         \
    asm volatile("mrs %0, " #r : "=r"(__val));              \
    __val;                                                  \
})
#define write_sysreg(r, v) \
    asm volatile("msr " #r ", %0" : : "r"((uint64_t)(v)))
#define isb() asm volatile("isb" : : : "memory")

/* last interrupt handled, written by irq_handler */
static volatile uint32_t last_intid = INTID_SPURIOUS;

void irq_handler(void)
{
    uint32_t intid = read_sysreg(ICC_IAR1_EL1) & 0xffffff;

    switch (intid) {
    case INTID_SPURIOUS:
        return;
    case INTID_TIMER:
        /* disable timer, to deassert its interrupt */
        write_sysreg(CNTP_CTL_EL0, 0);
        isb();
        break;
    case INTID_UART:
        /* disable UART interrupt */
        mmio_write8(UART_BASE + UART_IER, 0);
        break;
    default:
        break;
    }
    last_intid = intid;
    write_sysreg(ICC_EOIR1_EL1, intid);
    isb();
}

void unexpected_exception(uint64_t esr)
{
    uart_puts("aarch64: unexpected exception\n");
}

/*
 * Wait for interrupt @intid to be handled. IRQs are masked, except around
 * WFI, so we can't miss the wake up.
 */
static void wait_irq(uint32_t intid)
{
    while (last_intid != intid) {
        asm volatile("wfi");
        asm volatile("msr daifclr, #2\n\tisb\n\tmsr daifset, #2"
                     : : : "memory");
    }
    last_intid = INTID_SPURIOUS;
}

static void gic_enable_irq(uint32_t intid)
{
    if (intid < 32) {
        /* SGI/PPI: configured in redistributor */
        mmio_write32(GICR_SGI_BASE + GICR_IGROUPR0,
                     mmio_read32(GICR_SGI_BASE + GICR_IGROUPR0) |
                     (1 << intid));
        mmio_write8(GICR_SGI_BASE + GICR_IPRIORITYR(intid), 0x80);
        mmio_write32(GICR_SGI_BASE + GICR_ISENABLER0, 1 << intid);
    } else {
        /* SPI: configured in distributor, routed to this cpu (affinity 0) */
        mmio_write32(GICD_BASE + GICD_IGROUPR(intid),
                     mmio_read32(GICD_BASE + GICD_IGROUPR(intid)) |
                     (1 << (intid % 32)));
        mmio_write8(GICD_BASE + GICD_IPRIORITYR(intid), 0x80);
        mmio_write64(GICD_BASE + GICD_IROUTER(intid), 0);
        mmio_write32(GICD_BASE + GICD_ISENABLER(intid), 1 << (intid % 32));
    }
}

static void gic_init(void)
{
    /* distributor: affinity routing, group 1 (no security extensions) */
    mmio_write32(GICD_BASE + GICD_CTLR, GICD_CTLR_ARE);
    mmio_write32(GICD_BASE + GICD_CTLR,
                 GICD_CTLR_ARE | GICD_CTLR_ENABLE_GRP1);

    /* redistributor: wake up */
    mmio_write32(GICR_BASE + GICR_WAKER,
                 mmio_read32(GICR_BASE + GICR_WAKER) & ~GICR_WAKER_SLEEP);
    while (mmio_read32(GICR_BASE + GICR_WAKER) & GICR_WAKER_ASLEEP) {
        /* wait */
    }

    /* cpu interface */
    write_sysreg(ICC_SRE_EL1, read_sysreg(ICC_SRE_EL1) | 1);
    isb();
    write_sysreg(ICC_PMR_EL1, 0xff);
    write_sysreg(ICC_IGRPEN1_EL1, 1);
    isb();
}

int main(void)
{
    write_sysreg(VBAR_EL1, (uintptr_t)vectors);
    isb();
    gic_init();

    /* software generated interrupt, sent to self (target list: aff0 = 0) */
    gic_enable_irq(INTID_SGI);
    write_sysreg(ICC_SGI1R_EL1, ((uint64_t)INTID_SGI << 24) | 1);
    isb();
    wait_irq(INTID_SGI);
    uart_puts("aarch64: software interrupt received\n");

    /* timer, firing in 1ms */
    gic_enable_irq(INTID_TIMER);
    write_sysreg(CNTP_TVAL_EL0, read_sysreg(CNTFRQ_EL0) / 1000);
    write_sysreg(CNTP_CTL_EL0, 1);
    isb();
    wait_irq(INTID_TIMER);
    uart_puts("aarch64: timer interrupt received\n");

    /* shared UART, raises an interrupt as soon as THR is empty */
    gic_enable_irq(INTID_UART);
    mmio_write8(UART_BASE + UART_IER, UART_IER_THRI);
    wait_irq(INTID_UART);
    uart_puts("aarch64: uart interrupt received\n");

    /* riscv64 cpu will use UART interrupt too, ignore it */
    mmio_write32(GICD_BASE + GICD_ICENABLER(INTID_UART),
                 1 << (INTID_UART % 32));

    __atomic_store_n(MAILBOX, MAILBOX_ARM_DONE, __ATOMIC_RELEASE);
    return 0;
}
