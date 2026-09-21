/*
 * Turing Clock Controller (TURING_CC) — QEMU sysbus device
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "hw/core/resettable.h"
#include "hw/misc/qcom-turing-cc-regs.h"
#include "migration/vmstate.h"
#include "qemu/log.h"

#define R(off) ((off) / 4)

static inline void turing_cc_bad_access(const char *dir, hwaddr addr,
                                        unsigned size)
{
    qemu_log_mask(LOG_GUEST_ERROR,
                  TYPE_TURING_CC ": bad %s addr=0x%" HWADDR_PRIx " size=%u\n",
                  dir, addr, size);
}

static uint64_t turing_cc_read(void *opaque, hwaddr addr, unsigned size)
{
    TuringCcState *s = opaque;

    if (size != 4) {
        turing_cc_bad_access("read", addr, size);
        return 0;
    }

    switch (addr) {
    /* CMD_RCGR — return value as-is */
    case TURING_CC_AON_CMD_RCGR:
    case TURING_CC_TCC_CMD_RCGR:
    case TURING_CC_CXO_CMD_RCGR:
    case TURING_CC_VAPSS_VMA_CMD_RCGR:
    case TURING_CC_VAPSS_HCP_CMD_RCGR:
    case TURING_CC_VAPSS_FINT_CMD_RCGR:
    case TURING_CC_VAPSS_MVR_CMD_RCGR:
    case TURING_CC_CENG_CMD_RCGR:
    case TURING_CC_UBWCD_CMD_RCGR:
    case TURING_CC_DMA_CMD_RCGR:
    case TURING_CC_TCMS_CMD_RCGR:
    case TURING_CC_Q6SS_LMH_CMD_RCGR:
        return s->regs[R(addr)];

    /* CBCR — clear CLK_OFF (bit 31) on read */
    case TURING_CC_WRAPPER_AON_CBCR:
    case TURING_CC_AHB_MXC_CBCR:
    case TURING_CC_WRAPPER_CNOC_SWAY_AON_CBCR:
    case TURING_CC_WRAPPER_BUS_TIMEOUT_AON_CBCR:
    case TURING_CC_WRAPPER_RSCC_AON_CBCR:
    case TURING_CC_TCC_CBCR:
    case TURING_CC_TCC_MXC_CBCR:
    case TURING_CC_TCC_DIV_CBCR:
    case TURING_CC_XO_CBCR:
    case TURING_CC_XO_DIV_CBCR:
    case TURING_CC_VAPSS_AHBS_TIMEOUT_CBCR:
    case TURING_CC_VAPSS_VMA_AHBS_CBCR:
    case TURING_CC_VAPSS_HCP_AHBS_CBCR:
    case TURING_CC_VAPSS_AHBS_AON_CBCR:
    case TURING_CC_VAPSS_XO_CBCR:
    case TURING_CC_VAPSS_AXI_CBCR:
    case TURING_CC_VAPSS_ATB_CBCR:
    case TURING_CC_VAPSS_APB_CBCR:
    case TURING_CC_VAPSS_VMA_CBCR:
    case TURING_CC_VAPSS_VMA_MSF_CBCR:
    case TURING_CC_VAPSS_HCP_CBCR:
    case TURING_CC_VAPSS_HCP0_CBCR:
    case TURING_CC_VAPSS_HCP1_CBCR:
    case TURING_CC_VAPSS_HCP_MSF_CBCR:
    case TURING_CC_VAPSS_HCP_MDC_CBCR:
    case TURING_CC_VAPSS_FINT_CBCR:
    case TURING_CC_VAPSS_FINT_PROG_CBCR:
    case TURING_CC_VAPSS_MVR_CBCR:
    case TURING_CC_VAPSS_MVR_PROG_CBCR:
    case TURING_CC_CENG_AHBS_CBCR:
    case TURING_CC_NSPNOC_AHBS_CBCR:
    case TURING_CC_DMA_AHBS_CBCR:
    case TURING_CC_UBWCD_AHBS_CBCR:
    case TURING_CC_NSPAUX_XO_CBCR:
    case TURING_CC_NSPNOC_CBCR:
    case TURING_CC_NSPNOC_ATB_CBCR:
    case TURING_CC_NSPNOC_APB_CBCR:
    case TURING_CC_CENG_NSP_CBCR:
    case TURING_CC_CENG_NSP_AO_CBCR:
    case TURING_CC_CENG_PROC_CBCR:
    case TURING_CC_UBWCD_CBCR:
    case TURING_CC_UBWCD_IPNOC_CBCR:
    case TURING_CC_UBWCD_MSF_CBCR:
    case TURING_CC_UBWCD_NSPNOC_CBCR:
    case TURING_CC_DMA_CBCR:
    case TURING_CC_DMA_MSF_UBWCDIP1_CBCR:
    case TURING_CC_DMA_MSF_TCMS_CBCR:
    case TURING_CC_TCMS_CBCR:
    case TURING_CC_Q6SS_AHBM_AON_CBCR:
    case TURING_CC_Q6SS_AHBS_AON_CBCR:
    case TURING_CC_Q6SS_AHBS_AON_MXC_CBCR:
    case TURING_CC_Q6SS_ALT_RESET_AON_CBCR:
    case TURING_CC_Q6SS_LLM_CURR_SSC_CBCR:
    case TURING_CC_Q6SS_LLM2_CURR_SSC_CBCR:
    case TURING_CC_Q6SS_LLM_MXCDPM_CURR_SSC_CBCR:
    case TURING_CC_Q6SS_LLM2_MXCDPM_CURR_SSC_CBCR:
    case TURING_CC_Q6SS_Q6_AXIM_CBCR:
    case TURING_CC_Q6SS_AXIS2_CBCR:
    case TURING_CC_Q6SS_LMH_CBCR:
    case TURING_CC_Q6SS_LLM_TEMP_SSC_CBCR:
        return s->regs[R(addr)] & ~(1u << 31);

    /* GDSCR — force PWR_ON (bit 31) on read */
    case TURING_CC_VAPSS_GDSCR:
    case TURING_CC_NSPAUX_GDSCR:
        return s->regs[R(addr)] | (1u << 31);

    /* RO */
    case TURING_CC_VAPSS_HW_CTRL_DVM_STATUS_GDSR:
    case TURING_CC_VAPSS_HW_CTRL_HALT1_STATUS_GDSR:
    case TURING_CC_VAPSS_HW_CTRL_HALT2_STATUS_GDSR:
    case TURING_CC_VAPSS_HW_CTRL_IRQ_STATUS_GDSR:
    case TURING_CC_NSPAUX_HW_CTRL_DVM_STATUS_GDSR:
    case TURING_CC_NSPAUX_HW_CTRL_HALT1_STATUS_GDSR:
    case TURING_CC_NSPAUX_HW_CTRL_HALT2_STATUS_GDSR:
    case TURING_CC_NSPAUX_HW_CTRL_IRQ_STATUS_GDSR:
        return s->regs[R(addr)];

    /* RW */
    case TURING_CC_AON_CFG_RCGR:
    case TURING_CC_AON_DCD_DIV_DCDR:
    case TURING_CC_TCC_CFG_RCGR:
    case TURING_CC_TCC_DIV_CDIVR:
    case TURING_CC_CXO_CFG_RCGR:
    case TURING_CC_CXO_DIV_CDIVR:
    case TURING_CC_VAPSS_BCR:
    case TURING_CC_VAPSS_CFG_GDSCR:
    case TURING_CC_VAPSS_CFG2_GDSCR:
    case TURING_CC_VAPSS_CFG3_GDSCR:
    case TURING_CC_VAPSS_CFG4_GDSCR:
    case TURING_CC_VAPSS_HW_CTRL_CFG1_GDSR:
    case TURING_CC_VAPSS_HW_CTRL_CFG2_GDSR:
    case TURING_CC_VAPSS_HW_CTRL_REQ_SW_GDSR:
    case TURING_CC_VAPSS_HW_CTRL_IRQ_MASK_GDSR:
    case TURING_CC_VAPSS_HW_CTRL_IRQ_CLEAR_GDSR:
    case TURING_CC_VAPSS_HW_CTRL_REQ_SPARE_GDSR:
    case TURING_CC_VAPSS_AXI_SREGR:
    case TURING_CC_VAPSS_AXI_CFG_SREGR:
    case TURING_CC_VAPSS_AXI_CFG2_SREGR:
    case TURING_CC_VAPSS_VMA_CFG_RCGR:
    case TURING_CC_VAPSS_VMA_AACSR:
    case TURING_CC_VAPSS_VMA_DCD_DIV_DCDR:
    case TURING_CC_VAPSS_VMA_SREGR:
    case TURING_CC_VAPSS_VMA_CFG_SREGR:
    case TURING_CC_VAPSS_VMA_CFG2_SREGR:
    case TURING_CC_VAPSS_HCP_CFG_RCGR:
    case TURING_CC_VAPSS_HCP_AACSR:
    case TURING_CC_VAPSS_HCP_DCD_DIV_DCDR:
    case TURING_CC_VAPSS_HCP_SREGR:
    case TURING_CC_VAPSS_HCP_CFG_SREGR:
    case TURING_CC_VAPSS_HCP_CFG2_SREGR:
    case TURING_CC_VAPSS_HCP_DIV_CDIVR:
    case TURING_CC_VAPSS_HCP_MDC_SREGR:
    case TURING_CC_VAPSS_HCP_MDC_CFG_SREGR:
    case TURING_CC_VAPSS_HCP_MDC_CFG2_SREGR:
    case TURING_CC_VAPSS_FINT_CFG_RCGR:
    case TURING_CC_VAPSS_FINT_AACSR:
    case TURING_CC_VAPSS_FINT_DCD_DIV_DCDR:
    case TURING_CC_VAPSS_FINT_SREGR:
    case TURING_CC_VAPSS_FINT_CFG_SREGR:
    case TURING_CC_VAPSS_FINT_CFG2_SREGR:
    case TURING_CC_VAPSS_MVR_CFG_RCGR:
    case TURING_CC_VAPSS_MVR_AACSR:
    case TURING_CC_VAPSS_MVR_DCD_DIV_DCDR:
    case TURING_CC_VAPSS_MVR_SREGR:
    case TURING_CC_VAPSS_MVR_CFG_SREGR:
    case TURING_CC_VAPSS_MVR_CFG2_SREGR:
    case TURING_CC_NSPAUX_BCR:
    case TURING_CC_NSPAUX_CFG_GDSCR:
    case TURING_CC_NSPAUX_CFG2_GDSCR:
    case TURING_CC_NSPAUX_CFG3_GDSCR:
    case TURING_CC_NSPAUX_CFG4_GDSCR:
    case TURING_CC_NSPAUX_HW_CTRL_CFG1_GDSR:
    case TURING_CC_NSPAUX_HW_CTRL_CFG2_GDSR:
    case TURING_CC_NSPAUX_HW_CTRL_REQ_SW_GDSR:
    case TURING_CC_NSPAUX_HW_CTRL_IRQ_MASK_GDSR:
    case TURING_CC_NSPAUX_HW_CTRL_IRQ_CLEAR_GDSR:
    case TURING_CC_NSPAUX_HW_CTRL_REQ_SPARE_GDSR:
    case TURING_CC_NSPNOC_SREGR:
    case TURING_CC_NSPNOC_CFG_SREGR:
    case TURING_CC_NSPNOC_CFG2_SREGR:
    case TURING_CC_CENG_CFG_RCGR:
    case TURING_CC_CENG_AACSR:
    case TURING_CC_CENG_DCD_DIV_DCDR:
    case TURING_CC_CENG_DIV_CDIVR:
    case TURING_CC_CENG_PROC_SREGR:
    case TURING_CC_CENG_PROC_CFG_SREGR:
    case TURING_CC_CENG_PROC_CFG2_SREGR:
    case TURING_CC_UBWCD_CFG_RCGR:
    case TURING_CC_UBWCD_AACSR:
    case TURING_CC_UBWCD_DCD_DIV_DCDR:
    case TURING_CC_UBWCD_SREGR:
    case TURING_CC_UBWCD_CFG_SREGR:
    case TURING_CC_UBWCD_CFG2_SREGR:
    case TURING_CC_UBWCD_IPNOC_SREGR:
    case TURING_CC_UBWCD_IPNOC_CFG_SREGR:
    case TURING_CC_UBWCD_IPNOC_CFG2_SREGR:
    case TURING_CC_DMA_CFG_RCGR:
    case TURING_CC_DMA_AACSR:
    case TURING_CC_DMA_DCD_DIV_DCDR:
    case TURING_CC_DMA_SREGR:
    case TURING_CC_DMA_CFG_SREGR:
    case TURING_CC_DMA_CFG2_SREGR:
    case TURING_CC_TCMS_CFG_RCGR:
    case TURING_CC_TCMS_AACSR:
    case TURING_CC_TCMS_DCD_DIV_DCDR:
    case TURING_CC_TCMS_SREGR:
    case TURING_CC_TCMS_CFG_SREGR:
    case TURING_CC_TCMS_CFG2_SREGR:
    case TURING_CC_Q6SS_BCR:
    case TURING_CC_Q6SS_LMH_CFG_RCGR:
        return s->regs[R(addr)];

    default:
        turing_cc_bad_access("read", addr, size);
        return 0;
    }
}

static void turing_cc_write(void *opaque, hwaddr addr, uint64_t value,
                            unsigned size)
{
    TuringCcState *s = opaque;
    uint32_t v = (uint32_t)value;

    if (size != 4) {
        turing_cc_bad_access("write", addr, size);
        return;
    }

    switch (addr) {
    /* RO registers — reject writes */
    case TURING_CC_VAPSS_HW_CTRL_DVM_STATUS_GDSR:
    case TURING_CC_VAPSS_HW_CTRL_HALT1_STATUS_GDSR:
    case TURING_CC_VAPSS_HW_CTRL_HALT2_STATUS_GDSR:
    case TURING_CC_VAPSS_HW_CTRL_IRQ_STATUS_GDSR:
    case TURING_CC_NSPAUX_HW_CTRL_DVM_STATUS_GDSR:
    case TURING_CC_NSPAUX_HW_CTRL_HALT1_STATUS_GDSR:
    case TURING_CC_NSPAUX_HW_CTRL_HALT2_STATUS_GDSR:
    case TURING_CC_NSPAUX_HW_CTRL_IRQ_STATUS_GDSR:
        qemu_log_mask(LOG_GUEST_ERROR,
                      TYPE_TURING_CC ": write to RO reg 0x%" HWADDR_PRIx "\n",
                      addr);
        break;

    /* CMD_RCGR — clear ROOT_OFF and UPDATE on trigger */
    case TURING_CC_AON_CMD_RCGR:
    case TURING_CC_TCC_CMD_RCGR:
    case TURING_CC_CXO_CMD_RCGR:
    case TURING_CC_VAPSS_VMA_CMD_RCGR:
    case TURING_CC_VAPSS_HCP_CMD_RCGR:
    case TURING_CC_VAPSS_FINT_CMD_RCGR:
    case TURING_CC_VAPSS_MVR_CMD_RCGR:
    case TURING_CC_CENG_CMD_RCGR:
    case TURING_CC_UBWCD_CMD_RCGR:
    case TURING_CC_DMA_CMD_RCGR:
    case TURING_CC_TCMS_CMD_RCGR:
    case TURING_CC_Q6SS_LMH_CMD_RCGR:
        s->regs[R(addr)] = v;
        if (v & 0x1u) {
            s->regs[R(addr)] &= ~((1u << 31) | 0x1u);
        }
        break;

    /* CBCR */
    case TURING_CC_WRAPPER_AON_CBCR:
    case TURING_CC_AHB_MXC_CBCR:
    case TURING_CC_WRAPPER_CNOC_SWAY_AON_CBCR:
    case TURING_CC_WRAPPER_BUS_TIMEOUT_AON_CBCR:
    case TURING_CC_WRAPPER_RSCC_AON_CBCR:
    case TURING_CC_TCC_CBCR:
    case TURING_CC_TCC_MXC_CBCR:
    case TURING_CC_TCC_DIV_CBCR:
    case TURING_CC_XO_CBCR:
    case TURING_CC_XO_DIV_CBCR:
    case TURING_CC_VAPSS_AHBS_TIMEOUT_CBCR:
    case TURING_CC_VAPSS_VMA_AHBS_CBCR:
    case TURING_CC_VAPSS_HCP_AHBS_CBCR:
    case TURING_CC_VAPSS_AHBS_AON_CBCR:
    case TURING_CC_VAPSS_XO_CBCR:
    case TURING_CC_VAPSS_AXI_CBCR:
    case TURING_CC_VAPSS_ATB_CBCR:
    case TURING_CC_VAPSS_APB_CBCR:
    case TURING_CC_VAPSS_VMA_CBCR:
    case TURING_CC_VAPSS_VMA_MSF_CBCR:
    case TURING_CC_VAPSS_HCP_CBCR:
    case TURING_CC_VAPSS_HCP0_CBCR:
    case TURING_CC_VAPSS_HCP1_CBCR:
    case TURING_CC_VAPSS_HCP_MSF_CBCR:
    case TURING_CC_VAPSS_HCP_MDC_CBCR:
    case TURING_CC_VAPSS_FINT_CBCR:
    case TURING_CC_VAPSS_FINT_PROG_CBCR:
    case TURING_CC_VAPSS_MVR_CBCR:
    case TURING_CC_VAPSS_MVR_PROG_CBCR:
    case TURING_CC_CENG_AHBS_CBCR:
    case TURING_CC_NSPNOC_AHBS_CBCR:
    case TURING_CC_DMA_AHBS_CBCR:
    case TURING_CC_UBWCD_AHBS_CBCR:
    case TURING_CC_NSPAUX_XO_CBCR:
    case TURING_CC_NSPNOC_CBCR:
    case TURING_CC_NSPNOC_ATB_CBCR:
    case TURING_CC_NSPNOC_APB_CBCR:
    case TURING_CC_CENG_NSP_CBCR:
    case TURING_CC_CENG_NSP_AO_CBCR:
    case TURING_CC_CENG_PROC_CBCR:
    case TURING_CC_UBWCD_CBCR:
    case TURING_CC_UBWCD_IPNOC_CBCR:
    case TURING_CC_UBWCD_MSF_CBCR:
    case TURING_CC_UBWCD_NSPNOC_CBCR:
    case TURING_CC_DMA_CBCR:
    case TURING_CC_DMA_MSF_UBWCDIP1_CBCR:
    case TURING_CC_DMA_MSF_TCMS_CBCR:
    case TURING_CC_TCMS_CBCR:
    case TURING_CC_Q6SS_AHBM_AON_CBCR:
    case TURING_CC_Q6SS_AHBS_AON_CBCR:
    case TURING_CC_Q6SS_AHBS_AON_MXC_CBCR:
    case TURING_CC_Q6SS_ALT_RESET_AON_CBCR:
    case TURING_CC_Q6SS_LLM_CURR_SSC_CBCR:
    case TURING_CC_Q6SS_LLM2_CURR_SSC_CBCR:
    case TURING_CC_Q6SS_LLM_MXCDPM_CURR_SSC_CBCR:
    case TURING_CC_Q6SS_LLM2_MXCDPM_CURR_SSC_CBCR:
    case TURING_CC_Q6SS_Q6_AXIM_CBCR:
    case TURING_CC_Q6SS_AXIS2_CBCR:
    case TURING_CC_Q6SS_LMH_CBCR:
    case TURING_CC_Q6SS_LLM_TEMP_SSC_CBCR:
    /* GDSCR */
    case TURING_CC_VAPSS_GDSCR:
    case TURING_CC_NSPAUX_GDSCR:
    /* RW */
    case TURING_CC_AON_CFG_RCGR:
    case TURING_CC_AON_DCD_DIV_DCDR:
    case TURING_CC_TCC_CFG_RCGR:
    case TURING_CC_TCC_DIV_CDIVR:
    case TURING_CC_CXO_CFG_RCGR:
    case TURING_CC_CXO_DIV_CDIVR:
    case TURING_CC_VAPSS_BCR:
    case TURING_CC_VAPSS_CFG_GDSCR:
    case TURING_CC_VAPSS_CFG2_GDSCR:
    case TURING_CC_VAPSS_CFG3_GDSCR:
    case TURING_CC_VAPSS_CFG4_GDSCR:
    case TURING_CC_VAPSS_HW_CTRL_CFG1_GDSR:
    case TURING_CC_VAPSS_HW_CTRL_CFG2_GDSR:
    case TURING_CC_VAPSS_HW_CTRL_REQ_SW_GDSR:
    case TURING_CC_VAPSS_HW_CTRL_IRQ_MASK_GDSR:
    case TURING_CC_VAPSS_HW_CTRL_IRQ_CLEAR_GDSR:
    case TURING_CC_VAPSS_HW_CTRL_REQ_SPARE_GDSR:
    case TURING_CC_VAPSS_AXI_SREGR:
    case TURING_CC_VAPSS_AXI_CFG_SREGR:
    case TURING_CC_VAPSS_AXI_CFG2_SREGR:
    case TURING_CC_VAPSS_VMA_CFG_RCGR:
    case TURING_CC_VAPSS_VMA_AACSR:
    case TURING_CC_VAPSS_VMA_DCD_DIV_DCDR:
    case TURING_CC_VAPSS_VMA_SREGR:
    case TURING_CC_VAPSS_VMA_CFG_SREGR:
    case TURING_CC_VAPSS_VMA_CFG2_SREGR:
    case TURING_CC_VAPSS_HCP_CFG_RCGR:
    case TURING_CC_VAPSS_HCP_AACSR:
    case TURING_CC_VAPSS_HCP_DCD_DIV_DCDR:
    case TURING_CC_VAPSS_HCP_SREGR:
    case TURING_CC_VAPSS_HCP_CFG_SREGR:
    case TURING_CC_VAPSS_HCP_CFG2_SREGR:
    case TURING_CC_VAPSS_HCP_DIV_CDIVR:
    case TURING_CC_VAPSS_HCP_MDC_SREGR:
    case TURING_CC_VAPSS_HCP_MDC_CFG_SREGR:
    case TURING_CC_VAPSS_HCP_MDC_CFG2_SREGR:
    case TURING_CC_VAPSS_FINT_CFG_RCGR:
    case TURING_CC_VAPSS_FINT_AACSR:
    case TURING_CC_VAPSS_FINT_DCD_DIV_DCDR:
    case TURING_CC_VAPSS_FINT_SREGR:
    case TURING_CC_VAPSS_FINT_CFG_SREGR:
    case TURING_CC_VAPSS_FINT_CFG2_SREGR:
    case TURING_CC_VAPSS_MVR_CFG_RCGR:
    case TURING_CC_VAPSS_MVR_AACSR:
    case TURING_CC_VAPSS_MVR_DCD_DIV_DCDR:
    case TURING_CC_VAPSS_MVR_SREGR:
    case TURING_CC_VAPSS_MVR_CFG_SREGR:
    case TURING_CC_VAPSS_MVR_CFG2_SREGR:
    case TURING_CC_NSPAUX_BCR:
    case TURING_CC_NSPAUX_CFG_GDSCR:
    case TURING_CC_NSPAUX_CFG2_GDSCR:
    case TURING_CC_NSPAUX_CFG3_GDSCR:
    case TURING_CC_NSPAUX_CFG4_GDSCR:
    case TURING_CC_NSPAUX_HW_CTRL_CFG1_GDSR:
    case TURING_CC_NSPAUX_HW_CTRL_CFG2_GDSR:
    case TURING_CC_NSPAUX_HW_CTRL_REQ_SW_GDSR:
    case TURING_CC_NSPAUX_HW_CTRL_IRQ_MASK_GDSR:
    case TURING_CC_NSPAUX_HW_CTRL_IRQ_CLEAR_GDSR:
    case TURING_CC_NSPAUX_HW_CTRL_REQ_SPARE_GDSR:
    case TURING_CC_NSPNOC_SREGR:
    case TURING_CC_NSPNOC_CFG_SREGR:
    case TURING_CC_NSPNOC_CFG2_SREGR:
    case TURING_CC_CENG_CFG_RCGR:
    case TURING_CC_CENG_AACSR:
    case TURING_CC_CENG_DCD_DIV_DCDR:
    case TURING_CC_CENG_DIV_CDIVR:
    case TURING_CC_CENG_PROC_SREGR:
    case TURING_CC_CENG_PROC_CFG_SREGR:
    case TURING_CC_CENG_PROC_CFG2_SREGR:
    case TURING_CC_UBWCD_CFG_RCGR:
    case TURING_CC_UBWCD_AACSR:
    case TURING_CC_UBWCD_DCD_DIV_DCDR:
    case TURING_CC_UBWCD_SREGR:
    case TURING_CC_UBWCD_CFG_SREGR:
    case TURING_CC_UBWCD_CFG2_SREGR:
    case TURING_CC_UBWCD_IPNOC_SREGR:
    case TURING_CC_UBWCD_IPNOC_CFG_SREGR:
    case TURING_CC_UBWCD_IPNOC_CFG2_SREGR:
    case TURING_CC_DMA_CFG_RCGR:
    case TURING_CC_DMA_AACSR:
    case TURING_CC_DMA_DCD_DIV_DCDR:
    case TURING_CC_DMA_SREGR:
    case TURING_CC_DMA_CFG_SREGR:
    case TURING_CC_DMA_CFG2_SREGR:
    case TURING_CC_TCMS_CFG_RCGR:
    case TURING_CC_TCMS_AACSR:
    case TURING_CC_TCMS_DCD_DIV_DCDR:
    case TURING_CC_TCMS_SREGR:
    case TURING_CC_TCMS_CFG_SREGR:
    case TURING_CC_TCMS_CFG2_SREGR:
    case TURING_CC_Q6SS_BCR:
    case TURING_CC_Q6SS_LMH_CFG_RCGR:
        s->regs[R(addr)] = v;
        break;

    default:
        turing_cc_bad_access("write", addr, size);
        break;
    }
}

static const MemoryRegionOps turing_cc_ops = {
    .read = turing_cc_read,
    .write = turing_cc_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = { .min_access_size = 4, .max_access_size = 4 },
};

static void turing_cc_reset_hold(Object *obj, ResetType type)
{
    TuringCcState *s = TURING_CC(obj);

    memset(s->regs, 0, sizeof(s->regs));

    /* AON */
    s->regs[R(TURING_CC_AON_CFG_RCGR)]                  = 0x00100000;
    s->regs[R(TURING_CC_AON_DCD_DIV_DCDR)]              = 0x000000C1;
    s->regs[R(TURING_CC_WRAPPER_AON_CBCR)]              = 0x08000001;
    s->regs[R(TURING_CC_AHB_MXC_CBCR)]                  = 0x08000001;
    s->regs[R(TURING_CC_WRAPPER_CNOC_SWAY_AON_CBCR)]    = 0x08000001;
    s->regs[R(TURING_CC_WRAPPER_BUS_TIMEOUT_AON_CBCR)]  = 0x08000001;
    s->regs[R(TURING_CC_WRAPPER_RSCC_AON_CBCR)]         = 0x08000001;

    /* TCC */
    s->regs[R(TURING_CC_TCC_CFG_RCGR)]                  = 0x00100000;
    s->regs[R(TURING_CC_TCC_CBCR)]                      = 0x08000001;
    s->regs[R(TURING_CC_TCC_MXC_CBCR)]                  = 0x08000001;
    s->regs[R(TURING_CC_TCC_DIV_CDIVR)]                 = 0x00000030;
    s->regs[R(TURING_CC_TCC_DIV_CBCR)]                  = 0x08000001;

    /* CXO */
    s->regs[R(TURING_CC_CXO_CFG_RCGR)]                  = 0x00100000;
    s->regs[R(TURING_CC_XO_CBCR)]                       = 0x08000001;
    s->regs[R(TURING_CC_CXO_DIV_CDIVR)]                 = 0x000001FF;
    s->regs[R(TURING_CC_XO_DIV_CBCR)]                   = 0x08000001;

    /* VAPSS */
    s->regs[R(TURING_CC_VAPSS_GDSCR)]                   = 0x02227801;
    s->regs[R(TURING_CC_VAPSS_CFG_GDSCR)]               = 0x04088000;
    s->regs[R(TURING_CC_VAPSS_CFG2_GDSCR)]              = 0x0002022F;
    s->regs[R(TURING_CC_VAPSS_CFG3_GDSCR)]              = 0x03F00000;
    s->regs[R(TURING_CC_VAPSS_CFG4_GDSCR)]              = 0x00222222;
    s->regs[R(TURING_CC_VAPSS_HW_CTRL_CFG1_GDSR)]       = 0x2A000102;
    s->regs[R(TURING_CC_VAPSS_HW_CTRL_CFG2_GDSR)]       = 0x000F400D;
    s->regs[R(TURING_CC_VAPSS_HW_CTRL_IRQ_MASK_GDSR)]   = 0x00000001;
    s->regs[R(TURING_CC_VAPSS_AHBS_TIMEOUT_CBCR)]       = 0x88000000;
    s->regs[R(TURING_CC_VAPSS_VMA_AHBS_CBCR)]           = 0x88000000;
    s->regs[R(TURING_CC_VAPSS_HCP_AHBS_CBCR)]           = 0x88000000;
    s->regs[R(TURING_CC_VAPSS_AHBS_AON_CBCR)]           = 0x88000000;
    s->regs[R(TURING_CC_VAPSS_XO_CBCR)]                 = 0x88000000;
    s->regs[R(TURING_CC_VAPSS_AXI_CBCR)]                = 0x88000000;
    s->regs[R(TURING_CC_VAPSS_AXI_SREGR)]               = 0x02110001;
    s->regs[R(TURING_CC_VAPSS_AXI_CFG_SREGR)]           = 0x18080306;
    s->regs[R(TURING_CC_VAPSS_ATB_CBCR)]                = 0x88000000;
    s->regs[R(TURING_CC_VAPSS_APB_CBCR)]                = 0x88000000;
    s->regs[R(TURING_CC_VAPSS_VMA_CMD_RCGR)]            = 0x80000000;
    s->regs[R(TURING_CC_VAPSS_VMA_CFG_RCGR)]            = 0x00100000;
    s->regs[R(TURING_CC_VAPSS_VMA_AACSR)]               = 0x00000002;
    s->regs[R(TURING_CC_VAPSS_VMA_DCD_DIV_DCDR)]        = 0x000000C1;
    s->regs[R(TURING_CC_VAPSS_VMA_CBCR)]                = 0x88000000;
    s->regs[R(TURING_CC_VAPSS_VMA_SREGR)]               = 0x02110001;
    s->regs[R(TURING_CC_VAPSS_VMA_CFG_SREGR)]           = 0x18080306;
    s->regs[R(TURING_CC_VAPSS_VMA_MSF_CBCR)]            = 0x88000000;
    s->regs[R(TURING_CC_VAPSS_HCP_CMD_RCGR)]            = 0x80000000;
    s->regs[R(TURING_CC_VAPSS_HCP_CFG_RCGR)]            = 0x00100000;
    s->regs[R(TURING_CC_VAPSS_HCP_AACSR)]               = 0x00000002;
    s->regs[R(TURING_CC_VAPSS_HCP_DCD_DIV_DCDR)]        = 0x000000C1;
    s->regs[R(TURING_CC_VAPSS_HCP_CBCR)]                = 0x88000000;
    s->regs[R(TURING_CC_VAPSS_HCP_SREGR)]               = 0x02110001;
    s->regs[R(TURING_CC_VAPSS_HCP_CFG_SREGR)]           = 0x18080306;
    s->regs[R(TURING_CC_VAPSS_HCP0_CBCR)]               = 0x88000000;
    s->regs[R(TURING_CC_VAPSS_HCP1_CBCR)]               = 0x88000000;
    s->regs[R(TURING_CC_VAPSS_HCP_MSF_CBCR)]            = 0x88000000;
    s->regs[R(TURING_CC_VAPSS_HCP_DIV_CDIVR)]           = 0x00000001;
    s->regs[R(TURING_CC_VAPSS_HCP_MDC_CBCR)]            = 0x88000000;
    s->regs[R(TURING_CC_VAPSS_HCP_MDC_SREGR)]           = 0x02110001;
    s->regs[R(TURING_CC_VAPSS_HCP_MDC_CFG_SREGR)]       = 0x18080306;
    s->regs[R(TURING_CC_VAPSS_FINT_CMD_RCGR)]           = 0x80000000;
    s->regs[R(TURING_CC_VAPSS_FINT_CFG_RCGR)]           = 0x00100000;
    s->regs[R(TURING_CC_VAPSS_FINT_AACSR)]              = 0x00000002;
    s->regs[R(TURING_CC_VAPSS_FINT_DCD_DIV_DCDR)]       = 0x000000C1;
    s->regs[R(TURING_CC_VAPSS_FINT_CBCR)]               = 0x88000000;
    s->regs[R(TURING_CC_VAPSS_FINT_SREGR)]              = 0x02110001;
    s->regs[R(TURING_CC_VAPSS_FINT_CFG_SREGR)]          = 0x18080306;
    s->regs[R(TURING_CC_VAPSS_FINT_PROG_CBCR)]          = 0x88000000;
    s->regs[R(TURING_CC_VAPSS_MVR_CMD_RCGR)]            = 0x80000000;
    s->regs[R(TURING_CC_VAPSS_MVR_CFG_RCGR)]            = 0x00100000;
    s->regs[R(TURING_CC_VAPSS_MVR_AACSR)]               = 0x00000002;
    s->regs[R(TURING_CC_VAPSS_MVR_DCD_DIV_DCDR)]        = 0x000000C1;
    s->regs[R(TURING_CC_VAPSS_MVR_CBCR)]                = 0x88000000;
    s->regs[R(TURING_CC_VAPSS_MVR_SREGR)]               = 0x02110001;
    s->regs[R(TURING_CC_VAPSS_MVR_CFG_SREGR)]           = 0x18080306;
    s->regs[R(TURING_CC_VAPSS_MVR_PROG_CBCR)]           = 0x88000000;

    /* NSPAUX */
    s->regs[R(TURING_CC_NSPAUX_GDSCR)]                  = 0x02227801;
    s->regs[R(TURING_CC_NSPAUX_CFG_GDSCR)]              = 0x04088000;
    s->regs[R(TURING_CC_NSPAUX_CFG2_GDSCR)]             = 0x0002022F;
    s->regs[R(TURING_CC_NSPAUX_CFG3_GDSCR)]             = 0x03F00000;
    s->regs[R(TURING_CC_NSPAUX_CFG4_GDSCR)]             = 0x00222222;
    s->regs[R(TURING_CC_NSPAUX_HW_CTRL_CFG1_GDSR)]      = 0x2A000112;
    s->regs[R(TURING_CC_NSPAUX_HW_CTRL_CFG2_GDSR)]      = 0x000F400D;
    s->regs[R(TURING_CC_NSPAUX_HW_CTRL_IRQ_MASK_GDSR)]  = 0x00000001;
    s->regs[R(TURING_CC_CENG_AHBS_CBCR)]                = 0x88000000;
    s->regs[R(TURING_CC_NSPNOC_AHBS_CBCR)]              = 0x88000000;
    s->regs[R(TURING_CC_DMA_AHBS_CBCR)]                 = 0x88000000;
    s->regs[R(TURING_CC_UBWCD_AHBS_CBCR)]               = 0x88000000;
    s->regs[R(TURING_CC_NSPAUX_XO_CBCR)]                = 0x88000000;
    s->regs[R(TURING_CC_NSPNOC_CBCR)]                   = 0x88000000;
    s->regs[R(TURING_CC_NSPNOC_SREGR)]                  = 0x02110001;
    s->regs[R(TURING_CC_NSPNOC_CFG_SREGR)]              = 0x18080306;
    s->regs[R(TURING_CC_NSPNOC_ATB_CBCR)]               = 0x88000000;
    s->regs[R(TURING_CC_NSPNOC_APB_CBCR)]               = 0x88000000;

    /* CENG */
    s->regs[R(TURING_CC_CENG_CMD_RCGR)]                 = 0x80000000;
    s->regs[R(TURING_CC_CENG_CFG_RCGR)]                 = 0x00100000;
    s->regs[R(TURING_CC_CENG_AACSR)]                    = 0x00000002;
    s->regs[R(TURING_CC_CENG_DCD_DIV_DCDR)]             = 0x000000C1;
    s->regs[R(TURING_CC_CENG_NSP_CBCR)]                 = 0x88000000;
    s->regs[R(TURING_CC_CENG_NSP_AO_CBCR)]              = 0x88000000;
    s->regs[R(TURING_CC_CENG_DIV_CDIVR)]                = 0x00000001;
    s->regs[R(TURING_CC_CENG_PROC_CBCR)]                = 0x88000000;
    s->regs[R(TURING_CC_CENG_PROC_SREGR)]               = 0x02110001;
    s->regs[R(TURING_CC_CENG_PROC_CFG_SREGR)]           = 0x18080306;

    /* UBWCD */
    s->regs[R(TURING_CC_UBWCD_CMD_RCGR)]                = 0x80000000;
    s->regs[R(TURING_CC_UBWCD_CFG_RCGR)]                = 0x00100000;
    s->regs[R(TURING_CC_UBWCD_AACSR)]                   = 0x00000002;
    s->regs[R(TURING_CC_UBWCD_DCD_DIV_DCDR)]            = 0x00000089;
    s->regs[R(TURING_CC_UBWCD_CBCR)]                    = 0x88000000;
    s->regs[R(TURING_CC_UBWCD_SREGR)]                   = 0x02110001;
    s->regs[R(TURING_CC_UBWCD_CFG_SREGR)]               = 0x18080306;
    s->regs[R(TURING_CC_UBWCD_IPNOC_CBCR)]              = 0x88000000;
    s->regs[R(TURING_CC_UBWCD_IPNOC_SREGR)]             = 0x02110001;
    s->regs[R(TURING_CC_UBWCD_IPNOC_CFG_SREGR)]         = 0x18080306;
    s->regs[R(TURING_CC_UBWCD_MSF_CBCR)]                = 0x88000000;
    s->regs[R(TURING_CC_UBWCD_NSPNOC_CBCR)]             = 0x88000000;

    /* DMA */
    s->regs[R(TURING_CC_DMA_CMD_RCGR)]                  = 0x80000000;
    s->regs[R(TURING_CC_DMA_CFG_RCGR)]                  = 0x00100000;
    s->regs[R(TURING_CC_DMA_AACSR)]                     = 0x00000002;
    s->regs[R(TURING_CC_DMA_DCD_DIV_DCDR)]              = 0x000000C1;
    s->regs[R(TURING_CC_DMA_CBCR)]                      = 0x88000000;
    s->regs[R(TURING_CC_DMA_SREGR)]                     = 0x02110001;
    s->regs[R(TURING_CC_DMA_CFG_SREGR)]                 = 0x18080306;
    s->regs[R(TURING_CC_DMA_MSF_UBWCDIP1_CBCR)]         = 0x88000000;
    s->regs[R(TURING_CC_DMA_MSF_TCMS_CBCR)]             = 0x88000000;

    /* TCMS */
    s->regs[R(TURING_CC_TCMS_CMD_RCGR)]                 = 0x80000000;
    s->regs[R(TURING_CC_TCMS_CFG_RCGR)]                 = 0x00100000;
    s->regs[R(TURING_CC_TCMS_AACSR)]                    = 0x00000002;
    s->regs[R(TURING_CC_TCMS_DCD_DIV_DCDR)]             = 0x000000C1;
    s->regs[R(TURING_CC_TCMS_CBCR)]                     = 0x88000000;
    s->regs[R(TURING_CC_TCMS_SREGR)]                    = 0x02110001;
    s->regs[R(TURING_CC_TCMS_CFG_SREGR)]                = 0x18080306;

    /* Q6SS */
    s->regs[R(TURING_CC_Q6SS_AHBM_AON_CBCR)]            = 0x88000000;
    s->regs[R(TURING_CC_Q6SS_AHBS_AON_CBCR)]            = 0x08000001;
    s->regs[R(TURING_CC_Q6SS_AHBS_AON_MXC_CBCR)]        = 0x88000000;
    s->regs[R(TURING_CC_Q6SS_ALT_RESET_AON_CBCR)]       = 0x88000000;
    s->regs[R(TURING_CC_Q6SS_LLM_CURR_SSC_CBCR)]        = 0x88000000;
    s->regs[R(TURING_CC_Q6SS_LLM2_CURR_SSC_CBCR)]       = 0x88000000;
    s->regs[R(TURING_CC_Q6SS_LLM_MXCDPM_CURR_SSC_CBCR)] = 0x88000000;
    s->regs[R(TURING_CC_Q6SS_LLM2_MXCDPM_CURR_SSC_CBCR)] = 0x88000000;
    s->regs[R(TURING_CC_Q6SS_Q6_AXIM_CBCR)]             = 0x88000000;
    s->regs[R(TURING_CC_Q6SS_AXIS2_CBCR)]               = 0x88000000;
    s->regs[R(TURING_CC_Q6SS_LMH_CMD_RCGR)]             = 0x80000000;
    s->regs[R(TURING_CC_Q6SS_LMH_CFG_RCGR)]             = 0x00100000;
    s->regs[R(TURING_CC_Q6SS_LMH_CBCR)]                 = 0x88000000;
    s->regs[R(TURING_CC_Q6SS_LLM_TEMP_SSC_CBCR)]        = 0x88000000;
}

static void turing_cc_realize(DeviceState *dev, Error **errp)
{
    TuringCcState *s = TURING_CC(dev);

    memory_region_init_io(&s->mmio, OBJECT(dev), &turing_cc_ops, s,
                          TYPE_TURING_CC, TURING_CC_MMIO_SIZE);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->mmio);
}

static const VMStateDescription vmstate_turing_cc = {
    .name = "turing-cc",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_UINT32_ARRAY(regs, TuringCcState, TURING_CC_NUM_REGS),
        VMSTATE_END_OF_LIST()
    }
};

static void turing_cc_class_init(ObjectClass *oc, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);
    ResettableClass *rc = RESETTABLE_CLASS(oc);
    dc->realize = turing_cc_realize;
    dc->vmsd = &vmstate_turing_cc;
    rc->phases.hold = turing_cc_reset_hold;
    dc->desc = "Turing Clock Controller (TURING_CC)";
}

static const TypeInfo turing_cc_info = {
    .name = TYPE_TURING_CC,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(TuringCcState),
    .class_init = turing_cc_class_init,
};

static void turing_cc_register_types(void)
{
    type_register_static(&turing_cc_info);
}

type_init(turing_cc_register_types)
