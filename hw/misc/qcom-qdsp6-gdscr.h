/*
 * QCOM Turing GDSCR Registers — QEMU sysbus device
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef HW_MISC_QCOM_GDSCR_H
#define HW_MISC_QCOM_GDSCR_H

#include "qemu/osdep.h"
#include "hw/core/sysbus.h"

#define TYPE_QCOM_GDSCR "qcom-turing-gdscr"
OBJECT_DECLARE_SIMPLE_TYPE(QcomTuringGdscrState, QCOM_GDSCR)

/* ------------------------------------------------------------------------- */
#define GCC_0_GDSCR 0x00
#define GCC_0_CFG_GDSCR 0x04
#define GCC_0_CFG2_GDSCR 0x08
#define GCC_0_CFG3_GDSCR 0x0C
#define GCC_0_CFG4_GDSCR 0x10
#define GCC_0_Q6_TBU0_CBCR 0x14
#define GCC_0_Q6_TBU0_SREGR 0x18
#define GCC_0_Q6_TBU1_CBCR 0x1C
#define GCC_0_Q6_TBU1_SREGR 0x20
#define GCC_0_Q6_AXI_CBCR 0x24
#define GCC_0_CFG_AHB_CBCR 0x28
#define GCC_0_THROTTLE_NSP_AHB_CBCR 0x2C
#define GCC_0_AT_CBCR 0x30
#define GCC_0_TRIG_CBCR 0x34
#define GCC_0_Q6_AXI_CMD_RCGR 0x38
#define GCC_0_Q6_AXI_CFG_RCGR 0x3C
#define GCC_RPMH_CDSP_NOC0_CMD_DFSR 0x4C

/* Region size to cover highest offset (rounded up to page) */
#define QCOM_GDSCR_MMIO_SIZE 0x2000

/* ------------------------------------------------------------------------- */
/* Device state */
typedef struct QcomTuringGdscrState {
    SysBusDevice parent_obj;
    MemoryRegion mmio;

    uint32_t gdscr; /* 0x00 */
    uint32_t cfg_gdscr; /* 0x04 */
    uint32_t cfg2_gdscr; /* 0x08 */
    uint32_t cfg3_gdscr; /* 0x0C */
    uint32_t cfg4_gdscr; /* 0x10 */
    uint32_t q6_tbu0_cbcr; /* 0x14 */
    uint32_t q6_tbu0_sregr; /* 0x18 */
    uint32_t q6_tbu1_cbcr; /* 0x1C */
    uint32_t q6_tbu1_sregr; /* 0x20 */
    uint32_t q6_axi_cbcr; /* 0x24 */
    uint32_t cfg_ahb_cbcr; /* 0x28 */
    uint32_t throttle_nsp_ahb_cbcr; /* 0x2C */
    uint32_t at_cbcr; /* 0x30 */
    uint32_t trig_cbcr; /* 0x34 */
    uint32_t q6_axi_cmd_rcgr; /* 0x38 */
    uint32_t q6_axi_cfg_rcgr; /* 0x3C */
    uint32_t rpmh_cdhsp_noc0_cmd_dfsr; /* 0x4C */
} QcomTuringGdscrState;

#endif /* HW_MISC_QCOM_GDSCR_H */
