/*
 * CDSP PLL device
 *
 * Copyright(c) 2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef HW_MISC_CDSP_PLL_H
#define HW_MISC_CDSP_PLL_H

#include "hw/core/sysbus.h"
#include "qom/object.h"

#define TYPE_CDSP_PLL "cdsp-pll"
OBJECT_DECLARE_SIMPLE_TYPE(CdspPLLState, CDSP_PLL)

/* PLL register offsets */
#define CDSP_PLL_MODE                0x200
#define CDSP_PLL_L_VAL               0x204
#define CDSP_PLL_CAL_L_VAL           0x208
#define CDSP_PLL_USER_CTL            0x20c
#define CDSP_PLL_USER_CTL_U          0x210
#define CDSP_PLL_CONFIG_CTL          0x214
#define CDSP_PLL_CONFIG_CTL_U        0x218
#define CDSP_PLL_TEST_CTL            0x21c
#define CDSP_PLL_TEST_CTL_U          0x220
#define CDSP_PLL_STATUS              0x224
#define CDSP_PLL_FREQ_CTL            0x228
#define CDSP_PLL_OPMODE              0x238
#define CDSP_PLL_STATE               0x23c
#define CDSP_PLL_DROOP               0x234
#define CDSP_PLL_SPARE               0x23c
#define CDSP_PLL_SSC_DELTA_ALPHA     0x240
#define CDSP_PLL_SSC_UPDATE_RATE     0x244
#define CDSP_PLL_SSC_NUM_STEPS       0x248
#define CDSP_PLL_RCG_UPDATE_STATUS   0x250
#define CDSP_PLL_RCG_UPDATE_CFG      0x254
#define CDSP_PLL_RCG_UPDATE_DLYCTL   0x258
#define CDSP_PLL_RCG_UPDATE_CMD      0x25c
#define CDSP_PLL_CORE_CFG_RCGR       0x02c
#define CDSP_PLL_CORE_CMD_RCGR       0x028

/* PLL register count */
#define CDSP_PLL_NUM_REGS            24

/* PLL mode register bit definitions */
#define CDSP_PLL_MODE_OUTCTRL        (1 << 0)
#define CDSP_PLL_MODE_BYPASSNL       (1 << 1)
#define CDSP_PLL_MODE_RESET_N        (1 << 2)
#define CDSP_PLL_MODE_LOCK_DET       (1 << 31)
#define CDSP_PLL_MODE_UPDATE         (1 << 22)
#define CDSP_PLL_MODE_ACK_LATCH      (1 << 29)

/* PLL user control register bit definitions */
#define CDSP_PLL_USER_CTL_PLLOUT_MAIN    (1 << 0)
#define CDSP_PLL_USER_CTL_U_LATCH_BYPASS (1 << 10)
#define CDSP_PLL_USER_CTL_U_STATE_WRITE  (1 << 4)

/* PLL operation modes */
#define CDSP_PLL_OPMODE_STANDBY      0
#define CDSP_PLL_OPMODE_RUN          1

/* PLL configuration values */
#define HAL_CLK_UPDATED_CONFIG_CTL_VAL      0x20485699
#define HAL_CLK_UPDATED_CONFIG_CTL_U_VAL    0x00002067
#define HAL_CLK_UPDATED_TEST_CTL_VAL        0x40000000
#define HAL_CLK_UPDATED_TEST_CTL_U_VAL      0x0
#define HAL_CLK_UPDATED_USER_CTL_U_VAL      0x4804

struct CdspPLLState {
    SysBusDevice parent_obj;

    MemoryRegion iomem;
    uint32_t regs[CDSP_PLL_NUM_REGS];

    /* Configuration properties */
    uint32_t base_freq;
    uint32_t default_l_val;

    /* Internal state */
    bool pll_locked;
    bool outputs_enabled;
};

#endif
