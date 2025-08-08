/* 
 * Qualcomm GPUCC
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

/* GPU_CC clocks */
#define GPU_CC_AHB_CLK						0
#define GPU_CC_CB_CLK						1
#define GPU_CC_CX_ACCU_SHIFT_CLK				2
#define GPU_CC_CX_GMU_CLK					3
#define GPU_CC_CXO_AON_CLK					4
#define GPU_CC_CXO_CLK						5
#define GPU_CC_DEMET_CLK					6
#define GPU_CC_DPM_CLK						7
#define GPU_CC_FF_CLK_SRC					8
#define GPU_CC_FREQ_MEASURE_CLK					9
#define GPU_CC_GMU_CLK_SRC					10
#define GPU_CC_GPU_SMMU_VOTE_CLK				11
#define GPU_CC_GX_ACCU_SHIFT_CLK				12
#define GPU_CC_GX_GMU_CLK					13
#define GPU_CC_HUB_AON_CLK					14
#define GPU_CC_HUB_CLK_SRC					15
#define GPU_CC_HUB_CX_INT_CLK					16
#define GPU_CC_HUB_DIV_CLK_SRC					17
#define GPU_CC_MEMNOC_GFX_CLK					18
#define GPU_CC_PLL0						19
#define GPU_CC_PLL0_OUT_EVEN					20
#define GPU_CC_RSCC_HUB_AON_CLK					21
#define GPU_CC_RSCC_XO_AON_CLK					22
#define GPU_CC_SLEEP_CLK					23

/* GPU_CC power domains */
#define GPU_CC_CX_GDSC						0
#define GPU_CC_CX_SMMU_GDSC					1
#define GPU_CC_CX_GMU_GDSC					2

/* GPU_CC resets */
#define GPU_CC_CB_BCR						0
#define GPU_CC_CX_BCR						1
#define GPU_CC_FAST_HUB_BCR					2
#define GPU_CC_FF_BCR						3
#define GPU_CC_GMU_BCR						4
#define GPU_CC_GX_BCR						5
#define GPU_CC_XO_BCR						6

static struct gdsc gpu_cc_cx_gdsc = {
	.gdscr = 0x9080,
	.gds_hw_ctrl = 0x9094,
	.en_rest_wait_val = 0x2,
	.en_few_wait_val = 0x2,
	.clk_dis_wait_val = 0x8,
	// .pd = {
	// 	.name = "gpu_cc_cx_gdsc",
	// },
	.pwrsts = PWRSTS_OFF_ON,
	.flags = RETAIN_FF_ENABLE | VOTABLE,
	.supply = "vdd_cx",
};

static struct gdsc gpu_cc_cx_smmu_gdsc = {
	.gdscr = 0x9080,
	.gds_hw_ctrl = 0x9094,
	.en_rest_wait_val = 0x2,
	.en_few_wait_val = 0x2,
	.clk_dis_wait_val = 0x8,
	// .pd = {
	// 	.name = "gpu_cc_cx_smmu_gdsc",
	// 	.power_on = gdsc_cx_do_nothing,
	// 	.power_off = gdsc_cx_do_nothing,
	// },
	.pwrsts = PWRSTS_OFF_ON,
	.flags = RETAIN_FF_ENABLE | VOTABLE,
	// .parent = &gpu_cc_cx_gdsc.pd,
};

static struct gdsc gpu_cc_cx_gmu_gdsc = {
	.gdscr = 0x9080,
	.gds_hw_ctrl = 0x9094,
	.en_rest_wait_val = 0x2,
	.en_few_wait_val = 0x2,
	.clk_dis_wait_val = 0x8,
	// .pd = {
	// 	.name = "gpu_cc_cx_gmu_gdsc",
	// 	.power_on = gdsc_cx_do_nothing,
	// 	.power_off = gdsc_cx_do_nothing,
	// },
	.pwrsts = PWRSTS_OFF_ON,
	.flags = RETAIN_FF_ENABLE | VOTABLE,
	// .parent = &gpu_cc_cx_gdsc.pd,
};

static struct gdsc *gpu_cc_canoe_gdscs[] = {
	[GPU_CC_CX_GDSC] = &gpu_cc_cx_gdsc,
	[GPU_CC_CX_SMMU_GDSC] = &gpu_cc_cx_smmu_gdsc,
	[GPU_CC_CX_GMU_GDSC] = &gpu_cc_cx_gmu_gdsc,
};

static struct clk_alpha_pll gpu_cc_pll0 = {
	.offset = 0x0,
	// .vco_table = taycan_eko_t_vco,
	// .num_vco = ARRAY_SIZE(taycan_eko_t_vco),
	.regs = clk_alpha_pll_regs[CLK_ALPHA_PLL_TYPE_TAYCAN_EKO_T],
	// .clkr = {
	// 	.hw.init = &(const struct clk_init_data) {
	// 		.name = "gpu_cc_pll0",
	// 		.parent_data = &(const struct clk_parent_data) {
	// 			.fw_name = "bi_tcxo",
	// 		},
	// 		.num_parents = 1,
	// 		.ops = &clk_alpha_pll_taycan_eko_t_ops,
	// 	},
	// 	.vdd_data = {
	// 		.vdd_class = &vdd_mx,
	// 		.num_rate_max = VDD_NUM,
	// 		.rate_max = (unsigned long[VDD_NUM]) {
	// 			[VDD_LOWER_D2] = 1600000000,
	// 			[VDD_LOW] = 1600000000,
	// 			[VDD_LOW_L1] = 1600000000,
	// 			[VDD_NOMINAL] = 2000000000,
	// 			[VDD_HIGH] = 2500000000},
	// 	},
	// },
};

enum pll_kind {
    GPUCC_PLL_0,
};

static struct clk_alpha_pll *gpucc_canoe_plls[] = {
	[GPUCC_PLL_0] = &gpu_cc_pll0,
};

const struct qcom_cc_desc gpu_cc_canoe_desc = {
	// .config = &gpu_cc_canoe_regmap_config,
	// .clks = gpu_cc_canoe_clocks,
	// .num_clks = ARRAY_SIZE(gpu_cc_canoe_clocks),
	// .resets = gpu_cc_canoe_resets,
	// .num_resets = ARRAY_SIZE(gpu_cc_canoe_resets),
	// .clk_regulators = gpu_cc_canoe_regulators,
	// .num_clk_regulators = ARRAY_SIZE(gpu_cc_canoe_regulators),
	.gdscs = gpu_cc_canoe_gdscs,
	.num_gdscs = ARRAY_SIZE(gpu_cc_canoe_gdscs),

    .alpha_plls = gpucc_canoe_plls,
    .num_alpha_plls = ARRAY_SIZE(gpucc_canoe_plls),

    /* added field */
    .reset_regs = {
        [CC_REG_PLL_MODE] = 0x00000000
                                | PLL_ACTIVE_FLAG
                                | PLL_LOCK_DET,
        [CC_REG_PLL_L_VAL] = 0x00480000,
        [CC_REG_PLL_ALPHA_VAL] = 0x00000000,
        [CC_REG_PLL_USER_CTL] = 0x00000009,
        [CC_REG_GMU_CBCR] = 0x88000000,
        [CC_REG_CXO_CBCR] = 0x88000000,
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
