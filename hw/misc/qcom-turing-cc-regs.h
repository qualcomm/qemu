/*
 * Turing Clock Controller (TURING_CC) — QEMU sysbus device
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef HW_MISC_TURING_CC_REGS_H
#define HW_MISC_TURING_CC_REGS_H

#include "hw/core/sysbus.h"

#define TYPE_TURING_CC "turing-cc"
OBJECT_DECLARE_SIMPLE_TYPE(TuringCcState, TURING_CC)

#define TURING_CC_MMIO_SIZE 0x1000

/* Register offsets */
#define TURING_CC_AON_CMD_RCGR                       0x000
#define TURING_CC_AON_CFG_RCGR                       0x004
#define TURING_CC_AON_DCD_DIV_DCDR                   0x008
#define TURING_CC_WRAPPER_AON_CBCR                   0x00C
#define TURING_CC_AHB_MXC_CBCR                       0x010
#define TURING_CC_WRAPPER_CNOC_SWAY_AON_CBCR         0x014
#define TURING_CC_WRAPPER_BUS_TIMEOUT_AON_CBCR       0x018
#define TURING_CC_WRAPPER_RSCC_AON_CBCR              0x01C
#define TURING_CC_TCC_CMD_RCGR                       0x020
#define TURING_CC_TCC_CFG_RCGR                       0x024
#define TURING_CC_TCC_CBCR                           0x028
#define TURING_CC_TCC_MXC_CBCR                       0x02C
#define TURING_CC_TCC_DIV_CDIVR                      0x030
#define TURING_CC_TCC_DIV_CBCR                       0x034
#define TURING_CC_CXO_CMD_RCGR                       0x040
#define TURING_CC_CXO_CFG_RCGR                       0x044
#define TURING_CC_XO_CBCR                            0x048
#define TURING_CC_CXO_DIV_CDIVR                      0x04C
#define TURING_CC_XO_DIV_CBCR                        0x050
#define TURING_CC_VAPSS_BCR                          0x080
#define TURING_CC_VAPSS_GDSCR                        0x084
#define TURING_CC_VAPSS_CFG_GDSCR                    0x088
#define TURING_CC_VAPSS_CFG2_GDSCR                   0x08C
#define TURING_CC_VAPSS_CFG3_GDSCR                   0x090
#define TURING_CC_VAPSS_CFG4_GDSCR                   0x094
#define TURING_CC_VAPSS_HW_CTRL_CFG1_GDSR            0x098
#define TURING_CC_VAPSS_HW_CTRL_CFG2_GDSR            0x09C
#define TURING_CC_VAPSS_HW_CTRL_DVM_STATUS_GDSR      0x0A0
#define TURING_CC_VAPSS_HW_CTRL_HALT1_STATUS_GDSR    0x0A4
#define TURING_CC_VAPSS_HW_CTRL_HALT2_STATUS_GDSR    0x0A8
#define TURING_CC_VAPSS_HW_CTRL_REQ_SW_GDSR          0x0AC
#define TURING_CC_VAPSS_HW_CTRL_IRQ_STATUS_GDSR      0x0B0
#define TURING_CC_VAPSS_HW_CTRL_IRQ_MASK_GDSR        0x0B4
#define TURING_CC_VAPSS_HW_CTRL_IRQ_CLEAR_GDSR       0x0B8
#define TURING_CC_VAPSS_HW_CTRL_REQ_SPARE_GDSR       0x0BC
#define TURING_CC_VAPSS_AHBS_TIMEOUT_CBCR            0x0C0
#define TURING_CC_VAPSS_VMA_AHBS_CBCR                0x0C4
#define TURING_CC_VAPSS_HCP_AHBS_CBCR                0x0C8
#define TURING_CC_VAPSS_AHBS_AON_CBCR                0x0CC
#define TURING_CC_VAPSS_XO_CBCR                      0x0D0
#define TURING_CC_VAPSS_AXI_CBCR                     0x0D4
#define TURING_CC_VAPSS_AXI_SREGR                    0x0D8
#define TURING_CC_VAPSS_AXI_CFG_SREGR                0x0DC
#define TURING_CC_VAPSS_AXI_CFG2_SREGR               0x0E0
#define TURING_CC_VAPSS_ATB_CBCR                     0x0E4
#define TURING_CC_VAPSS_APB_CBCR                     0x0E8
#define TURING_CC_VAPSS_VMA_CMD_RCGR                 0x0EC
#define TURING_CC_VAPSS_VMA_CFG_RCGR                 0x0F0
#define TURING_CC_VAPSS_VMA_AACSR                    0x100
#define TURING_CC_VAPSS_VMA_DCD_DIV_DCDR             0x104
#define TURING_CC_VAPSS_VMA_CBCR                     0x108
#define TURING_CC_VAPSS_VMA_SREGR                    0x10C
#define TURING_CC_VAPSS_VMA_CFG_SREGR                0x110
#define TURING_CC_VAPSS_VMA_CFG2_SREGR               0x114
#define TURING_CC_VAPSS_VMA_MSF_CBCR                 0x118
#define TURING_CC_VAPSS_HCP_CMD_RCGR                 0x11C
#define TURING_CC_VAPSS_HCP_CFG_RCGR                 0x120
#define TURING_CC_VAPSS_HCP_AACSR                    0x130
#define TURING_CC_VAPSS_HCP_DCD_DIV_DCDR             0x134
#define TURING_CC_VAPSS_HCP_CBCR                     0x138
#define TURING_CC_VAPSS_HCP_SREGR                    0x13C
#define TURING_CC_VAPSS_HCP_CFG_SREGR                0x140
#define TURING_CC_VAPSS_HCP_CFG2_SREGR               0x144
#define TURING_CC_VAPSS_HCP0_CBCR                    0x148
#define TURING_CC_VAPSS_HCP1_CBCR                    0x14C
#define TURING_CC_VAPSS_HCP_MSF_CBCR                 0x150
#define TURING_CC_VAPSS_HCP_DIV_CDIVR                0x154
#define TURING_CC_VAPSS_HCP_MDC_CBCR                 0x158
#define TURING_CC_VAPSS_HCP_MDC_SREGR                0x15C
#define TURING_CC_VAPSS_HCP_MDC_CFG_SREGR            0x160
#define TURING_CC_VAPSS_HCP_MDC_CFG2_SREGR           0x164
#define TURING_CC_VAPSS_FINT_CMD_RCGR                0x168
#define TURING_CC_VAPSS_FINT_CFG_RCGR                0x16C
#define TURING_CC_VAPSS_FINT_AACSR                   0x17C
#define TURING_CC_VAPSS_FINT_DCD_DIV_DCDR            0x180
#define TURING_CC_VAPSS_FINT_CBCR                    0x184
#define TURING_CC_VAPSS_FINT_SREGR                   0x188
#define TURING_CC_VAPSS_FINT_CFG_SREGR               0x18C
#define TURING_CC_VAPSS_FINT_CFG2_SREGR              0x190
#define TURING_CC_VAPSS_FINT_PROG_CBCR               0x194
#define TURING_CC_VAPSS_MVR_CMD_RCGR                 0x198
#define TURING_CC_VAPSS_MVR_CFG_RCGR                 0x19C
#define TURING_CC_VAPSS_MVR_AACSR                    0x1AC
#define TURING_CC_VAPSS_MVR_DCD_DIV_DCDR             0x1B0
#define TURING_CC_VAPSS_MVR_CBCR                     0x1B4
#define TURING_CC_VAPSS_MVR_SREGR                    0x1B8
#define TURING_CC_VAPSS_MVR_CFG_SREGR                0x1BC
#define TURING_CC_VAPSS_MVR_CFG2_SREGR               0x1C0
#define TURING_CC_VAPSS_MVR_PROG_CBCR                0x1C4
#define TURING_CC_NSPAUX_BCR                         0x200
#define TURING_CC_NSPAUX_GDSCR                       0x204
#define TURING_CC_NSPAUX_CFG_GDSCR                   0x208
#define TURING_CC_NSPAUX_CFG2_GDSCR                  0x20C
#define TURING_CC_NSPAUX_CFG3_GDSCR                  0x210
#define TURING_CC_NSPAUX_CFG4_GDSCR                  0x214
#define TURING_CC_NSPAUX_HW_CTRL_CFG1_GDSR           0x218
#define TURING_CC_NSPAUX_HW_CTRL_CFG2_GDSR           0x21C
#define TURING_CC_NSPAUX_HW_CTRL_DVM_STATUS_GDSR     0x220
#define TURING_CC_NSPAUX_HW_CTRL_HALT1_STATUS_GDSR   0x224
#define TURING_CC_NSPAUX_HW_CTRL_HALT2_STATUS_GDSR   0x228
#define TURING_CC_NSPAUX_HW_CTRL_REQ_SW_GDSR         0x22C
#define TURING_CC_NSPAUX_HW_CTRL_IRQ_STATUS_GDSR     0x230
#define TURING_CC_NSPAUX_HW_CTRL_IRQ_MASK_GDSR       0x234
#define TURING_CC_NSPAUX_HW_CTRL_IRQ_CLEAR_GDSR      0x238
#define TURING_CC_NSPAUX_HW_CTRL_REQ_SPARE_GDSR      0x23C
#define TURING_CC_CENG_AHBS_CBCR                     0x240
#define TURING_CC_NSPNOC_AHBS_CBCR                   0x244
#define TURING_CC_DMA_AHBS_CBCR                      0x248
#define TURING_CC_UBWCD_AHBS_CBCR                    0x24C
#define TURING_CC_NSPAUX_XO_CBCR                     0x250
#define TURING_CC_NSPNOC_CBCR                        0x254
#define TURING_CC_NSPNOC_SREGR                       0x258
#define TURING_CC_NSPNOC_CFG_SREGR                   0x25C
#define TURING_CC_NSPNOC_CFG2_SREGR                  0x260
#define TURING_CC_NSPNOC_ATB_CBCR                    0x264
#define TURING_CC_NSPNOC_APB_CBCR                    0x268
#define TURING_CC_CENG_CMD_RCGR                      0x26C
#define TURING_CC_CENG_CFG_RCGR                      0x270
#define TURING_CC_CENG_AACSR                         0x280
#define TURING_CC_CENG_DCD_DIV_DCDR                  0x284
#define TURING_CC_CENG_NSP_CBCR                      0x288
#define TURING_CC_CENG_NSP_AO_CBCR                   0x28C
#define TURING_CC_CENG_DIV_CDIVR                     0x290
#define TURING_CC_CENG_PROC_CBCR                     0x294
#define TURING_CC_CENG_PROC_SREGR                    0x298
#define TURING_CC_CENG_PROC_CFG_SREGR                0x29C
#define TURING_CC_CENG_PROC_CFG2_SREGR               0x2A0
#define TURING_CC_UBWCD_CMD_RCGR                     0x2A4
#define TURING_CC_UBWCD_CFG_RCGR                     0x2A8
#define TURING_CC_UBWCD_AACSR                        0x2B8
#define TURING_CC_UBWCD_DCD_DIV_DCDR                 0x2BC
#define TURING_CC_UBWCD_CBCR                         0x2C0
#define TURING_CC_UBWCD_SREGR                        0x2C4
#define TURING_CC_UBWCD_CFG_SREGR                    0x2C8
#define TURING_CC_UBWCD_CFG2_SREGR                   0x2CC
#define TURING_CC_UBWCD_IPNOC_CBCR                   0x2D0
#define TURING_CC_UBWCD_IPNOC_SREGR                  0x2D4
#define TURING_CC_UBWCD_IPNOC_CFG_SREGR              0x2D8
#define TURING_CC_UBWCD_IPNOC_CFG2_SREGR             0x2DC
#define TURING_CC_UBWCD_MSF_CBCR                     0x2E0
#define TURING_CC_UBWCD_NSPNOC_CBCR                  0x2E4
#define TURING_CC_DMA_CMD_RCGR                       0x2E8
#define TURING_CC_DMA_CFG_RCGR                       0x2EC
#define TURING_CC_DMA_AACSR                          0x2FC
#define TURING_CC_DMA_DCD_DIV_DCDR                   0x300
#define TURING_CC_DMA_CBCR                           0x304
#define TURING_CC_DMA_SREGR                          0x308
#define TURING_CC_DMA_CFG_SREGR                      0x30C
#define TURING_CC_DMA_CFG2_SREGR                     0x310
#define TURING_CC_DMA_MSF_UBWCDIP1_CBCR              0x314
#define TURING_CC_DMA_MSF_TCMS_CBCR                  0x318
#define TURING_CC_TCMS_CMD_RCGR                      0x31C
#define TURING_CC_TCMS_CFG_RCGR                      0x320
#define TURING_CC_TCMS_AACSR                         0x330
#define TURING_CC_TCMS_DCD_DIV_DCDR                  0x334
#define TURING_CC_TCMS_CBCR                          0x338
#define TURING_CC_TCMS_SREGR                         0x33C
#define TURING_CC_TCMS_CFG_SREGR                     0x340
#define TURING_CC_TCMS_CFG2_SREGR                    0x344
#define TURING_CC_Q6SS_BCR                           0x400
#define TURING_CC_Q6SS_AHBM_AON_CBCR                 0x404
#define TURING_CC_Q6SS_AHBS_AON_CBCR                 0x408
#define TURING_CC_Q6SS_AHBS_AON_MXC_CBCR             0x40C
#define TURING_CC_Q6SS_ALT_RESET_AON_CBCR            0x410
#define TURING_CC_Q6SS_LLM_CURR_SSC_CBCR             0x414
#define TURING_CC_Q6SS_LLM2_CURR_SSC_CBCR            0x418
#define TURING_CC_Q6SS_LLM_MXCDPM_CURR_SSC_CBCR     0x41C
#define TURING_CC_Q6SS_LLM2_MXCDPM_CURR_SSC_CBCR    0x420
#define TURING_CC_Q6SS_Q6_AXIM_CBCR                  0x424
#define TURING_CC_Q6SS_AXIS2_CBCR                    0x428
#define TURING_CC_Q6SS_LMH_CMD_RCGR                  0x42C
#define TURING_CC_Q6SS_LMH_CFG_RCGR                  0x430
#define TURING_CC_Q6SS_LMH_CBCR                      0x434
#define TURING_CC_Q6SS_LLM_TEMP_SSC_CBCR             0x438

#define TURING_CC_NUM_REGS (TURING_CC_MMIO_SIZE / 4)

typedef struct TuringCcState {
    SysBusDevice parent_obj;
    MemoryRegion mmio;
    uint32_t regs[TURING_CC_NUM_REGS];
} TuringCcState;

#endif /* HW_MISC_TURING_CC_REGS_H */
