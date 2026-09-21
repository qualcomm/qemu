
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

    /* 32-bit registers */
    uint32_t pll_mode;
    uint32_t pll_opmode;
    uint32_t pll_state; /* read-only */
    uint32_t pll_status; /* read-only */
    uint32_t pll_l_val;
    uint32_t pll_alpha_val;
    uint32_t pll_user_ctl;
    uint32_t pll_user_ctl_u;
    uint32_t pll_config_ctl;
    uint32_t pll_config_ctl_u;
    uint32_t pll_config_ctl_u1;
    uint32_t pll_ssc;
    uint32_t fusa_status_register;
} TuringQdsp6PllState;

#endif /* HW_MISC_QOCM_TURING_QDSPSS_PLL_H */
