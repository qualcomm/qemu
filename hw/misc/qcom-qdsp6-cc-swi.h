/*
 * CDSP0 Clock Controller (CLKCTL) — QEMU sysbus device
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef HW_MISC_QDSP6_CC_SWI
#define HW_MISC_QDSP6_CC_SWI

#include "hw/core/sysbus.h"

#define TYPE_CDSP0_CLKCTL "cdsp0-clkctl"
OBJECT_DECLARE_SIMPLE_TYPE(Cdsp0ClkctlState, CDSP0_CLKCTL)

/* MMIO size large enough to cover highest offset 0x1024 */
#define CDSP0_CLKCTL_MMIO_SIZE 0x20000

/* ------------------------------------------------------------------------- */
/* Offsets (from the table, kept verbatim)                                   */
/* ------------------------------------------------------------------------- */
#define CDSP0_AON_CMD_RCGR 0x0000
#define CDSP0_AON_CFG_RCGR 0x0004
#define CDSP0_AON_DCD_CDIV_DCDR 0x0008
#define CDSP0_TURING_WRAPPER_AON_CBCR 0x000C
#define CDSP0_TURING_WRAPPER_CNOC_SWAY_AON_CBCR 0x0010
#define CDSP0_TURING_WRAPPER_BUS_TIMEOUT_AON_CBCR 0x0014
#define CDSP0_TURING_WRAPPER_RSCC_AON_CBCR 0x0018

#define CDSP0_AUX_XO_CBCR 0x0040
#define CDSP0_AUX_ACCU_XO_CBCR 0x0044
#define CDSP0_VAPSS_XO_CBCR 0x0048
#define CDSP0_VAPSS_ACCU_XO_CBCR 0x004C
#define CDSP0_XO_CBCR 0x0050
#define CDSP0_XO_CDIV_CDIVR 0x0054
#define CDSP0_XO_CDIV_CBCR 0x0058

#define CDSP0_VAPSS_GDSCR 0x0080
#define CDSP0_VAPSS_CFG_GDSCR 0x0084
#define CDSP0_VAPSS_CFG2_GDSCR 0x0088
#define CDSP0_VAPSS_CFG3_GDSCR 0x008C
#define CDSP0_VAPSS_CFG4_GDSCR 0x0090
#define CDSP0_VAPSS_CORE_BCR 0x0094
#define CDSP0_VAPSS_DMA_CMD_RCGR 0x0098
#define CDSP0_VAPSS_DMA_CFG_RCGR 0x009C
#define CDSP0_VAPSS_DMA_DCD_CDIV_DCDR 0x00A0
#define CDSP0_VAPSS_DMA_CBCR 0x00A4
#define CDSP0_VAPSS_DMA_SREGR 0x00A8

#define CDSP0_VAPSS_VMA_CMD_RCGR 0x00B0
#define CDSP0_VAPSS_VMA_CFG_RCGR 0x00B4
#define CDSP0_VAPSS_VMA_DCD_CDIV_DCDR 0x00B8
#define CDSP0_VAPSS_VMA_CBCR 0x00BC
#define CDSP0_VAPSS_VMA_SREGR 0x00C0

#define CDSP0_VAPSS_TCMS_CMD_RCGR 0x00C8
#define CDSP0_VAPSS_TCMS_CFG_RCGR 0x00CC
#define CDSP0_VAPSS_TCMS_DCD_CDIV_DCDR 0x00D0
#define CDSP0_VAPSS_TCMS_CBCR 0x00D4
#define CDSP0_VAPSS_TCMS_SREGR 0x00D8

#define CDSP0_VAPSS_HCP_CMD_RCGR 0x00E0
#define CDSP0_VAPSS_HCP_CFG_RCGR 0x00E4
#define CDSP0_VAPSS_HCP_DCD_CDIV_DCDR 0x00E8
#define CDSP0_VAPSS_HCP_CBCR 0x00EC
#define CDSP0_VAPSS_HCP_SREGR 0x00F0
#define CDSP0_VAPSS_HCP0_CBCR 0x00F8

#define CDSP0_VAPSS_BUS_CMD_RCGR 0x00FC
#define CDSP0_VAPSS_BUS_CFG_RCGR 0x0100
#define CDSP0_VAPSS_Q6_AXI_DCD_CDIV_DCDR 0x0104
#define CDSP0_VAPSS_BUS_CBCR 0x0108
#define CDSP0_VAPSS_BUS_SREGR 0x010C

#define CDSP0_VAPSS_FINT_CMD_RCGR 0x0114
#define CDSP0_VAPSS_FINT_CFG_RCGR 0x0118
#define CDSP0_VAPSS_FINT_DCD_CDIV_DCDR 0x011C
#define CDSP0_VAPSS_FINT_CBCR 0x0120
#define CDSP0_VAPSS_FINT_SREGR 0x0124
#define CDSP0_VAPSS_FINT_PROG_CBCR 0x012C
#define CDSP0_VAPSS_AHBS_TIMEOUT_CBCR 0x0130
#define CDSP0_VAPSS_DMA_AHBS_CBCR 0x0134
#define CDSP0_VAPSS_VMA_AHBS_CBCR 0x0138
#define CDSP0_VAPSS_HCP_AHBS_CBCR 0x013C
#define CDSP0_VAPSS_AHBS_AON_CBCR 0x0140
#define CDSP0_VAPSS_TBUF2_AHBS_CBCR 0x0144

#define CDSP0_VAPSS_ATB_CBCR 0x0150
#define CDSP0_VAPSS_APB_CBCR 0x0154

#define CDSP0_VAPSS_TBUF2_CMD_RCGR 0x0180
#define CDSP0_VAPSS_TBUF2_CFG_RCGR 0x0184
#define CDSP0_VAPSS_TBUF2_CBCR 0x018C
#define CDSP0_VAPSS_TBUF2_SREGR 0x0190
#define CDSP0_NOC_TBUF2_CBCR 0x0198

#define CDSP0_AUX_GDSCR 0x0200
#define CDSP0_AUX_CFG_GDSCR 0x0204
#define CDSP0_AUX_CFG2_GDSCR 0x0208
#define CDSP0_AUX_CFG3_GDSCR 0x020C
#define CDSP0_AUX_CFG4_GDSCR 0x0210
#define CDSP0_AUX_CORE_BCR 0x0214
#define CDSP0_CENG_AHBS_CBCR 0x0218
#define CDSP0_NOC_AHBS_CBCR 0x021C
#define CDSP0_CENG_CBCR 0x0220
#define CDSP0_CENG_SREGR 0x0224
#define CDSP0_NOC_CBCR 0x022C
#define CDSP0_NOC_SREGR 0x0230
#define CDSP0_NOC_VAPSS_BUS_CBCR 0x0238
#define CDSP0_NOC_ATB_CBCR 0x023C
#define CDSP0_NOC_APB_CBCR 0x0240

#define CDSP0_Q6SS_BCR 0x0400
#define CDSP0_Q6SS_Q6_AXIM_CBCR 0x0404
#define CDSP0_Q6SS_AXIS2_CBCR 0x040C
#define CDSP0_Q6SS_AHBM_AON_CBCR 0x0410
#define CDSP0_Q6SS_AHBS_AON_CBCR 0x0414
#define CDSP0_Q6SS_ALT_RESET_AON_CBCR 0x0418
#define CDSP0_Q6SS_LMH_CMD_RCGR 0x041C
#define CDSP0_Q6SS_LMH_CFG_RCGR 0x0420
#define CDSP0_Q6SS_LMH_CBCR 0x0424
#define CDSP0_Q6SS_ISENSE_CMD_RCGR 0x0428
#define CDSP0_Q6SS_ISENSE_CFG_RCGR 0x042C
#define CDSP0_Q6SS_ISENSE_CTRL_CBCR 0x0430
#define CDSP0_Q6SS_ISENSE_CORE_CBCR 0x0434
#define CDSP0_Q6SS_LLM_TEMP_SSC_CBCR 0x0438
#define CDSP0_Q6SS_LLM_CURR_SSC_CBCR 0x043C

#define CDSP0_VAPSS_BUS_DCD_CDIV_DCDR 0x0618
#define CDSP0_VAPSS_TBUF2_DCD_CDIV_DCDR 0x061C

#define CDSP0_DEBUG_MUX_MUXR 0x1000
#define CDSP0_DEBUG_DIV_CDIVR 0x1004
#define CDSP0_DEBUG_CBCR 0x1008
#define CDSP0_PLL_TEST_MUX_MUXR 0x100C
#define CDSP0_PLL_STATUS_MUXR 0x1010
#define CDSP0_PLL_TEST_DIV_CDIVR 0x1014
#define CDSP0_PLL_TEST_CBCR 0x1018
#define CDSP0_SM_DEBUG_DIV_CDIVR 0x1020
#define CDSP0_SM_DEBUG_CBCR 0x1024

#define CDSP0_VAPSS_GDS_HW_CTRL 0x10000
#define CDSP0_VAPSS_GDS_HW_STATUS 0x10004
#define CDSP0_GDS_HW_CTRL_SEQUENCE_ABORT_IRQ_STATUS_Q6 0x10008
#define CDSP0_GDS_HW_CTRL_SEQUENCE_ABORT_IRQ_ENABLE_Q6 0x1000C
#define CDSP0_GDS_HW_CTRL_SEQUENCE_ABORT_IRQ_STATUS_APPS 0x10010
#define CDSP0_GDS_HW_CTRL_SEQUENCE_ABORT_IRQ_ENABLE_APPS 0x10014
#define CDSP0_TURING_WRAPPER_RSCC_BR_EVENT 0x10018
#define CDSP0_TURING_WRAPPER_RSCC_BR_EVENT_OVERRIDE_MASK 0x1001C
#define CDSP0_TURING_WRAPPER_RSCC_BR_EVENT_OVERRIDE_VAL 0x10020
#define CDSP0_TURING_WRAPPER_RSCC_PWR_CTRL_OVERRIDE_MASK 0x10024
#define CDSP0_TURING_WRAPPER_RSCC_PWR_CTRL_OVERRIDE_VAL 0x10028
#define CDSP0_TURING_WRAPPER_RSCC_WAIT_EVENT_OVERRIDE_MASK 0x1002C
#define CDSP0_TURING_WRAPPER_RSCC_WAIT_EVENT_OVERRIDE_VAL 0x10030
#define CDSP0_Q6SS_ALT_RESET_CTL 0x10034
#define CDSP0_Q6_AXIM_CLKON_HW_EN 0x1003C
#define CDSP0_SPARE_CTRL 0x13FF8
#define CDSP0_SPARE_STATUS 0x13FFC

/* ------------------------------------------------------------------------- */
/* Device state: one 32-bit storage per register                             */
/* ------------------------------------------------------------------------- */
typedef struct Cdsp0ClkctlState {
    SysBusDevice parent_obj;
    MemoryRegion mmio;

    /* 0x0000.. */
    uint32_t aon_cmd_rcgr;
    uint32_t aon_cfg_rcgr;
    uint32_t aon_dcd_cdiv_dcdr;
    uint32_t turing_wrapper_aon_cbcr;
    uint32_t turing_wrapper_cnoc_sway_aon_cbcr;
    uint32_t turing_wrapper_bus_timeout_aon_cbcr;
    uint32_t turing_wrapper_rscc_aon_cbcr;

    /* 0x0040..0x0058 */
    uint32_t aux_xo_cbcr;
    uint32_t aux_accu_xo_cbcr;
    uint32_t vapss_xo_cbcr;
    uint32_t vapss_accu_xo_cbcr;
    uint32_t xo_cbcr;
    uint32_t xo_cdiv_cdivr;
    uint32_t xo_cdiv_cbcr;

    /* 0x0080..0x00A8 */
    uint32_t vapss_gdscr;
    uint32_t vapss_cfg_gdscr;
    uint32_t vapss_cfg2_gdscr;
    uint32_t vapss_cfg3_gdscr;
    uint32_t vapss_cfg4_gdscr;
    uint32_t vapss_core_bcr;
    uint32_t vapss_dma_cmd_rcgr;
    uint32_t vapss_dma_cfg_rcgr;
    uint32_t vapss_dma_dcd_cdiv_dcdr;
    uint32_t vapss_dma_cbcr;
    uint32_t vapss_dma_sregr;

    /* 0x00B0..0x00C0 */
    uint32_t vapss_vma_cmd_rcgr;
    uint32_t vapss_vma_cfg_rcgr;
    uint32_t vapss_vma_dcd_cdiv_dcdr;
    uint32_t vapss_vma_cbcr;
    uint32_t vapss_vma_sregr;

    /* 0x00C8..0x00D8 */
    uint32_t vapss_tcms_cmd_rcgr;
    uint32_t vapss_tcms_cfg_rcgr;
    uint32_t vapss_tcms_dcd_cdiv_dcdr;
    uint32_t vapss_tcms_cbcr;
    uint32_t vapss_tcms_sregr;

    /* 0x00E0..0x00F8 */
    uint32_t vapss_hcp_cmd_rcgr;
    uint32_t vapss_hcp_cfg_rcgr;
    uint32_t vapss_hcp_dcd_cdiv_dcdr;
    uint32_t vapss_hcp_cbcr;
    uint32_t vapss_hcp_sregr;
    uint32_t vapss_hcp0_cbcr;

    /* 0x00FC..0x010C */
    uint32_t vapss_bus_cmd_rcgr;
    uint32_t vapss_bus_cfg_rcgr;
    uint32_t vapss_q6_axi_dcd_cdiv_dcdr;
    uint32_t vapss_bus_cbcr;
    uint32_t vapss_bus_sregr;

    /* 0x0114..0x0144 */
    uint32_t vapss_fint_cmd_rcgr;
    uint32_t vapss_fint_cfg_rcgr;
    uint32_t vapss_fint_dcd_cdiv_dcdr;
    uint32_t vapss_fint_cbcr;
    uint32_t vapss_fint_sregr;
    uint32_t vapss_fint_prog_cbcr;
    uint32_t vapss_ahbs_timeout_cbcr;
    uint32_t vapss_dma_ahbs_cbcr;
    uint32_t vapss_vma_ahbs_cbcr;
    uint32_t vapss_hcp_ahbs_cbcr;
    uint32_t vapss_ahbs_aon_cbcr;
    uint32_t vapss_tbuf2_ahbs_cbcr;

    /* 0x0150..0x0154 */
    uint32_t vapss_atb_cbcr;
    uint32_t vapss_apb_cbcr;

    /* 0x0180..0x0198 */
    uint32_t vapss_tbuf2_cmd_rcgr;
    uint32_t vapss_tbuf2_cfg_rcgr;
    uint32_t vapss_tbuf2_cbcr;
    uint32_t vapss_tbuf2_sregr;
    uint32_t noc_tbuf2_cbcr;

    /* 0x0200..0x0240 */
    uint32_t aux_gdscr;
    uint32_t aux_cfg_gdscr;
    uint32_t aux_cfg2_gdscr;
    uint32_t aux_cfg3_gdscr;
    uint32_t aux_cfg4_gdscr;
    uint32_t aux_core_bcr;
    uint32_t ceng_ahbs_cbcr;
    uint32_t noc_ahbs_cbcr;
    uint32_t ceng_cbcr;
    uint32_t ceng_sregr;
    uint32_t noc_cbcr;
    uint32_t noc_sregr;
    uint32_t noc_vapss_bus_cbcr;
    uint32_t noc_atb_cbcr;
    uint32_t noc_apb_cbcr;

    /* 0x0400..0x043C */
    uint32_t q6ss_bcr;
    uint32_t q6ss_q6_axim_cbcr;
    uint32_t q6ss_axis2_cbcr;
    uint32_t q6ss_ahbm_aon_cbcr;
    uint32_t q6ss_ahbs_aon_cbcr;
    uint32_t q6ss_alt_reset_aon_cbcr;
    uint32_t q6ss_lmh_cmd_rcgr;
    uint32_t q6ss_lmh_cfg_rcgr;
    uint32_t q6ss_lmh_cbcr;
    uint32_t q6ss_isense_cmd_rcgr;
    uint32_t q6ss_isense_cfg_rcgr;
    uint32_t q6ss_isense_ctrl_cbcr;
    uint32_t q6ss_isense_core_cbcr;
    uint32_t q6ss_llm_temp_ssc_cbcr;
    uint32_t q6ss_llm_curr_ssc_cbcr;

    /* 0x0618..0x061C */
    uint32_t vapss_bus_dcd_cdiv_dcdr;
    uint32_t vapss_tbuf2_dcd_cdiv_dcdr;

    /* 0x1000..0x1024 */
    uint32_t debug_mux_muxr;
    uint32_t debug_div_cdivr;
    uint32_t debug_cbcr;
    uint32_t pll_test_mux_muxr;
    uint32_t pll_status_muxr;
    uint32_t pll_test_div_cdivr;
    uint32_t pll_test_cbcr;
    uint32_t sm_debug_div_cdivr;
    uint32_t sm_debug_cbcr;

    uint32_t vapss_gds_hw_ctrl;
    uint32_t vapss_gds_hw_status;
    uint32_t gds_hw_ctrl_sequence_abort_irq_status_q6;
    uint32_t gds_hw_ctrl_sequence_abort_irq_enable_q6;
    uint32_t gds_hw_ctrl_sequence_abort_irq_status_apps;
    uint32_t gds_hw_ctrl_sequence_abort_irq_enable_apps;
    uint32_t turing_wrapper_rscc_br_event;
    uint32_t turing_wrapper_rscc_br_event_override_mask;
    uint32_t turing_wrapper_rscc_br_event_override_val;
    uint32_t turing_wrapper_rscc_pwr_ctrl_override_mask;
    uint32_t turing_wrapper_rscc_pwr_ctrl_override_val;
    uint32_t turing_wrapper_rscc_wait_event_override_mask;
    uint32_t turing_wrapper_rscc_wait_event_override_val;
    uint32_t q6ss_alt_reset_ctl;
    uint32_t q6_axim_clkon_hw_en;
    uint32_t spare_ctrl;
    uint32_t spare_status;

} Cdsp0ClkctlState;

#endif /* HW_MISC_QDSP6_CC_SWI */
