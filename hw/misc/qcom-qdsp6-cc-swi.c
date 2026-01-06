/*
 * CDSP0 Clock Controller SWI (CLKCTL) — QEMU sysbus device
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "hw/misc/qcom-qdsp6-cc-swi.h"
#include "migration/vmstate.h"
#include "qemu/log.h"

/* Log helper */
static inline void cdsp0_clkctl_bad(const char *dir, hwaddr addr, unsigned size)
{
    qemu_log_mask(LOG_GUEST_ERROR,
                  TYPE_CDSP0_CLKCTL ": bad %s addr=0x%" HWADDR_PRIx
                                    " size=%u\n",
                  dir, addr, size);
}
static uint64_t cdsp0_clkctl_read(void *opaque, hwaddr addr, unsigned size)
{
    Cdsp0ClkctlState *s = opaque;
    if (size != 4) {
        cdsp0_clkctl_bad("read", addr, size);
        return 0;
    }
    switch (addr) {
    case CDSP0_AON_CMD_RCGR:
        if (s->aon_cmd_rcgr & 0x1) {
            /* Simulate a clock update */
            s->aon_cmd_rcgr &= ~0x1;
        }
        return s->aon_cmd_rcgr;
    case CDSP0_AON_CFG_RCGR:
        return s->aon_cfg_rcgr;
    case CDSP0_AON_DCD_CDIV_DCDR:
        return s->aon_dcd_cdiv_dcdr;
    case CDSP0_TURING_WRAPPER_AON_CBCR:
        return s->turing_wrapper_aon_cbcr;
    case CDSP0_TURING_WRAPPER_CNOC_SWAY_AON_CBCR:
        return s->turing_wrapper_cnoc_sway_aon_cbcr;
    case CDSP0_TURING_WRAPPER_BUS_TIMEOUT_AON_CBCR:
        return s->turing_wrapper_bus_timeout_aon_cbcr;
    case CDSP0_TURING_WRAPPER_RSCC_AON_CBCR:
        return s->turing_wrapper_rscc_aon_cbcr;
    case CDSP0_AUX_XO_CBCR:
        return s->aux_xo_cbcr;
    case CDSP0_AUX_ACCU_XO_CBCR:
        return s->aux_accu_xo_cbcr;
    case CDSP0_VAPSS_XO_CBCR:
        return s->vapss_xo_cbcr;
    case CDSP0_VAPSS_ACCU_XO_CBCR:
        return s->vapss_accu_xo_cbcr;
    case CDSP0_XO_CBCR:
        return s->xo_cbcr;
    case CDSP0_XO_CDIV_CDIVR:
        return s->xo_cdiv_cdivr;
    case CDSP0_XO_CDIV_CBCR:
        return s->xo_cdiv_cbcr;
    case CDSP0_VAPSS_GDSCR:
        return s->vapss_gdscr;
    case CDSP0_VAPSS_CFG_GDSCR:
        return s->vapss_cfg_gdscr;
    case CDSP0_VAPSS_CFG2_GDSCR:
        return s->vapss_cfg2_gdscr;
    case CDSP0_VAPSS_CFG3_GDSCR:
        return s->vapss_cfg3_gdscr;
    case CDSP0_VAPSS_CFG4_GDSCR:
        return s->vapss_cfg4_gdscr;
    case CDSP0_VAPSS_CORE_BCR:
        return s->vapss_core_bcr;
    case CDSP0_VAPSS_DMA_CMD_RCGR:
        return s->vapss_dma_cmd_rcgr;
    case CDSP0_VAPSS_DMA_CFG_RCGR:
        return s->vapss_dma_cfg_rcgr;
    case CDSP0_VAPSS_DMA_DCD_CDIV_DCDR:
        return s->vapss_dma_dcd_cdiv_dcdr;
    case CDSP0_VAPSS_DMA_CBCR:
        return s->vapss_dma_cbcr;
    case CDSP0_VAPSS_DMA_SREGR:
        return s->vapss_dma_sregr;
    case CDSP0_VAPSS_VMA_CMD_RCGR:
        return s->vapss_vma_cmd_rcgr;
    case CDSP0_VAPSS_VMA_CFG_RCGR:
        return s->vapss_vma_cfg_rcgr;
    case CDSP0_VAPSS_VMA_DCD_CDIV_DCDR:
        return s->vapss_vma_dcd_cdiv_dcdr;
    case CDSP0_VAPSS_VMA_CBCR:
        return s->vapss_vma_cbcr;
    case CDSP0_VAPSS_VMA_SREGR:
        return s->vapss_vma_sregr;
    case CDSP0_VAPSS_TCMS_CMD_RCGR:
        return s->vapss_tcms_cmd_rcgr;
    case CDSP0_VAPSS_TCMS_CFG_RCGR:
        return s->vapss_tcms_cfg_rcgr;
    case CDSP0_VAPSS_TCMS_DCD_CDIV_DCDR:
        return s->vapss_tcms_dcd_cdiv_dcdr;
    case CDSP0_VAPSS_TCMS_CBCR:
        return s->vapss_tcms_cbcr;
    case CDSP0_VAPSS_TCMS_SREGR:
        return s->vapss_tcms_sregr;
    case CDSP0_VAPSS_HCP_CMD_RCGR:
        return s->vapss_hcp_cmd_rcgr;
    case CDSP0_VAPSS_HCP_CFG_RCGR:
        return s->vapss_hcp_cfg_rcgr;
    case CDSP0_VAPSS_HCP_DCD_CDIV_DCDR:
        return s->vapss_hcp_dcd_cdiv_dcdr;
    case CDSP0_VAPSS_HCP_CBCR:
        return s->vapss_hcp_cbcr;
    case CDSP0_VAPSS_HCP_SREGR:
        return s->vapss_hcp_sregr;
    case CDSP0_VAPSS_HCP0_CBCR:
        return s->vapss_hcp0_cbcr;
    case CDSP0_VAPSS_BUS_CMD_RCGR:
        return s->vapss_bus_cmd_rcgr;
    case CDSP0_VAPSS_BUS_CFG_RCGR:
        return s->vapss_bus_cfg_rcgr;
    case CDSP0_VAPSS_Q6_AXI_DCD_CDIV_DCDR:
        return s->vapss_q6_axi_dcd_cdiv_dcdr;
    case CDSP0_VAPSS_BUS_CBCR:
        return s->vapss_bus_cbcr;
    case CDSP0_VAPSS_BUS_SREGR:
        return s->vapss_bus_sregr;
    case CDSP0_VAPSS_FINT_CMD_RCGR:
        return s->vapss_fint_cmd_rcgr;
    case CDSP0_VAPSS_FINT_CFG_RCGR:
        return s->vapss_fint_cfg_rcgr;
    case CDSP0_VAPSS_FINT_DCD_CDIV_DCDR:
        return s->vapss_fint_dcd_cdiv_dcdr;
    case CDSP0_VAPSS_FINT_CBCR:
        return s->vapss_fint_cbcr;
    case CDSP0_VAPSS_FINT_SREGR:
        return s->vapss_fint_sregr;
    case CDSP0_VAPSS_FINT_PROG_CBCR:
        return s->vapss_fint_prog_cbcr;
    case CDSP0_VAPSS_AHBS_TIMEOUT_CBCR:
        return s->vapss_ahbs_timeout_cbcr;
    case CDSP0_VAPSS_DMA_AHBS_CBCR:
        return s->vapss_dma_ahbs_cbcr;
    case CDSP0_VAPSS_VMA_AHBS_CBCR:
        return s->vapss_vma_ahbs_cbcr;
    case CDSP0_VAPSS_HCP_AHBS_CBCR:
        return s->vapss_hcp_ahbs_cbcr;
    case CDSP0_VAPSS_AHBS_AON_CBCR:
        return s->vapss_ahbs_aon_cbcr;
    case CDSP0_VAPSS_TBUF2_AHBS_CBCR:
        return s->vapss_tbuf2_ahbs_cbcr;
    case CDSP0_VAPSS_ATB_CBCR:
        return s->vapss_atb_cbcr;
    case CDSP0_VAPSS_APB_CBCR:
        return s->vapss_apb_cbcr;
    case CDSP0_VAPSS_TBUF2_CMD_RCGR:
        return s->vapss_tbuf2_cmd_rcgr;
    case CDSP0_VAPSS_TBUF2_CFG_RCGR:
        return s->vapss_tbuf2_cfg_rcgr;
    case CDSP0_VAPSS_TBUF2_CBCR:
        return s->vapss_tbuf2_cbcr;
    case CDSP0_VAPSS_TBUF2_SREGR:
        return s->vapss_tbuf2_sregr;
    case CDSP0_NOC_TBUF2_CBCR:
        return s->noc_tbuf2_cbcr;
    case CDSP0_AUX_GDSCR:
        return s->aux_gdscr;
    case CDSP0_AUX_CFG_GDSCR:
        return s->aux_cfg_gdscr;
    case CDSP0_AUX_CFG2_GDSCR:
        return s->aux_cfg2_gdscr;
    case CDSP0_AUX_CFG3_GDSCR:
        return s->aux_cfg3_gdscr;
    case CDSP0_AUX_CFG4_GDSCR:
        return s->aux_cfg4_gdscr;
    case CDSP0_AUX_CORE_BCR:
        return s->aux_core_bcr;
    case CDSP0_CENG_AHBS_CBCR:
        return s->ceng_ahbs_cbcr;
    case CDSP0_NOC_AHBS_CBCR:
        return s->noc_ahbs_cbcr;
    case CDSP0_CENG_CBCR:
        return s->ceng_cbcr;
    case CDSP0_CENG_SREGR:
        return s->ceng_sregr;
    case CDSP0_NOC_CBCR:
        return s->noc_cbcr;
    case CDSP0_NOC_SREGR:
        return s->noc_sregr;
    case CDSP0_NOC_VAPSS_BUS_CBCR:
        return s->noc_vapss_bus_cbcr;
    case CDSP0_NOC_ATB_CBCR:
        return s->noc_atb_cbcr;
    case CDSP0_NOC_APB_CBCR:
        return s->noc_apb_cbcr;
    case CDSP0_Q6SS_BCR:
        return s->q6ss_bcr;
    case CDSP0_Q6SS_Q6_AXIM_CBCR:
        return s->q6ss_q6_axim_cbcr;
    case CDSP0_Q6SS_AXIS2_CBCR:
        return s->q6ss_axis2_cbcr;
    case CDSP0_Q6SS_AHBM_AON_CBCR:
        return s->q6ss_ahbm_aon_cbcr;
    case CDSP0_Q6SS_AHBS_AON_CBCR:
        return s->q6ss_ahbs_aon_cbcr;
    case CDSP0_Q6SS_ALT_RESET_AON_CBCR:
        return s->q6ss_alt_reset_aon_cbcr;
    case CDSP0_Q6SS_LMH_CMD_RCGR:
        return s->q6ss_lmh_cmd_rcgr;
    case CDSP0_Q6SS_LMH_CFG_RCGR:
        return s->q6ss_lmh_cfg_rcgr;
    case CDSP0_Q6SS_LMH_CBCR:
        return s->q6ss_lmh_cbcr;
    case CDSP0_Q6SS_ISENSE_CMD_RCGR:
        return s->q6ss_isense_cmd_rcgr;
    case CDSP0_Q6SS_ISENSE_CFG_RCGR:
        return s->q6ss_isense_cfg_rcgr;
    case CDSP0_Q6SS_ISENSE_CTRL_CBCR:
        return s->q6ss_isense_ctrl_cbcr;
    case CDSP0_Q6SS_ISENSE_CORE_CBCR:
        return s->q6ss_isense_core_cbcr;
    case CDSP0_Q6SS_LLM_TEMP_SSC_CBCR:
        return s->q6ss_llm_temp_ssc_cbcr;
    case CDSP0_Q6SS_LLM_CURR_SSC_CBCR:
        return s->q6ss_llm_curr_ssc_cbcr;
    case CDSP0_VAPSS_BUS_DCD_CDIV_DCDR:
        return s->vapss_bus_dcd_cdiv_dcdr;
    case CDSP0_VAPSS_TBUF2_DCD_CDIV_DCDR:
        return s->vapss_tbuf2_dcd_cdiv_dcdr;
    case CDSP0_DEBUG_MUX_MUXR:
        return s->debug_mux_muxr;
    case CDSP0_DEBUG_DIV_CDIVR:
        return s->debug_div_cdivr;
    case CDSP0_DEBUG_CBCR:
        return s->debug_cbcr;
    case CDSP0_PLL_TEST_MUX_MUXR:
        return s->pll_test_mux_muxr;
    case CDSP0_PLL_STATUS_MUXR:
        return s->pll_status_muxr;
    case CDSP0_PLL_TEST_DIV_CDIVR:
        return s->pll_test_div_cdivr;
    case CDSP0_PLL_TEST_CBCR:
        return s->pll_test_cbcr;
    case CDSP0_SM_DEBUG_DIV_CDIVR:
        return s->sm_debug_div_cdivr;
    case CDSP0_SM_DEBUG_CBCR:
        return s->sm_debug_cbcr;
    case CDSP0_VAPSS_GDS_HW_CTRL:
        return s->vapss_gds_hw_ctrl;
    case CDSP0_VAPSS_GDS_HW_STATUS:
        return s->vapss_gds_hw_status;
    case CDSP0_GDS_HW_CTRL_SEQUENCE_ABORT_IRQ_STATUS_Q6:
        return s->gds_hw_ctrl_sequence_abort_irq_status_q6;
    case CDSP0_GDS_HW_CTRL_SEQUENCE_ABORT_IRQ_ENABLE_Q6:
        return s->gds_hw_ctrl_sequence_abort_irq_enable_q6;
    case CDSP0_GDS_HW_CTRL_SEQUENCE_ABORT_IRQ_STATUS_APPS:
        return s->gds_hw_ctrl_sequence_abort_irq_status_apps;
    case CDSP0_GDS_HW_CTRL_SEQUENCE_ABORT_IRQ_ENABLE_APPS:
        return s->gds_hw_ctrl_sequence_abort_irq_enable_apps;
    case CDSP0_TURING_WRAPPER_RSCC_BR_EVENT:
        return s->turing_wrapper_rscc_br_event;
    case CDSP0_TURING_WRAPPER_RSCC_BR_EVENT_OVERRIDE_MASK:
        return s->turing_wrapper_rscc_br_event_override_mask;
    case CDSP0_TURING_WRAPPER_RSCC_BR_EVENT_OVERRIDE_VAL:
        return s->turing_wrapper_rscc_br_event_override_val;
    case CDSP0_TURING_WRAPPER_RSCC_PWR_CTRL_OVERRIDE_MASK:
        return s->turing_wrapper_rscc_pwr_ctrl_override_mask;
    case CDSP0_TURING_WRAPPER_RSCC_PWR_CTRL_OVERRIDE_VAL:
        return s->turing_wrapper_rscc_pwr_ctrl_override_val;
    case CDSP0_TURING_WRAPPER_RSCC_WAIT_EVENT_OVERRIDE_MASK:
        return s->turing_wrapper_rscc_wait_event_override_mask;
    case CDSP0_TURING_WRAPPER_RSCC_WAIT_EVENT_OVERRIDE_VAL:
        return s->turing_wrapper_rscc_wait_event_override_val;
    case CDSP0_Q6SS_ALT_RESET_CTL:
        return s->q6ss_alt_reset_ctl;
    case CDSP0_Q6_AXIM_CLKON_HW_EN:
        return s->q6_axim_clkon_hw_en;
    case CDSP0_SPARE_CTRL:
        return s->spare_ctrl;
    case CDSP0_SPARE_STATUS:
        return s->spare_status;
    default:
        return 0;
    }
}

static void cdsp0_clkctl_write(void *opaque, hwaddr addr, uint64_t value,
                               unsigned size)
{
    Cdsp0ClkctlState *s = opaque;
    if (size != 4) {
        cdsp0_clkctl_bad("write", addr, size);
        return;
    }
    switch (addr) {
    case CDSP0_AON_CMD_RCGR:
        s->aon_cmd_rcgr = (uint32_t)value;
        break;
    case CDSP0_AON_CFG_RCGR:
        s->aon_cfg_rcgr = (uint32_t)value;
        break;
    case CDSP0_AON_DCD_CDIV_DCDR:
        s->aon_dcd_cdiv_dcdr = (uint32_t)value;
        break;
    case CDSP0_TURING_WRAPPER_AON_CBCR:
        s->turing_wrapper_aon_cbcr = (uint32_t)value;
        break;
    case CDSP0_TURING_WRAPPER_CNOC_SWAY_AON_CBCR:
        s->turing_wrapper_cnoc_sway_aon_cbcr = (uint32_t)value;
        break;
    case CDSP0_TURING_WRAPPER_BUS_TIMEOUT_AON_CBCR:
        s->turing_wrapper_bus_timeout_aon_cbcr = (uint32_t)value;
        break;
    case CDSP0_TURING_WRAPPER_RSCC_AON_CBCR:
        s->turing_wrapper_rscc_aon_cbcr = (uint32_t)value;
        break;
    case CDSP0_AUX_XO_CBCR:
        s->aux_xo_cbcr = (uint32_t)value;
        break;
    case CDSP0_AUX_ACCU_XO_CBCR:
        s->aux_accu_xo_cbcr = (uint32_t)value;
        break;
    case CDSP0_VAPSS_XO_CBCR:
        s->vapss_xo_cbcr = (uint32_t)value;
        break;
    case CDSP0_VAPSS_ACCU_XO_CBCR:
        s->vapss_accu_xo_cbcr = (uint32_t)value;
        break;
    case CDSP0_XO_CBCR:
        s->xo_cbcr = (uint32_t)value;
        break;
    case CDSP0_XO_CDIV_CDIVR:
        s->xo_cdiv_cdivr = (uint32_t)value;
        break;
    case CDSP0_XO_CDIV_CBCR:
        s->xo_cdiv_cbcr = (uint32_t)value;
        break;
    case CDSP0_VAPSS_GDSCR:
        s->vapss_gdscr = (uint32_t)value;
        break;
    case CDSP0_VAPSS_CFG_GDSCR:
        s->vapss_cfg_gdscr = (uint32_t)value;
        break;
    case CDSP0_VAPSS_CFG2_GDSCR:
        s->vapss_cfg2_gdscr = (uint32_t)value;
        break;
    case CDSP0_VAPSS_CFG3_GDSCR:
        s->vapss_cfg3_gdscr = (uint32_t)value;
        break;
    case CDSP0_VAPSS_CFG4_GDSCR:
        s->vapss_cfg4_gdscr = (uint32_t)value;
        break;
    case CDSP0_VAPSS_CORE_BCR:
        s->vapss_core_bcr = (uint32_t)value;
        break;
    case CDSP0_VAPSS_DMA_CMD_RCGR:
        s->vapss_dma_cmd_rcgr = (uint32_t)value;
        break;
    case CDSP0_VAPSS_DMA_CFG_RCGR:
        s->vapss_dma_cfg_rcgr = (uint32_t)value;
        break;
    case CDSP0_VAPSS_DMA_DCD_CDIV_DCDR:
        s->vapss_dma_dcd_cdiv_dcdr = (uint32_t)value;
        break;
    case CDSP0_VAPSS_DMA_CBCR:
        s->vapss_dma_cbcr = (uint32_t)value;
        break;
    case CDSP0_VAPSS_DMA_SREGR:
        s->vapss_dma_sregr = (uint32_t)value;
        break;
    case CDSP0_VAPSS_VMA_CMD_RCGR:
        s->vapss_vma_cmd_rcgr = (uint32_t)value;
        break;
    case CDSP0_VAPSS_VMA_CFG_RCGR:
        s->vapss_vma_cfg_rcgr = (uint32_t)value;
        break;
    case CDSP0_VAPSS_VMA_DCD_CDIV_DCDR:
        s->vapss_vma_dcd_cdiv_dcdr = (uint32_t)value;
        break;
    case CDSP0_VAPSS_VMA_CBCR:
        s->vapss_vma_cbcr = (uint32_t)value;
        break;
    case CDSP0_VAPSS_VMA_SREGR:
        s->vapss_vma_sregr = (uint32_t)value;
        break;
    case CDSP0_VAPSS_TCMS_CMD_RCGR:
        s->vapss_tcms_cmd_rcgr = (uint32_t)value;
        break;
    case CDSP0_VAPSS_TCMS_CFG_RCGR:
        s->vapss_tcms_cfg_rcgr = (uint32_t)value;
        break;
    case CDSP0_VAPSS_TCMS_DCD_CDIV_DCDR:
        s->vapss_tcms_dcd_cdiv_dcdr = (uint32_t)value;
        break;
    case CDSP0_VAPSS_TCMS_CBCR:
        s->vapss_tcms_cbcr = (uint32_t)value;
        break;
    case CDSP0_VAPSS_TCMS_SREGR:
        s->vapss_tcms_sregr = (uint32_t)value;
        break;
    case CDSP0_VAPSS_HCP_CMD_RCGR:
        s->vapss_hcp_cmd_rcgr = (uint32_t)value;
        break;
    case CDSP0_VAPSS_HCP_CFG_RCGR:
        s->vapss_hcp_cfg_rcgr = (uint32_t)value;
        break;
    case CDSP0_VAPSS_HCP_DCD_CDIV_DCDR:
        s->vapss_hcp_dcd_cdiv_dcdr = (uint32_t)value;
        break;
    case CDSP0_VAPSS_HCP_CBCR:
        s->vapss_hcp_cbcr = (uint32_t)value;
        break;
    case CDSP0_VAPSS_HCP_SREGR:
        s->vapss_hcp_sregr = (uint32_t)value;
        break;
    case CDSP0_VAPSS_HCP0_CBCR:
        s->vapss_hcp0_cbcr = (uint32_t)value;
        break;
    case CDSP0_VAPSS_BUS_CMD_RCGR:
        s->vapss_bus_cmd_rcgr = (uint32_t)value;
        break;
    case CDSP0_VAPSS_BUS_CFG_RCGR:
        s->vapss_bus_cfg_rcgr = (uint32_t)value;
        break;
    case CDSP0_VAPSS_Q6_AXI_DCD_CDIV_DCDR:
        s->vapss_q6_axi_dcd_cdiv_dcdr = (uint32_t)value;
        break;
    case CDSP0_VAPSS_BUS_CBCR:
        s->vapss_bus_cbcr = (uint32_t)value;
        break;
    case CDSP0_VAPSS_BUS_SREGR:
        s->vapss_bus_sregr = (uint32_t)value;
        break;
    case CDSP0_VAPSS_FINT_CMD_RCGR:
        s->vapss_fint_cmd_rcgr = (uint32_t)value;
        break;
    case CDSP0_VAPSS_FINT_CFG_RCGR:
        s->vapss_fint_cfg_rcgr = (uint32_t)value;
        break;
    case CDSP0_VAPSS_FINT_DCD_CDIV_DCDR:
        s->vapss_fint_dcd_cdiv_dcdr = (uint32_t)value;
        break;
    case CDSP0_VAPSS_FINT_CBCR:
        s->vapss_fint_cbcr = (uint32_t)value;
        break;
    case CDSP0_VAPSS_FINT_SREGR:
        s->vapss_fint_sregr = (uint32_t)value;
        break;
    case CDSP0_VAPSS_FINT_PROG_CBCR:
        s->vapss_fint_prog_cbcr = (uint32_t)value;
        break;
    case CDSP0_VAPSS_AHBS_TIMEOUT_CBCR:
        s->vapss_ahbs_timeout_cbcr = (uint32_t)value;
        break;
    case CDSP0_VAPSS_DMA_AHBS_CBCR:
        s->vapss_dma_ahbs_cbcr = (uint32_t)value;
        break;
    case CDSP0_VAPSS_VMA_AHBS_CBCR:
        s->vapss_vma_ahbs_cbcr = (uint32_t)value;
        break;
    case CDSP0_VAPSS_HCP_AHBS_CBCR:
        s->vapss_hcp_ahbs_cbcr = (uint32_t)value;
        break;
    case CDSP0_VAPSS_AHBS_AON_CBCR:
        s->vapss_ahbs_aon_cbcr = (uint32_t)value;
        break;
    case CDSP0_VAPSS_TBUF2_AHBS_CBCR:
        s->vapss_tbuf2_ahbs_cbcr = (uint32_t)value;
        break;
    case CDSP0_VAPSS_ATB_CBCR:
        s->vapss_atb_cbcr = (uint32_t)value;
        break;
    case CDSP0_VAPSS_APB_CBCR:
        s->vapss_apb_cbcr = (uint32_t)value;
        break;
    case CDSP0_VAPSS_TBUF2_CMD_RCGR:
        s->vapss_tbuf2_cmd_rcgr = (uint32_t)value;
        break;
    case CDSP0_VAPSS_TBUF2_CFG_RCGR:
        s->vapss_tbuf2_cfg_rcgr = (uint32_t)value;
        break;
    case CDSP0_VAPSS_TBUF2_CBCR:
        s->vapss_tbuf2_cbcr = (uint32_t)value;
        break;
    case CDSP0_VAPSS_TBUF2_SREGR:
        s->vapss_tbuf2_sregr = (uint32_t)value;
        break;
    case CDSP0_NOC_TBUF2_CBCR:
        s->noc_tbuf2_cbcr = (uint32_t)value;
        break;
    case CDSP0_AUX_GDSCR:
        s->aux_gdscr = (uint32_t)value;
        break;
    case CDSP0_AUX_CFG_GDSCR:
        s->aux_cfg_gdscr = (uint32_t)value;
        break;
    case CDSP0_AUX_CFG2_GDSCR:
        s->aux_cfg2_gdscr = (uint32_t)value;
        break;
    case CDSP0_AUX_CFG3_GDSCR:
        s->aux_cfg3_gdscr = (uint32_t)value;
        break;
    case CDSP0_AUX_CFG4_GDSCR:
        s->aux_cfg4_gdscr = (uint32_t)value;
        break;
    case CDSP0_AUX_CORE_BCR:
        s->aux_core_bcr = (uint32_t)value;
        break;
    case CDSP0_CENG_AHBS_CBCR:
        s->ceng_ahbs_cbcr = (uint32_t)value;
        break;
    case CDSP0_NOC_AHBS_CBCR:
        s->noc_ahbs_cbcr = (uint32_t)value;
        break;
    case CDSP0_CENG_CBCR:
        s->ceng_cbcr = (uint32_t)value;
        break;
    case CDSP0_CENG_SREGR:
        s->ceng_sregr = (uint32_t)value;
        break;
    case CDSP0_NOC_CBCR:
        s->noc_cbcr = (uint32_t)value;
        break;
    case CDSP0_NOC_SREGR:
        s->noc_sregr = (uint32_t)value;
        break;
    case CDSP0_NOC_VAPSS_BUS_CBCR:
        s->noc_vapss_bus_cbcr = (uint32_t)value;
        break;
    case CDSP0_NOC_ATB_CBCR:
        s->noc_atb_cbcr = (uint32_t)value;
        break;
    case CDSP0_NOC_APB_CBCR:
        s->noc_apb_cbcr = (uint32_t)value;
        break;
    case CDSP0_Q6SS_BCR:
        s->q6ss_bcr = (uint32_t)value;
        break;
    case CDSP0_Q6SS_Q6_AXIM_CBCR:
        s->q6ss_q6_axim_cbcr = (uint32_t)value;
        break;
    case CDSP0_Q6SS_AXIS2_CBCR:
        s->q6ss_axis2_cbcr = (uint32_t)value;
        break;
    case CDSP0_Q6SS_AHBM_AON_CBCR:
        s->q6ss_ahbm_aon_cbcr = (uint32_t)value;
        break;
    case CDSP0_Q6SS_AHBS_AON_CBCR:
        s->q6ss_ahbs_aon_cbcr = (uint32_t)value;
        break;
    case CDSP0_Q6SS_ALT_RESET_AON_CBCR:
        s->q6ss_alt_reset_aon_cbcr = (uint32_t)value;
        break;
    case CDSP0_Q6SS_LMH_CMD_RCGR:
        s->q6ss_lmh_cmd_rcgr = (uint32_t)value;
        break;
    case CDSP0_Q6SS_LMH_CFG_RCGR:
        s->q6ss_lmh_cfg_rcgr = (uint32_t)value;
        break;
    case CDSP0_Q6SS_LMH_CBCR:
        s->q6ss_lmh_cbcr = (uint32_t)value;
        break;
    case CDSP0_Q6SS_ISENSE_CMD_RCGR:
        s->q6ss_isense_cmd_rcgr = (uint32_t)value;
        break;
    case CDSP0_Q6SS_ISENSE_CFG_RCGR:
        s->q6ss_isense_cfg_rcgr = (uint32_t)value;
        break;
    case CDSP0_Q6SS_ISENSE_CTRL_CBCR:
        s->q6ss_isense_ctrl_cbcr = (uint32_t)value;
        break;
    case CDSP0_Q6SS_ISENSE_CORE_CBCR:
        s->q6ss_isense_core_cbcr = (uint32_t)value;
        break;
    case CDSP0_Q6SS_LLM_TEMP_SSC_CBCR:
        s->q6ss_llm_temp_ssc_cbcr = (uint32_t)value;
        break;
    case CDSP0_Q6SS_LLM_CURR_SSC_CBCR:
        s->q6ss_llm_curr_ssc_cbcr = (uint32_t)value;
        break;
    case CDSP0_VAPSS_BUS_DCD_CDIV_DCDR:
        s->vapss_bus_dcd_cdiv_dcdr = (uint32_t)value;
        break;
    case CDSP0_VAPSS_TBUF2_DCD_CDIV_DCDR:
        s->vapss_tbuf2_dcd_cdiv_dcdr = (uint32_t)value;
        break;
    case CDSP0_DEBUG_MUX_MUXR:
        s->debug_mux_muxr = (uint32_t)value;
        break;
    case CDSP0_DEBUG_DIV_CDIVR:
        s->debug_div_cdivr = (uint32_t)value;
        break;
    case CDSP0_DEBUG_CBCR:
        s->debug_cbcr = (uint32_t)value;
        break;
    case CDSP0_PLL_TEST_MUX_MUXR:
        s->pll_test_mux_muxr = (uint32_t)value;
        break;
    case CDSP0_PLL_STATUS_MUXR:
        s->pll_status_muxr = (uint32_t)value;
        break;
    case CDSP0_PLL_TEST_DIV_CDIVR:
        s->pll_test_div_cdivr = (uint32_t)value;
        break;
    case CDSP0_PLL_TEST_CBCR:
        s->pll_test_cbcr = (uint32_t)value;
        break;
    case CDSP0_SM_DEBUG_DIV_CDIVR:
        s->sm_debug_div_cdivr = (uint32_t)value;
        break;
    case CDSP0_SM_DEBUG_CBCR:
        s->sm_debug_cbcr = (uint32_t)value;
        break;
    case CDSP0_VAPSS_GDS_HW_CTRL:
        s->vapss_gds_hw_ctrl = (uint32_t)value;
        break;
    case CDSP0_VAPSS_GDS_HW_STATUS:
        s->vapss_gds_hw_status = (uint32_t)value;
        break;
    case CDSP0_GDS_HW_CTRL_SEQUENCE_ABORT_IRQ_STATUS_Q6:
        s->gds_hw_ctrl_sequence_abort_irq_status_q6 = (uint32_t)value;
        break;
    case CDSP0_GDS_HW_CTRL_SEQUENCE_ABORT_IRQ_ENABLE_Q6:
        s->gds_hw_ctrl_sequence_abort_irq_enable_q6 = (uint32_t)value;
        break;
    case CDSP0_GDS_HW_CTRL_SEQUENCE_ABORT_IRQ_STATUS_APPS:
        s->gds_hw_ctrl_sequence_abort_irq_status_apps = (uint32_t)value;
        break;
    case CDSP0_GDS_HW_CTRL_SEQUENCE_ABORT_IRQ_ENABLE_APPS:
        s->gds_hw_ctrl_sequence_abort_irq_enable_apps = (uint32_t)value;
        break;
    case CDSP0_TURING_WRAPPER_RSCC_BR_EVENT:
        s->turing_wrapper_rscc_br_event = (uint32_t)value;
        break;
    case CDSP0_TURING_WRAPPER_RSCC_BR_EVENT_OVERRIDE_MASK:
        s->turing_wrapper_rscc_br_event_override_mask = (uint32_t)value;
        break;
    case CDSP0_TURING_WRAPPER_RSCC_BR_EVENT_OVERRIDE_VAL:
        s->turing_wrapper_rscc_br_event_override_val = (uint32_t)value;
        break;
    case CDSP0_TURING_WRAPPER_RSCC_PWR_CTRL_OVERRIDE_MASK:
        s->turing_wrapper_rscc_pwr_ctrl_override_mask = (uint32_t)value;
        break;
    case CDSP0_TURING_WRAPPER_RSCC_PWR_CTRL_OVERRIDE_VAL:
        s->turing_wrapper_rscc_pwr_ctrl_override_val = (uint32_t)value;
        break;
    case CDSP0_TURING_WRAPPER_RSCC_WAIT_EVENT_OVERRIDE_MASK:
        s->turing_wrapper_rscc_wait_event_override_mask = (uint32_t)value;
        break;
    case CDSP0_TURING_WRAPPER_RSCC_WAIT_EVENT_OVERRIDE_VAL:
        s->turing_wrapper_rscc_wait_event_override_val = (uint32_t)value;
        break;
    case CDSP0_Q6SS_ALT_RESET_CTL:
        s->q6ss_alt_reset_ctl = (uint32_t)value;
        break;
    case CDSP0_Q6_AXIM_CLKON_HW_EN:
        s->q6_axim_clkon_hw_en = (uint32_t)value;
        break;
    case CDSP0_SPARE_CTRL:
        s->spare_ctrl = (uint32_t)value;
        break;
    case CDSP0_SPARE_STATUS:
        s->spare_status = (uint32_t)value;
        break;
    default:
        break;
    }
}

static const MemoryRegionOps cdsp0_clkctl_ops = {
    .read = cdsp0_clkctl_read,
    .write = cdsp0_clkctl_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = { .min_access_size = 4, .max_access_size = 4 },
};

static void cdsp0_clkctl_reset(Object *obj, ResetType type)
{
    Cdsp0ClkctlState *s = CDSP0_CLKCTL(obj);

    /* Exact reset values as provided */
    s->aon_cmd_rcgr = 0x00000000;
    s->aon_cfg_rcgr = 0x00100000;
    s->aon_dcd_cdiv_dcdr = 0x00000021;
    s->turing_wrapper_aon_cbcr = 0x00000001;
    s->turing_wrapper_cnoc_sway_aon_cbcr = 0x00000001;
    s->turing_wrapper_bus_timeout_aon_cbcr = 0x00000001;
    s->turing_wrapper_rscc_aon_cbcr = 0x00000001;

    s->aux_xo_cbcr = 0x80000000;
    s->aux_accu_xo_cbcr = 0x80000000;
    s->vapss_xo_cbcr = 0x80000000;
    s->vapss_accu_xo_cbcr = 0x80000000;
    s->xo_cbcr = 0x00000001;
    s->xo_cdiv_cdivr = 0x00000001;
    s->xo_cdiv_cbcr = 0x00000001;

    s->vapss_gdscr = 0x80226001;
    s->vapss_cfg_gdscr = 0x00098400;
    s->vapss_cfg2_gdscr = 0x0002022A;
    s->vapss_cfg3_gdscr = 0x02F00000;
    s->vapss_cfg4_gdscr = 0x00222222;
    s->vapss_core_bcr = 0x00000000;
    s->vapss_dma_cmd_rcgr = 0x80000000;
    s->vapss_dma_cfg_rcgr = 0x00100000;
    s->vapss_dma_dcd_cdiv_dcdr = 0x00000021;
    s->vapss_dma_cbcr = 0x80000220;
    s->vapss_dma_sregr = 0x00014000;

    s->vapss_vma_cmd_rcgr = 0x80000000;
    s->vapss_vma_cfg_rcgr = 0x00100000;
    s->vapss_vma_dcd_cdiv_dcdr = 0x00000021;
    s->vapss_vma_cbcr = 0x80000220;
    s->vapss_vma_sregr = 0x00014000;

    s->vapss_tcms_cmd_rcgr = 0x80000000;
    s->vapss_tcms_cfg_rcgr = 0x00100000;
    s->vapss_tcms_dcd_cdiv_dcdr = 0x00000021;
    s->vapss_tcms_cbcr = 0x80000220;
    s->vapss_tcms_sregr = 0x00014000;

    s->vapss_hcp_cmd_rcgr = 0x80000000;
    s->vapss_hcp_cfg_rcgr = 0x00100000;
    s->vapss_hcp_dcd_cdiv_dcdr = 0x00000021;
    s->vapss_hcp_cbcr = 0x80000220;
    s->vapss_hcp_sregr = 0x00014000;
    s->vapss_hcp0_cbcr = 0x80000000;

    s->vapss_bus_cmd_rcgr = 0x80000000;
    s->vapss_bus_cfg_rcgr = 0x00100000;
    s->vapss_q6_axi_dcd_cdiv_dcdr = 0x00000021;
    s->vapss_bus_cbcr = 0x80000220;
    s->vapss_bus_sregr = 0x00014000;

    s->vapss_fint_cmd_rcgr = 0x80000000;
    s->vapss_fint_cfg_rcgr = 0x00100000;
    s->vapss_fint_dcd_cdiv_dcdr = 0x00000021;
    s->vapss_fint_cbcr = 0x80000220;
    s->vapss_fint_sregr = 0x00014000;
    s->vapss_fint_prog_cbcr = 0x80000000;
    s->vapss_ahbs_timeout_cbcr = 0x80000000;
    s->vapss_dma_ahbs_cbcr = 0x80000000;
    s->vapss_vma_ahbs_cbcr = 0x80000000;
    s->vapss_hcp_ahbs_cbcr = 0x80000000;
    s->vapss_ahbs_aon_cbcr = 0x80000000;
    s->vapss_tbuf2_ahbs_cbcr = 0x80000000;

    s->vapss_atb_cbcr = 0x80000000;
    s->vapss_apb_cbcr = 0x80000000;

    s->vapss_tbuf2_cmd_rcgr = 0x80000000;
    s->vapss_tbuf2_cfg_rcgr = 0x00100000;
    s->vapss_tbuf2_cbcr = 0x80000220;
    s->vapss_tbuf2_sregr = 0x00014000;
    s->noc_tbuf2_cbcr = 0x80000000;

    s->aux_gdscr = 0x80226000;
    s->aux_cfg_gdscr = 0x00088400;
    s->aux_cfg2_gdscr = 0x0002022A;
    s->aux_cfg3_gdscr = 0x02F00000;
    s->aux_cfg4_gdscr = 0x00222222;
    s->aux_core_bcr = 0x00000000;
    s->ceng_ahbs_cbcr = 0x80000000;
    s->noc_ahbs_cbcr = 0x80000000;
    s->ceng_cbcr = 0x80000220;
    s->ceng_sregr = 0x00014000;
    s->noc_cbcr = 0x80000220;
    s->noc_sregr = 0x00014000;
    s->noc_vapss_bus_cbcr = 0x80000000;
    s->noc_atb_cbcr = 0x80000000;
    s->noc_apb_cbcr = 0x80000000;

    s->q6ss_bcr = 0x00000000;
    s->q6ss_q6_axim_cbcr = 0x80000000;
    s->q6ss_axis2_cbcr = 0x80000000;
    s->q6ss_ahbm_aon_cbcr = 0x80000000;
    s->q6ss_ahbs_aon_cbcr = 0x80000000;
    s->q6ss_alt_reset_aon_cbcr = 0x80000000;
    s->q6ss_lmh_cmd_rcgr = 0x80000000;
    s->q6ss_lmh_cfg_rcgr = 0x00000000;
    s->q6ss_lmh_cbcr = 0x80000000;
    s->q6ss_isense_cmd_rcgr = 0x80000000;
    s->q6ss_isense_cfg_rcgr = 0x00000000;
    s->q6ss_isense_ctrl_cbcr = 0x80000000;
    s->q6ss_isense_core_cbcr = 0x80000000;
    s->q6ss_llm_temp_ssc_cbcr = 0x80000000;
    s->q6ss_llm_curr_ssc_cbcr = 0x80000000;

    s->vapss_bus_dcd_cdiv_dcdr = 0x00000021;
    s->vapss_tbuf2_dcd_cdiv_dcdr = 0x00000021;

    s->debug_mux_muxr = 0x00000000;
    s->debug_div_cdivr = 0x00000001;
    s->debug_cbcr = 0x80000000;
    s->pll_test_mux_muxr = 0x00000000;
    s->pll_status_muxr = 0x00000000;
    s->pll_test_div_cdivr = 0x00000001;
    s->pll_test_cbcr = 0x80000000;
    s->sm_debug_div_cdivr = 0x00000001;
    s->sm_debug_cbcr = 0x80000000;

    s->vapss_gds_hw_ctrl = 0x00000FF0;
    s->vapss_gds_hw_status = 0x0000004a;
    s->gds_hw_ctrl_sequence_abort_irq_status_q6 = 0x00000000;
    s->gds_hw_ctrl_sequence_abort_irq_enable_q6 = 0x00000000;
    s->gds_hw_ctrl_sequence_abort_irq_status_apps = 0x00000000;
    s->gds_hw_ctrl_sequence_abort_irq_enable_apps = 0x00000000;
    s->turing_wrapper_rscc_br_event = 0x00000000;
    s->turing_wrapper_rscc_br_event_override_mask = 0x00000000;
    s->turing_wrapper_rscc_br_event_override_val = 0x00000000;
    s->turing_wrapper_rscc_pwr_ctrl_override_mask = 0x00000000;
    s->turing_wrapper_rscc_pwr_ctrl_override_val = 0x00000000;
    s->turing_wrapper_rscc_wait_event_override_mask = 0x00000000;
    s->turing_wrapper_rscc_wait_event_override_val = 0x00000000;
    s->q6ss_alt_reset_ctl = 0x00000000;
    s->q6_axim_clkon_hw_en = 0x00000000;
    s->spare_ctrl = 0x00000000;
    s->spare_status = 0x00000000;
}

static void cdsp0_clkctl_realize(DeviceState *dev, Error **errp)
{
    Cdsp0ClkctlState *s = CDSP0_CLKCTL(dev);

    memory_region_init_io(&s->mmio, OBJECT(dev), &cdsp0_clkctl_ops, s,
                          TYPE_CDSP0_CLKCTL, CDSP0_CLKCTL_MMIO_SIZE);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->mmio);
}

static const VMStateDescription vmstate_qdsp6_cc_swi = {
    .name = "qdsp6ss-cc-swi",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_UINT32(aon_cmd_rcgr, Cdsp0ClkctlState),
        VMSTATE_UINT32(aon_cfg_rcgr, Cdsp0ClkctlState),
        VMSTATE_UINT32(aon_dcd_cdiv_dcdr, Cdsp0ClkctlState),
        VMSTATE_UINT32(turing_wrapper_aon_cbcr, Cdsp0ClkctlState),
        VMSTATE_UINT32(turing_wrapper_cnoc_sway_aon_cbcr, Cdsp0ClkctlState),
        VMSTATE_UINT32(turing_wrapper_bus_timeout_aon_cbcr, Cdsp0ClkctlState),
        VMSTATE_UINT32(turing_wrapper_rscc_aon_cbcr, Cdsp0ClkctlState),
        VMSTATE_UINT32(aux_xo_cbcr, Cdsp0ClkctlState),
        VMSTATE_UINT32(aux_accu_xo_cbcr, Cdsp0ClkctlState),
        VMSTATE_UINT32(vapss_xo_cbcr, Cdsp0ClkctlState),
        VMSTATE_UINT32(vapss_accu_xo_cbcr, Cdsp0ClkctlState),
        VMSTATE_UINT32(xo_cbcr, Cdsp0ClkctlState),
        VMSTATE_UINT32(xo_cdiv_cdivr, Cdsp0ClkctlState),
        VMSTATE_UINT32(xo_cdiv_cbcr, Cdsp0ClkctlState),
        VMSTATE_UINT32(vapss_gdscr, Cdsp0ClkctlState),
        VMSTATE_UINT32(vapss_cfg_gdscr, Cdsp0ClkctlState),
        VMSTATE_UINT32(vapss_cfg2_gdscr, Cdsp0ClkctlState),
        VMSTATE_UINT32(vapss_cfg3_gdscr, Cdsp0ClkctlState),
        VMSTATE_UINT32(vapss_cfg4_gdscr, Cdsp0ClkctlState),
        VMSTATE_UINT32(vapss_core_bcr, Cdsp0ClkctlState),
        VMSTATE_UINT32(vapss_dma_cmd_rcgr, Cdsp0ClkctlState),
        VMSTATE_UINT32(vapss_dma_cfg_rcgr, Cdsp0ClkctlState),
        VMSTATE_UINT32(vapss_dma_dcd_cdiv_dcdr, Cdsp0ClkctlState),
        VMSTATE_UINT32(vapss_dma_cbcr, Cdsp0ClkctlState),
        VMSTATE_UINT32(vapss_dma_sregr, Cdsp0ClkctlState),
        VMSTATE_UINT32(vapss_vma_cmd_rcgr, Cdsp0ClkctlState),
        VMSTATE_UINT32(vapss_vma_cfg_rcgr, Cdsp0ClkctlState),
        VMSTATE_UINT32(vapss_vma_dcd_cdiv_dcdr, Cdsp0ClkctlState),
        VMSTATE_UINT32(vapss_vma_cbcr, Cdsp0ClkctlState),
        VMSTATE_UINT32(vapss_vma_sregr, Cdsp0ClkctlState),
        VMSTATE_UINT32(vapss_tcms_cmd_rcgr, Cdsp0ClkctlState),
        VMSTATE_UINT32(vapss_tcms_cfg_rcgr, Cdsp0ClkctlState),
        VMSTATE_UINT32(vapss_tcms_dcd_cdiv_dcdr, Cdsp0ClkctlState),
        VMSTATE_UINT32(vapss_tcms_cbcr, Cdsp0ClkctlState),
        VMSTATE_UINT32(vapss_tcms_sregr, Cdsp0ClkctlState),
        VMSTATE_UINT32(vapss_hcp_cmd_rcgr, Cdsp0ClkctlState),
        VMSTATE_UINT32(vapss_hcp_cfg_rcgr, Cdsp0ClkctlState),
        VMSTATE_UINT32(vapss_hcp_dcd_cdiv_dcdr, Cdsp0ClkctlState),
        VMSTATE_UINT32(vapss_hcp_cbcr, Cdsp0ClkctlState),
        VMSTATE_UINT32(vapss_hcp_sregr, Cdsp0ClkctlState),
        VMSTATE_UINT32(vapss_hcp0_cbcr, Cdsp0ClkctlState),
        VMSTATE_UINT32(vapss_bus_cmd_rcgr, Cdsp0ClkctlState),
        VMSTATE_UINT32(vapss_bus_cfg_rcgr, Cdsp0ClkctlState),
        VMSTATE_UINT32(vapss_q6_axi_dcd_cdiv_dcdr, Cdsp0ClkctlState),
        VMSTATE_UINT32(vapss_bus_cbcr, Cdsp0ClkctlState),
        VMSTATE_UINT32(vapss_bus_sregr, Cdsp0ClkctlState),
        VMSTATE_UINT32(vapss_fint_cmd_rcgr, Cdsp0ClkctlState),
        VMSTATE_UINT32(vapss_fint_cfg_rcgr, Cdsp0ClkctlState),
        VMSTATE_UINT32(vapss_fint_dcd_cdiv_dcdr, Cdsp0ClkctlState),
        VMSTATE_UINT32(vapss_fint_cbcr, Cdsp0ClkctlState),
        VMSTATE_UINT32(vapss_fint_sregr, Cdsp0ClkctlState),
        VMSTATE_UINT32(vapss_fint_prog_cbcr, Cdsp0ClkctlState),
        VMSTATE_UINT32(vapss_ahbs_timeout_cbcr, Cdsp0ClkctlState),
        VMSTATE_UINT32(vapss_dma_ahbs_cbcr, Cdsp0ClkctlState),
        VMSTATE_UINT32(vapss_vma_ahbs_cbcr, Cdsp0ClkctlState),
        VMSTATE_UINT32(vapss_hcp_ahbs_cbcr, Cdsp0ClkctlState),
        VMSTATE_UINT32(vapss_ahbs_aon_cbcr, Cdsp0ClkctlState),
        VMSTATE_UINT32(vapss_tbuf2_ahbs_cbcr, Cdsp0ClkctlState),
        VMSTATE_UINT32(vapss_atb_cbcr, Cdsp0ClkctlState),
        VMSTATE_UINT32(vapss_apb_cbcr, Cdsp0ClkctlState),
        VMSTATE_UINT32(vapss_tbuf2_cmd_rcgr, Cdsp0ClkctlState),
        VMSTATE_UINT32(vapss_tbuf2_cfg_rcgr, Cdsp0ClkctlState),
        VMSTATE_UINT32(vapss_tbuf2_cbcr, Cdsp0ClkctlState),
        VMSTATE_UINT32(vapss_tbuf2_sregr, Cdsp0ClkctlState),
        VMSTATE_UINT32(noc_tbuf2_cbcr, Cdsp0ClkctlState),
        VMSTATE_UINT32(aux_gdscr, Cdsp0ClkctlState),
        VMSTATE_UINT32(aux_cfg_gdscr, Cdsp0ClkctlState),
        VMSTATE_UINT32(aux_cfg2_gdscr, Cdsp0ClkctlState),
        VMSTATE_UINT32(aux_cfg3_gdscr, Cdsp0ClkctlState),
        VMSTATE_UINT32(aux_cfg4_gdscr, Cdsp0ClkctlState),
        VMSTATE_UINT32(aux_core_bcr, Cdsp0ClkctlState),
        VMSTATE_UINT32(ceng_ahbs_cbcr, Cdsp0ClkctlState),
        VMSTATE_UINT32(noc_ahbs_cbcr, Cdsp0ClkctlState),
        VMSTATE_UINT32(ceng_cbcr, Cdsp0ClkctlState),
        VMSTATE_UINT32(ceng_sregr, Cdsp0ClkctlState),
        VMSTATE_UINT32(noc_cbcr, Cdsp0ClkctlState),
        VMSTATE_UINT32(noc_sregr, Cdsp0ClkctlState),
        VMSTATE_UINT32(noc_vapss_bus_cbcr, Cdsp0ClkctlState),
        VMSTATE_UINT32(noc_atb_cbcr, Cdsp0ClkctlState),
        VMSTATE_UINT32(noc_apb_cbcr, Cdsp0ClkctlState),
        VMSTATE_UINT32(q6ss_bcr, Cdsp0ClkctlState),
        VMSTATE_UINT32(q6ss_q6_axim_cbcr, Cdsp0ClkctlState),
        VMSTATE_UINT32(q6ss_axis2_cbcr, Cdsp0ClkctlState),
        VMSTATE_UINT32(q6ss_ahbm_aon_cbcr, Cdsp0ClkctlState),
        VMSTATE_UINT32(q6ss_ahbs_aon_cbcr, Cdsp0ClkctlState),
        VMSTATE_UINT32(q6ss_alt_reset_aon_cbcr, Cdsp0ClkctlState),
        VMSTATE_UINT32(q6ss_lmh_cmd_rcgr, Cdsp0ClkctlState),
        VMSTATE_UINT32(q6ss_lmh_cfg_rcgr, Cdsp0ClkctlState),
        VMSTATE_UINT32(q6ss_lmh_cbcr, Cdsp0ClkctlState),
        VMSTATE_UINT32(q6ss_isense_cmd_rcgr, Cdsp0ClkctlState),
        VMSTATE_UINT32(q6ss_isense_cfg_rcgr, Cdsp0ClkctlState),
        VMSTATE_UINT32(q6ss_isense_ctrl_cbcr, Cdsp0ClkctlState),
        VMSTATE_UINT32(q6ss_isense_core_cbcr, Cdsp0ClkctlState),
        VMSTATE_UINT32(q6ss_llm_temp_ssc_cbcr, Cdsp0ClkctlState),
        VMSTATE_UINT32(q6ss_llm_curr_ssc_cbcr, Cdsp0ClkctlState),
        VMSTATE_UINT32(vapss_bus_dcd_cdiv_dcdr, Cdsp0ClkctlState),
        VMSTATE_UINT32(vapss_tbuf2_dcd_cdiv_dcdr, Cdsp0ClkctlState),
        VMSTATE_UINT32(debug_mux_muxr, Cdsp0ClkctlState),
        VMSTATE_UINT32(debug_div_cdivr, Cdsp0ClkctlState),
        VMSTATE_UINT32(debug_cbcr, Cdsp0ClkctlState),
        VMSTATE_UINT32(pll_test_mux_muxr, Cdsp0ClkctlState),
        VMSTATE_UINT32(pll_status_muxr, Cdsp0ClkctlState),
        VMSTATE_UINT32(pll_test_div_cdivr, Cdsp0ClkctlState),
        VMSTATE_END_OF_LIST()
    }
};
static void cdsp0_clkctl_class_init(ObjectClass *oc, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);
    ResettableClass *rc = RESETTABLE_CLASS(oc);
    dc->realize = cdsp0_clkctl_realize;
    dc->vmsd = &vmstate_qdsp6_cc_swi;
    rc->phases.hold = cdsp0_clkctl_reset;
    dc->desc = "QDSP6 Clock Software Interface (SWI)";
}

static const TypeInfo cdsp0_clkctl_info = {
    .name = TYPE_CDSP0_CLKCTL,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(Cdsp0ClkctlState),
    .class_init = cdsp0_clkctl_class_init,
};

static void cdsp0_clkctl_register_types(void)
{
    type_register_static(&cdsp0_clkctl_info);
}

type_init(cdsp0_clkctl_register_types)
