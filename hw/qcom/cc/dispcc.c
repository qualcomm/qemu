#include "qemu/osdep.h"
#include "hw/sysbus-of.h"

#include "hw/qcom/cc/cc.h"

// ==== adapted from the linux kernel ====

static struct clk_alpha_pll disp_cc_pll0 = {
	.offset = 0x0,
	// .vco_table = taycan_eko_t_vco,
	// .num_vco = ARRAY_SIZE(taycan_eko_t_vco),
	// .regs = clk_alpha_pll_regs[CLK_ALPHA_PLL_TYPE_TAYCAN_EKO_T],
	// .clkr = {
	// 	.hw.init = &(const struct clk_init_data) {
	// 		.name = "disp_cc_pll0",
	// 		.parent_data = &(const struct clk_parent_data) {
	// 			.fw_name = "bi_tcxo",
	// 			.name = "bi_tcxo",
	// 		},
	// 		.num_parents = 1,
	// 		.flags = CLK_GET_RATE_NOCACHE,
	// 		.ops = &clk_alpha_pll_crm_taycan_eko_t_ops,
	// 	},
	// 	.vdd_data = {
	// 		.vdd_class = &vdd_mm,
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

static struct clk_alpha_pll disp_cc_pll1 = {
	.offset = 0x1000,
	// .vco_table = taycan_eko_t_vco,
	// .num_vco = ARRAY_SIZE(taycan_eko_t_vco),
	// .regs = clk_alpha_pll_regs[CLK_ALPHA_PLL_TYPE_TAYCAN_EKO_T],
	// .clkr = {
	// 	.hw.init = &(const struct clk_init_data) {
	// 		.name = "disp_cc_pll1",
	// 		.parent_data = &(const struct clk_parent_data) {
	// 			.fw_name = "bi_tcxo",
	// 			.name = "bi_tcxo",
	// 		},
	// 		.num_parents = 1,
	// 		.ops = &clk_alpha_pll_taycan_eko_t_ops,
	// 	},
	// 	.vdd_data = {
	// 		.vdd_class = &vdd_mm,
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

static struct clk_alpha_pll disp_cc_pll2 = {
	.offset = 0x2000,
	// .vco_table = pongo_eko_t_vco,
	// .num_vco = ARRAY_SIZE(pongo_eko_t_vco),
	// .regs = clk_alpha_pll_regs[CLK_ALPHA_PLL_TYPE_PONGO_EKO_T],
	// .clkr = {
	// 	.hw.init = &(const struct clk_init_data) {
	// 		.name = "disp_cc_pll2",
	// 		.parent_data = &(const struct clk_parent_data) {
	// 			.fw_name = "sleep_clk",
	// 			.name = "sleep_clk",
	// 		},
	// 		.num_parents = 1,
	// 		.ops = &clk_alpha_pll_pongo_eko_t_ops,
	// 	},
	// 	.vdd_data = {
	// 		.vdd_class = &vdd_mx,
	// 		.num_rate_max = VDD_NUM,
	// 		.rate_max = (unsigned long[VDD_NUM]) {
	// 			[VDD_LOWER_D1] = 153600000},
	// 	},
	// },
};

enum pll_kind {
    DISP_CC_PLL_0,
    DISP_CC_PLL_1,
    DISP_CC_PLL_2,
};

static struct clk_alpha_pll *disp_cc_canoe_plls[] = {
	[DISP_CC_PLL_0] = &disp_cc_pll0,
	[DISP_CC_PLL_1] = &disp_cc_pll1,
	[DISP_CC_PLL_2] = &disp_cc_pll2,
};

struct qcom_cc_desc disp_cc_canoe_desc = {
	// .config = &disp_cc_canoe_regmap_config,
	// .clks = disp_cc_canoe_clocks,
	// .num_clks = ARRAY_SIZE(disp_cc_canoe_clocks),
	// .resets = disp_cc_canoe_resets,
	// .num_resets = ARRAY_SIZE(disp_cc_canoe_resets),
	// .clk_regulators = disp_cc_canoe_regulators,
	// .num_clk_regulators = ARRAY_SIZE(disp_cc_canoe_regulators),
	// .gdscs = disp_cc_canoe_gdscs,
	// .num_gdscs = ARRAY_SIZE(disp_cc_canoe_gdscs),

    /* added field */
    .plls = disp_cc_canoe_plls,
    .num_plls = ARRAY_SIZE(disp_cc_canoe_plls),

    .reset_regs = {
        [CC_REG_PLL_MODE] = 0x00000000
                                | PLL_ACTIVE_FLAG
                                | PLL_LOCK_DET,
    },
};

// ==== adapated code ends here ====
