/*
 * Turing Clock Controller (TURING_CC) — QEMU sysbus device
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef HW_MISC_TURING_CC_REGS_H
#define HW_MISC_TURING_CC_REGS_H

#include "hw/core/sysbus.h"

#define TYPE_TURING_CC "turing-cc"
OBJECT_DECLARE_SIMPLE_TYPE(TuringCcState, TURING_CC)

#define TURING_CC_MMIO_SIZE 0x440
#define TURING_CC_NUM_REGS (TURING_CC_MMIO_SIZE / 4)

typedef struct TuringCcState {
    SysBusDevice parent_obj;
    MemoryRegion mmio;
    uint32_t regs[TURING_CC_NUM_REGS];
} TuringCcState;

#endif /* HW_MISC_TURING_CC_REGS_H */
