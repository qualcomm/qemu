/*
 * TURING QDSP6SS Clock Controller (CLKCTL) — QEMU sysbus device
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef HW_MISC_QDSP6_CLKCTL_H
#define HW_MISC_QDSP6_CLKCTL_H

#include "hw/core/sysbus.h"

#define TYPE_QDSP6SS_CLKCTL "qdsp6ss-clkctl"
OBJECT_DECLARE_SIMPLE_TYPE(TuringQdsp6ClkctlState, QDSP6SS_CLKCTL)

#define QDSP6SS_CLKCTL_MMIO_SIZE 0x1ac

/* ------------------------------------------------------------------------- */
/* Device state                                                              */
/* ------------------------------------------------------------------------- */
typedef struct TuringQdsp6ClkctlState {
    SysBusDevice parent_obj;
    MemoryRegion mmio;

    uint32_t values[6];
} TuringQdsp6ClkctlState;

#endif /* HW_MISC_QDSP6_CLKCTL_H */
