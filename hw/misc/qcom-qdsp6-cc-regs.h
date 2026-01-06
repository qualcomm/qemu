/*
 * TURING QDSP6SS Clock Controller (CLKCTL) — QEMU sysbus device
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef HW_MISC_QDSP6_CLKCTL_H
#define HW_MISC_QDSP6_CLKCTL_H

#include "hw/core/sysbus.h"

#define TYPE_QDSP6SS_CLKCTL "qdsp6ss-clkctl"
OBJECT_DECLARE_SIMPLE_TYPE(TuringQdsp6ClkctlState, QDSP6SS_CLKCTL)

/* ------------------------------------------------------------------------- */
/* MMIO layout: OFFSETS RELATIVE TO BASE 0x26348000                           */
/* ------------------------------------------------------------------------- */
/* Root/branch dividers & mux: */
#define CLKCTL_CORE_CMD_RCGR 0x00020
#define CMD_RCGR_UPDATE_MSK 0x1
#define CLKCTL_CORE_CFG_RCGR 0x00024

/* Branch clocks (Clock Branch Control Registers): */
#define CLKCTL_CORE_CBCR 0x00040
#define CLKCTL_SLPGEN_CBCR 0x00060
#define CLKCTL_L2MEM_SLPGEN_CBCR 0x00080
#define CLKCTL_L2ITCM_SLPGEN_CBCR 0x000A0
#define CLKCTL_L2VTCM_SLPGEN_CBCR 0x000C0
#define CLKCTL_MON_CBCR 0x00100
#define CLKCTL_ACD_XO_CBCR 0x00120
#define CLKCTL_DEBUG_CBCR 0x00148
#define CLKCTL_PLL_AHBS_CBCR 0x00168
#define CLKCTL_ACD_AHBS_CBCR 0x00188
#define CLKCTL_ACD_SCAN_CBCR 0x001C8
#define CLKCTL_SM_OBS_CBCR 0x001D4

/* Clock dividers (Clock Divider Registers): */
#define CLKCTL_MON_DIV_CDIVR 0x000E0
#define CLKCTL_DEBUG_DIV_CDIVR 0x00144
#define CLKCTL_ACD_SCAN_DIV_CDIVR 0x001A8
#define CLKCTL_SM_DIV_CDIVR 0x001D0

/* Region size to cover highest offset: */
#define QDSP6SS_CLKCTL_MMIO_SIZE 0x1000

/* ------------------------------------------------------------------------- */
/* Device state                                                              */
/* ------------------------------------------------------------------------- */
typedef struct TuringQdsp6ClkctlState {
    SysBusDevice parent_obj;
    MemoryRegion mmio;

    /* RCGR */
    uint32_t core_cmd_rcgr; /* 0x00020 */
    uint32_t core_cfg_rcgr; /* 0x00024 */

    /* CBCR */
    uint32_t core_cbcr; /* 0x00040 */
    uint32_t slpgen_cbcr; /* 0x00060 */
    uint32_t l2mem_slpgen_cbcr; /* 0x00080 */
    uint32_t l2itcm_slpgen_cbcr; /* 0x000A0 */
    uint32_t l2vtcm_slpgen_cbcr; /* 0x000C0 */
    uint32_t mon_cbcr; /* 0x00100 */
    uint32_t acd_xo_cbcr; /* 0x00120 */
    uint32_t debug_cbcr; /* 0x00148 */
    uint32_t pll_ahbs_cbcr; /* 0x00168 */
    uint32_t acd_ahbs_cbcr; /* 0x00188 */
    uint32_t acd_scan_cbcr; /* 0x001C8 */
    uint32_t sm_obs_cbcr; /* 0x001D4 */

    /* CDIVR */
    uint32_t mon_div_cdivr; /* 0x000E0 */
    uint32_t debug_div_cdivr; /* 0x00144 */
    uint32_t acd_scan_div_cdivr; /* 0x001A8 */
    uint32_t sm_div_cdivr; /* 0x001D0 */

    /* MUX */
    uint32_t debug_mux_muxr; /* 0x22004 */

    /* STATUS/DIAG */
    uint32_t parity_status_reg; /* 0x33000 (RO) */
    uint32_t fusa_status_register; /* 0x44000 */
} TuringQdsp6ClkctlState;

#endif /* HW_MISC_QDSP6_CLKCTL_H */
