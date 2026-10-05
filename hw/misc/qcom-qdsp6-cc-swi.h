/*
 * CDSP0 Clock Controller (CLKCTL) — QEMU sysbus device
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef HW_MISC_QDSP6_CC_SWI
#define HW_MISC_QDSP6_CC_SWI

#include "hw/core/sysbus.h"

#define TYPE_CDSP0_CLKCTL "cdsp0-clkctl"
OBJECT_DECLARE_SIMPLE_TYPE(Cdsp0ClkctlState, CDSP0_CLKCTL)

#define CDSP0_CLKCTL_MMIO_SIZE 0x440
#define CDSP0_CLKCTL_NUM_REGS (CDSP0_CLKCTL_MMIO_SIZE / 4)

typedef struct Cdsp0ClkctlState {
    SysBusDevice parent_obj;
    MemoryRegion mmio;
    uint32_t regs[CDSP0_CLKCTL_NUM_REGS];
} Cdsp0ClkctlState;

#endif /* HW_MISC_QDSP6_CC_SWI */
