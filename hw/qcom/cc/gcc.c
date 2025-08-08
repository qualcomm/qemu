/*
 * Qualcomm GCC.
 *
 * Copyright (c) 2025 Qualcomm Technologies, Inc. All Rights Reserved.
 *
 * Author: Romain Malmain <rmalmain@qti.qualcomm.com>
 */

#include "qemu/osdep.h"
#include "hw/sysbus-of.h"

#include "hw/qcom/cc/cc.h"
#include "hw/qcom/cc/clk-alpha-pll.h"

// ==== adapted from the linux kernel ====

/* GCC clocks */
#define GCC_AGGRE_NOC_PCIE_AXI_CLK				0
#define GCC_AGGRE_UFS_PHY_AXI_CLK				1
#define GCC_AGGRE_UFS_PHY_AXI_HW_CTL_CLK			2
#define GCC_AGGRE_USB3_PRIM_AXI_CLK				3
#define GCC_BOOT_ROM_AHB_CLK					4
#define GCC_CAM_BIST_MCLK_AHB_CLK				5
#define GCC_CAMERA_AHB_CLK					6
#define GCC_CAMERA_HF_AXI_CLK					7
#define GCC_CAMERA_SF_AXI_CLK					8
#define GCC_CAMERA_XO_CLK					9
#define GCC_CFG_NOC_PCIE_ANOC_AHB_CLK				10
#define GCC_CFG_NOC_USB3_PRIM_AXI_CLK				11
#define GCC_CNOC_PCIE_SF_AXI_CLK				12
#define GCC_DDRSS_PCIE_SF_QTB_CLK				13
#define GCC_DISP_AHB_CLK					14
#define GCC_DISP_HF_AXI_CLK					15
#define GCC_DISP_SF_AXI_CLK					16
#define GCC_EVA_AHB_CLK						17
#define GCC_EVA_AXI0_CLK					18
#define GCC_EVA_AXI0C_CLK					19
#define GCC_EVA_XO_CLK						20
#define GCC_GP1_CLK						21
#define GCC_GP1_CLK_SRC						22
#define GCC_GP2_CLK						23
#define GCC_GP2_CLK_SRC						24
#define GCC_GP3_CLK						25
#define GCC_GP3_CLK_SRC						26
#define GCC_GPLL0						27
#define GCC_GPLL0_OUT_EVEN					28
#define GCC_GPLL1						29
#define GCC_GPLL4						30
#define GCC_GPLL7						31
#define GCC_GPLL9						32
#define GCC_GPU_CFG_AHB_CLK					33
#define GCC_GPU_GEMNOC_GFX_CLK					34
#define GCC_GPU_GPLL0_CLK_SRC					35
#define GCC_GPU_GPLL0_DIV_CLK_SRC				36
#define GCC_PCIE_0_AUX_CLK					37
#define GCC_PCIE_0_AUX_CLK_SRC					38
#define GCC_PCIE_0_CFG_AHB_CLK					39
#define GCC_PCIE_0_MSTR_AXI_CLK					40
#define GCC_PCIE_0_PHY_AUX_CLK					41
#define GCC_PCIE_0_PHY_AUX_CLK_SRC				42
#define GCC_PCIE_0_PHY_RCHNG_CLK				43
#define GCC_PCIE_0_PHY_RCHNG_CLK_SRC				44
#define GCC_PCIE_0_PIPE_CLK					45
#define GCC_PCIE_0_PIPE_CLK_SRC					46
#define GCC_PCIE_0_SLV_AXI_CLK					47
#define GCC_PCIE_0_SLV_Q2A_AXI_CLK				48
#define GCC_PCIE_RSCC_CFG_AHB_CLK				49
#define GCC_PCIE_RSCC_XO_CLK					50
#define GCC_PDM2_CLK						51
#define GCC_PDM2_CLK_SRC					52
#define GCC_PDM_AHB_CLK						53
#define GCC_PDM_XO4_CLK						54
#define GCC_QMIP_CAMERA_CMD_AHB_CLK				55
#define GCC_QMIP_CAMERA_NRT_AHB_CLK				56
#define GCC_QMIP_CAMERA_RT_AHB_CLK				57
#define GCC_QMIP_DISP_DCP_SF_AHB_CLK				58
#define GCC_QMIP_GPU_AHB_CLK					59
#define GCC_QMIP_PCIE_AHB_CLK					60
#define GCC_QMIP_VIDEO_CV_CPU_AHB_CLK				61
#define GCC_QMIP_VIDEO_CVP_AHB_CLK				62
#define GCC_QMIP_VIDEO_V_CPU_AHB_CLK				63
#define GCC_QMIP_VIDEO_VCODEC_AHB_CLK				64
#define GCC_QUPV3_I2C_CORE_CLK					65
#define GCC_QUPV3_I2C_S0_CLK					66
#define GCC_QUPV3_I2C_S0_CLK_SRC				67
#define GCC_QUPV3_I2C_S1_CLK					68
#define GCC_QUPV3_I2C_S1_CLK_SRC				69
#define GCC_QUPV3_I2C_S2_CLK					70
#define GCC_QUPV3_I2C_S2_CLK_SRC				71
#define GCC_QUPV3_I2C_S3_CLK					72
#define GCC_QUPV3_I2C_S3_CLK_SRC				73
#define GCC_QUPV3_I2C_S4_CLK					74
#define GCC_QUPV3_I2C_S4_CLK_SRC				75
#define GCC_QUPV3_I2C_S_AHB_CLK					76
#define GCC_QUPV3_WRAP1_CORE_2X_CLK				77
#define GCC_QUPV3_WRAP1_CORE_CLK				78
#define GCC_QUPV3_WRAP1_QSPI_REF_CLK				79
#define GCC_QUPV3_WRAP1_QSPI_REF_CLK_SRC			80
#define GCC_QUPV3_WRAP1_S0_CLK					81
#define GCC_QUPV3_WRAP1_S0_CLK_SRC				82
#define GCC_QUPV3_WRAP1_S1_CLK					83
#define GCC_QUPV3_WRAP1_S1_CLK_SRC				84
#define GCC_QUPV3_WRAP1_S2_CLK					85
#define GCC_QUPV3_WRAP1_S2_CLK_SRC				86
#define GCC_QUPV3_WRAP1_S3_CLK					87
#define GCC_QUPV3_WRAP1_S3_CLK_SRC				88
#define GCC_QUPV3_WRAP1_S4_CLK					89
#define GCC_QUPV3_WRAP1_S4_CLK_SRC				90
#define GCC_QUPV3_WRAP1_S5_CLK					91
#define GCC_QUPV3_WRAP1_S5_CLK_SRC				92
#define GCC_QUPV3_WRAP1_S6_CLK					93
#define GCC_QUPV3_WRAP1_S6_CLK_SRC				94
#define GCC_QUPV3_WRAP1_S7_CLK					95
#define GCC_QUPV3_WRAP1_S7_CLK_SRC				96
#define GCC_QUPV3_WRAP2_CORE_2X_CLK				97
#define GCC_QUPV3_WRAP2_CORE_CLK				98
#define GCC_QUPV3_WRAP2_S0_CLK					99
#define GCC_QUPV3_WRAP2_S0_CLK_SRC				100
#define GCC_QUPV3_WRAP2_S1_CLK					101
#define GCC_QUPV3_WRAP2_S1_CLK_SRC				102
#define GCC_QUPV3_WRAP2_S2_CLK					103
#define GCC_QUPV3_WRAP2_S2_CLK_SRC				104
#define GCC_QUPV3_WRAP2_S3_CLK					105
#define GCC_QUPV3_WRAP2_S3_CLK_SRC				106
#define GCC_QUPV3_WRAP2_S4_CLK					107
#define GCC_QUPV3_WRAP2_S4_CLK_SRC				108
#define GCC_QUPV3_WRAP3_CORE_2X_CLK				109
#define GCC_QUPV3_WRAP3_CORE_CLK				110
#define GCC_QUPV3_WRAP3_IBI_CTRL_0_CLK_SRC			111
#define GCC_QUPV3_WRAP3_IBI_CTRL_1_CLK				112
#define GCC_QUPV3_WRAP3_IBI_CTRL_2_CLK				113
#define GCC_QUPV3_WRAP3_S0_CLK					114
#define GCC_QUPV3_WRAP3_S0_CLK_SRC				115
#define GCC_QUPV3_WRAP3_S1_CLK					116
#define GCC_QUPV3_WRAP3_S1_CLK_SRC				117
#define GCC_QUPV3_WRAP3_S2_CLK					118
#define GCC_QUPV3_WRAP3_S2_CLK_SRC				119
#define GCC_QUPV3_WRAP3_S3_CLK					120
#define GCC_QUPV3_WRAP3_S3_CLK_SRC				121
#define GCC_QUPV3_WRAP3_S4_CLK					122
#define GCC_QUPV3_WRAP3_S4_CLK_SRC				123
#define GCC_QUPV3_WRAP3_S5_CLK					124
#define GCC_QUPV3_WRAP3_S5_CLK_SRC				125
#define GCC_QUPV3_WRAP4_CORE_2X_CLK				126
#define GCC_QUPV3_WRAP4_CORE_CLK				127
#define GCC_QUPV3_WRAP4_S0_CLK					128
#define GCC_QUPV3_WRAP4_S0_CLK_SRC				129
#define GCC_QUPV3_WRAP4_S1_CLK					130
#define GCC_QUPV3_WRAP4_S1_CLK_SRC				131
#define GCC_QUPV3_WRAP4_S2_CLK					132
#define GCC_QUPV3_WRAP4_S2_CLK_SRC				133
#define GCC_QUPV3_WRAP4_S3_CLK					134
#define GCC_QUPV3_WRAP4_S3_CLK_SRC				135
#define GCC_QUPV3_WRAP4_S4_CLK					136
#define GCC_QUPV3_WRAP4_S4_CLK_SRC				137
#define GCC_QUPV3_WRAP_1_M_AXI_CLK				138
#define GCC_QUPV3_WRAP_1_S_AHB_CLK				139
#define GCC_QUPV3_WRAP_2_M_AHB_CLK				140
#define GCC_QUPV3_WRAP_2_S_AHB_CLK				141
#define GCC_QUPV3_WRAP_3_IBI_1_AHB_CLK				142
#define GCC_QUPV3_WRAP_3_IBI_2_AHB_CLK				143
#define GCC_QUPV3_WRAP_3_M_AHB_CLK				144
#define GCC_QUPV3_WRAP_3_S_AHB_CLK				145
#define GCC_QUPV3_WRAP_4_M_AHB_CLK				146
#define GCC_QUPV3_WRAP_4_S_AHB_CLK				147
#define GCC_SDCC2_AHB_CLK					148
#define GCC_SDCC2_APPS_CLK					149
#define GCC_SDCC2_APPS_CLK_SRC					150
#define GCC_SDCC4_AHB_CLK					151
#define GCC_SDCC4_APPS_CLK					152
#define GCC_SDCC4_APPS_CLK_SRC					153
#define GCC_UFS_PHY_AHB_CLK					154
#define GCC_UFS_PHY_AXI_CLK					155
#define GCC_UFS_PHY_AXI_CLK_SRC					156
#define GCC_UFS_PHY_AXI_HW_CTL_CLK				157
#define GCC_UFS_PHY_ICE_CORE_CLK				158
#define GCC_UFS_PHY_ICE_CORE_CLK_SRC				159
#define GCC_UFS_PHY_ICE_CORE_HW_CTL_CLK				160
#define GCC_UFS_PHY_PHY_AUX_CLK					161
#define GCC_UFS_PHY_PHY_AUX_CLK_SRC				162
#define GCC_UFS_PHY_PHY_AUX_HW_CTL_CLK				163
#define GCC_UFS_PHY_RX_SYMBOL_0_CLK				164
#define GCC_UFS_PHY_RX_SYMBOL_0_CLK_SRC				165
#define GCC_UFS_PHY_RX_SYMBOL_1_CLK				166
#define GCC_UFS_PHY_RX_SYMBOL_1_CLK_SRC				167
#define GCC_UFS_PHY_TX_SYMBOL_0_CLK				168
#define GCC_UFS_PHY_TX_SYMBOL_0_CLK_SRC				169
#define GCC_UFS_PHY_UNIPRO_CORE_CLK				170
#define GCC_UFS_PHY_UNIPRO_CORE_CLK_SRC				171
#define GCC_UFS_PHY_UNIPRO_CORE_HW_CTL_CLK			172
#define GCC_USB30_PRIM_MASTER_CLK				173
#define GCC_USB30_PRIM_MASTER_CLK_SRC				174
#define GCC_USB30_PRIM_MOCK_UTMI_CLK				175
#define GCC_USB30_PRIM_MOCK_UTMI_CLK_SRC			176
#define GCC_USB30_PRIM_MOCK_UTMI_POSTDIV_CLK_SRC		177
#define GCC_USB30_PRIM_SLEEP_CLK				178
#define GCC_USB3_PRIM_PHY_AUX_CLK				179
#define GCC_USB3_PRIM_PHY_AUX_CLK_SRC				180
#define GCC_USB3_PRIM_PHY_COM_AUX_CLK				181
#define GCC_USB3_PRIM_PHY_PIPE_CLK				182
#define GCC_USB3_PRIM_PHY_PIPE_CLK_SRC				183
#define GCC_VIDEO_AHB_CLK					184
#define GCC_VIDEO_AXI0_CLK					185
#define GCC_VIDEO_AXI1_CLK					186
#define GCC_VIDEO_XO_CLK					187

/* GCC power domains */
#define GCC_PCIE_0_GDSC						0
#define GCC_PCIE_0_PHY_GDSC					1
#define GCC_UFS_MEM_PHY_GDSC					2
#define GCC_UFS_PHY_GDSC					3
#define GCC_USB30_PRIM_GDSC					4
#define GCC_USB3_PHY_GDSC					5

/* GCC resets */
#define GCC_CAMERA_BCR						0
#define GCC_DISPLAY_BCR						1
#define GCC_EVA_AXI0_CLK_ARES					2
#define GCC_EVA_AXI0C_CLK_ARES					3
#define GCC_EVA_BCR						4
#define GCC_GPU_BCR						5
#define GCC_PCIE_0_BCR						6
#define GCC_PCIE_0_LINK_DOWN_BCR				7
#define GCC_PCIE_0_NOCSR_COM_PHY_BCR				8
#define GCC_PCIE_0_PHY_BCR					9
#define GCC_PCIE_0_PHY_NOCSR_COM_PHY_BCR			10
#define GCC_PCIE_PHY_BCR					11
#define GCC_PCIE_PHY_CFG_AHB_BCR				12
#define GCC_PCIE_PHY_COM_BCR					13
#define GCC_PCIE_RSCC_BCR					14
#define GCC_PDM_BCR						15
#define GCC_QUPV3_WRAPPER_1_BCR					16
#define GCC_QUPV3_WRAPPER_2_BCR					17
#define GCC_QUPV3_WRAPPER_3_BCR					18
#define GCC_QUPV3_WRAPPER_4_BCR					19
#define GCC_QUPV3_WRAPPER_I2C_BCR				20
#define GCC_QUSB2PHY_PRIM_BCR					21
#define GCC_QUSB2PHY_SEC_BCR					22
#define GCC_SDCC2_BCR						23
#define GCC_SDCC4_BCR						24
#define GCC_UFS_PHY_BCR						25
#define GCC_USB30_PRIM_BCR					26
#define GCC_USB3_DP_PHY_PRIM_BCR				27
#define GCC_USB3_DP_PHY_SEC_BCR					28
#define GCC_USB3_PHY_PRIM_BCR					29
#define GCC_USB3_PHY_SEC_BCR					30
#define GCC_USB3PHY_PHY_PRIM_BCR				31
#define GCC_USB3PHY_PHY_SEC_BCR					32
#define GCC_VIDEO_AXI0_CLK_ARES					33
#define GCC_VIDEO_AXI1_CLK_ARES					34
#define GCC_VIDEO_BCR						35
#define GCC_VIDEO_XO_CLK_ARES					36

static struct gdsc gcc_pcie_0_gdsc = {
	.gdscr = 0x6b004,
	.en_rest_wait_val = 0x2,
	.en_few_wait_val = 0x2,
	.clk_dis_wait_val = 0xf,
	.collapse_ctrl = 0x5214c,
	.collapse_mask = BIT(0),
	// .pd = {
	// 	.name = "gcc_pcie_0_gdsc",
	// },
	.pwrsts = PWRSTS_OFF_ON,
	.flags = POLL_CFG_GDSCR | RETAIN_FF_ENABLE | VOTABLE,
	.supply = "vdd_cx",
};

static struct gdsc gcc_pcie_0_phy_gdsc = {
	.gdscr = 0x6c000,
	.en_rest_wait_val = 0x2,
	.en_few_wait_val = 0x2,
	.clk_dis_wait_val = 0x2,
	.collapse_ctrl = 0x5214c,
	.collapse_mask = BIT(2),
	// .pd = {
	// 	.name = "gcc_pcie_0_phy_gdsc",
	// },
	.pwrsts = PWRSTS_OFF_ON,
	.flags = POLL_CFG_GDSCR | RETAIN_FF_ENABLE | VOTABLE,
	.supply = "vdd_mx",
};

static struct gdsc gcc_ufs_mem_phy_gdsc = {
	.gdscr = 0x9e000,
	.en_rest_wait_val = 0x2,
	.en_few_wait_val = 0x2,
	.clk_dis_wait_val = 0x2,
	// .pd = {
	// 	.name = "gcc_ufs_mem_phy_gdsc",
	// },
	.pwrsts = PWRSTS_OFF_ON,
	.flags = POLL_CFG_GDSCR | RETAIN_FF_ENABLE,
	.supply = "vdd_mx",
};

static struct gdsc gcc_ufs_phy_gdsc = {
	.gdscr = 0x77004,
	.en_rest_wait_val = 0x2,
	.en_few_wait_val = 0x2,
	.clk_dis_wait_val = 0xf,
	// .pd = {
	// 	.name = "gcc_ufs_phy_gdsc",
	// },
	.pwrsts = PWRSTS_OFF_ON,
	.flags = POLL_CFG_GDSCR | RETAIN_FF_ENABLE,
	.supply = "vdd_cx",
};

static struct gdsc *gcc_canoe_gdscs[] = {
	[GCC_PCIE_0_GDSC] = &gcc_pcie_0_gdsc,
	[GCC_PCIE_0_PHY_GDSC] = &gcc_pcie_0_phy_gdsc,
	[GCC_UFS_PHY_GDSC] = &gcc_ufs_phy_gdsc,
	[GCC_UFS_MEM_PHY_GDSC] = &gcc_ufs_mem_phy_gdsc,
};

static struct clk_alpha_pll gcc_gpll0 = {
	.offset = 0x0,
	.regs = clk_alpha_pll_regs[CLK_ALPHA_PLL_TYPE_TAYCAN_EKO_T],
	// .clkr = {
	// 	.enable_reg = 0x52020,
	// 	.enable_mask = BIT(0),
	// 	.hw.init = &(const struct clk_init_data) {
	// 		.name = "gcc_gpll0",
	// 		.parent_data = &(const struct clk_parent_data) {
	// 			.fw_name = "bi_tcxo",
	// 		},
	// 		.num_parents = 1,
	// 		.ops = &clk_alpha_pll_fixed_taycan_eko_t_ops,
	// 	},
	// 	.vdd_data = {
	// 		.vdd_class = &vdd_cx,
	// 		.num_rate_max = VDD_NUM,
	// 		.rate_max = (unsigned long[VDD_NUM]) {
	// 			[VDD_LOWER_D2] = 621000000,
	// 			[VDD_LOW] = 1066000000,
	// 			[VDD_LOW_L1] = 1600000000,
	// 			[VDD_NOMINAL] = 2000000000,
	// 			[VDD_HIGH] = 2500000000},
	// 	},
	// },
};

static struct clk_alpha_pll gcc_gpll1 = {
	.offset = 0x1000,
	.regs = clk_alpha_pll_regs[CLK_ALPHA_PLL_TYPE_TAYCAN_EKO_T],
	// .clkr = {
	// 	.enable_reg = 0x52020,
	// 	.enable_mask = BIT(1),
	// 	.hw.init = &(const struct clk_init_data) {
	// 		.name = "gcc_gpll1",
	// 		.parent_data = &(const struct clk_parent_data) {
	// 			.fw_name = "bi_tcxo",
	// 		},
	// 		.num_parents = 1,
	// 		.ops = &clk_alpha_pll_fixed_taycan_eko_t_ops,
	// 	},
	// 	.vdd_data = {
	// 		.vdd_class = &vdd_cx,
	// 		.num_rate_max = VDD_NUM,
	// 		.rate_max = (unsigned long[VDD_NUM]) {
	// 			[VDD_LOWER_D2] = 621000000,
	// 			[VDD_LOW] = 1066000000,
	// 			[VDD_LOW_L1] = 1600000000,
	// 			[VDD_NOMINAL] = 2000000000,
	// 			[VDD_HIGH] = 2500000000},
	// 	},
	// },
};

static struct clk_alpha_pll gcc_gpll4 = {
	.offset = 0x4000,
	.regs = clk_alpha_pll_regs[CLK_ALPHA_PLL_TYPE_TAYCAN_EKO_T],
	// .clkr = {
	// 	.enable_reg = 0x52020,
	// 	.enable_mask = BIT(4),
	// 	.hw.init = &(const struct clk_init_data) {
	// 		.name = "gcc_gpll4",
	// 		.parent_data = &(const struct clk_parent_data) {
	// 			.fw_name = "bi_tcxo",
	// 		},
	// 		.num_parents = 1,
	// 		.ops = &clk_alpha_pll_fixed_taycan_eko_t_ops,
	// 	},
	// 	.vdd_data = {
	// 		.vdd_class = &vdd_cx,
	// 		.num_rate_max = VDD_NUM,
	// 		.rate_max = (unsigned long[VDD_NUM]) {
	// 			[VDD_LOWER_D2] = 621000000,
	// 			[VDD_LOW] = 1066000000,
	// 			[VDD_LOW_L1] = 1600000000,
	// 			[VDD_NOMINAL] = 2000000000,
	// 			[VDD_HIGH] = 2500000000},
	// 	},
	// },
};

static struct clk_alpha_pll gcc_gpll7 = {
	.offset = 0x7000,
	.regs = clk_alpha_pll_regs[CLK_ALPHA_PLL_TYPE_TAYCAN_EKO_T],
	// .clkr = {
	// 	.enable_reg = 0x52020,
	// 	.enable_mask = BIT(7),
	// 	.hw.init = &(const struct clk_init_data) {
	// 		.name = "gcc_gpll7",
	// 		.parent_data = &(const struct clk_parent_data) {
	// 			.fw_name = "bi_tcxo",
	// 		},
	// 		.num_parents = 1,
	// 		.ops = &clk_alpha_pll_fixed_taycan_eko_t_ops,
	// 	},
	// 	.vdd_data = {
	// 		.vdd_class = &vdd_cx,
	// 		.num_rate_max = VDD_NUM,
	// 		.rate_max = (unsigned long[VDD_NUM]) {
	// 			[VDD_LOWER_D2] = 621000000,
	// 			[VDD_LOW] = 1066000000,
	// 			[VDD_LOW_L1] = 1600000000,
	// 			[VDD_NOMINAL] = 2000000000,
	// 			[VDD_HIGH] = 2500000000},
	// 	},
	// },
};

static struct clk_alpha_pll gcc_gpll9 = {
	.offset = 0x9000,
	.regs = clk_alpha_pll_regs[CLK_ALPHA_PLL_TYPE_TAYCAN_EKO_T],
	// .clkr = {
	// 	.enable_reg = 0x52020,
	// 	.enable_mask = BIT(9),
	// 	.hw.init = &(const struct clk_init_data) {
	// 		.name = "gcc_gpll9",
	// 		.parent_data = &(const struct clk_parent_data) {
	// 			.fw_name = "bi_tcxo",
	// 		},
	// 		.num_parents = 1,
	// 		.ops = &clk_alpha_pll_fixed_taycan_eko_t_ops,
	// 	},
	// 	.vdd_data = {
	// 		.vdd_class = &vdd_cx,
	// 		.num_rate_max = VDD_NUM,
	// 		.rate_max = (unsigned long[VDD_NUM]) {
	// 			[VDD_LOWER_D2] = 621000000,
	// 			[VDD_LOW] = 1066000000,
	// 			[VDD_LOW_L1] = 1600000000,
	// 			[VDD_NOMINAL] = 2000000000,
	// 			[VDD_HIGH] = 2500000000},
	// 	},
	// },
};

enum pll_kind {
    GCC_PLL_0,
    GCC_PLL_1,
    GCC_PLL_4,
    GCC_PLL_7,
    GCC_PLL_9,
};

static struct clk_alpha_pll *gcc_canoe_plls[] = {
	[GCC_PLL_0] = &gcc_gpll0,
	[GCC_PLL_1] = &gcc_gpll1,
	[GCC_PLL_4] = &gcc_gpll4,
	[GCC_PLL_7] = &gcc_gpll7,
	[GCC_PLL_9] = &gcc_gpll9,
};

const struct qcom_cc_desc gcc_canoe_desc = {
	// .config = &gcc_canoe_regmap_config,
	// .clks = gcc_canoe_clocks,
	// .num_clks = ARRAY_SIZE(gcc_canoe_clocks),
	// .resets = gcc_canoe_resets,
	// .num_resets = ARRAY_SIZE(gcc_canoe_resets),
	// .clk_regulators = gcc_canoe_regulators,
	// .num_clk_regulators = ARRAY_SIZE(gcc_canoe_regulators),
	.gdscs = gcc_canoe_gdscs,
	.num_gdscs = ARRAY_SIZE(gcc_canoe_gdscs),

    .alpha_plls = gcc_canoe_plls,
    .num_alpha_plls = ARRAY_SIZE(gcc_canoe_plls),

    /* added field */
    .reset_regs = {
        [CC_REG_PLL_MODE] = 0x00000000
                                | PLL_ACTIVE_FLAG
                                | PLL_LOCK_DET,
        [CC_REG_PLL_L_VAL] = 0x00480000,
        [CC_REG_PLL_ALPHA_VAL] = 0x00000000,
        [CC_REG_PLL_USER_CTL] = 0x00000009,
        [CC_REG_CXO_CBCR] = 0x88000001,
        [CC_REG_GDSCR] = 0x0222F801
                                | PWR_ON_MASK,
        [CC_REG_GDSCR_CFG] = 0x04088000
                                | GDSC_POWER_UP_COMPLETE
                                | GDSC_POWER_DOWN_COMPLETE,
        [CC_REG_HW_CTRL] = 0x2A00010A
                                | PWR_ON_MASK,
    },
};

// ==== adapated code ends here ====
