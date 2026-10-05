
/*
 * TURING QDSP6SS PLL — simple QEMU sysbus device
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef HW_MISC_QOCM_TURING_QDSPSS_PLL_H
#define HW_MISC_QOCM_TURING_QDSPSS_PLL_H

#include "hw/core/sysbus.h"
#include "qemu/typedefs.h"

#define TYPE_TURING_QDSP6SS_PLL "turing-qdsp6ss-pll"
OBJECT_DECLARE_SIMPLE_TYPE(TuringQdsp6PllState, TURING_QDSP6SS_PLL)

/* Device state */
typedef struct TuringQdsp6PllState {
    SysBusDevice parent_obj;
    MemoryRegion mmio;

    uint32_t values[11];
} TuringQdsp6PllState;

#endif /* HW_MISC_QOCM_TURING_QDSPSS_PLL_H */
