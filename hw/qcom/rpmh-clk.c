#include "qemu/osdep.h"
#include "qemu/bitops.h"

#include "hw/qcom/rpmh-clk.h"

/* RPMh controlled clocks */
#define RPMH_CXO_CLK				0
#define RPMH_CXO_CLK_A				1
#define RPMH_LN_BB_CLK2				2
#define RPMH_LN_BB_CLK2_A			3
#define RPMH_LN_BB_CLK3				4
#define RPMH_LN_BB_CLK3_A			5
#define RPMH_RF_CLK1				6
#define RPMH_RF_CLK1_A				7
#define RPMH_RF_CLK2				8
#define RPMH_RF_CLK2_A				9
#define RPMH_RF_CLK3				10
#define RPMH_RF_CLK3_A				11
#define RPMH_IPA_CLK				12
#define RPMH_LN_BB_CLK1				13
#define RPMH_LN_BB_CLK1_A			14
#define RPMH_CE_CLK				15
#define RPMH_QPIC_CLK				16
#define RPMH_DIV_CLK1				17
#define RPMH_DIV_CLK1_A				18
#define RPMH_RF_CLK4				19
#define RPMH_RF_CLK4_A				20
#define RPMH_RF_CLK5				21
#define RPMH_RF_CLK5_A				22
#define RPMH_PKA_CLK				23
#define RPMH_HWKM_CLK				24
#define RPMH_QLINK_CLK				25
#define RPMH_QLINK_CLK_A			26
#define RPMH_CXO_PAD_CLK			27
#define RPMH_CXO_PAD_CLK_A			28
#define RPMH_SLP_CLK2				29
#define RPMH_SLP_CLK2_A				30

/**
 * rpmh_state: state for the request
 *
 * RPMH_SLEEP_STATE:       State of the resource when the processor subsystem
 *                         is powered down. There is no client using the
 *                         resource actively.
 * RPMH_WAKE_ONLY_STATE:   Resume resource state to the value previously
 *                         requested before the processor was powered down.
 * RPMH_ACTIVE_ONLY_STATE: Active or AMC mode requests. Resource state
 *                         is aggregated immediately.
 */
enum rpmh_state {
	RPMH_SLEEP_STATE,
	RPMH_WAKE_ONLY_STATE,
	RPMH_ACTIVE_ONLY_STATE,
};

#define CLK_RPMH_ARC_EN_OFFSET		0
#define CLK_RPMH_VRM_EN_OFFSET		4

/**
 * struct clk_duty - Structure encoding the duty cycle ratio of a clock
 *
 * @num:	Numerator of the duty cycle ratio
 * @den:	Denominator of the duty cycle ratio
 */
struct clk_duty {
	unsigned int num;
	unsigned int den;
};

struct clk_fixed_factor {
	// struct clk_hw	hw;
	unsigned int	mult;
	unsigned int	div;
	unsigned long	acc;
	unsigned int	flags;
};

// static DEFINE_MUTEX(rpmh_clk_lock);

#define __DEFINE_CLK_RPMH(_name, _clk_name, _res_name,			\
			  _res_en_offset, _res_on, _div, _optional)		\
	static struct clk_rpmh clk_rpmh_##_clk_name##_ao;		\
	static struct clk_rpmh clk_rpmh_##_clk_name = {			\
		.res_name = _res_name,					\
		.res_addr = _res_en_offset,				\
		.res_on_val = _res_on,					\
		.div = _div,						\
		.optional = _optional,					\
		.peer = &clk_rpmh_##_clk_name##_ao,			\
		.valid_state_mask = (BIT(RPMH_WAKE_ONLY_STATE) |	\
				      BIT(RPMH_ACTIVE_ONLY_STATE) |	\
				      BIT(RPMH_SLEEP_STATE)),		\
	};								\
	static struct clk_rpmh clk_rpmh_##_clk_name##_ao= {		\
		.res_name = _res_name,					\
		.res_addr = _res_en_offset,				\
		.res_on_val = _res_on,					\
		.div = _div,						\
		.optional = _optional,					\
		.peer = &clk_rpmh_##_clk_name,				\
		.valid_state_mask = (BIT(RPMH_WAKE_ONLY_STATE) |	\
					BIT(RPMH_ACTIVE_ONLY_STATE)),	\
	}

#define DEFINE_CLK_RPMH_ARC(_name, _res_name, _res_on, _div)		\
	__DEFINE_CLK_RPMH(_name, _name##_##div##_div, _res_name,	\
			  CLK_RPMH_ARC_EN_OFFSET, _res_on, _div, false)

#define DEFINE_CLK_RPMH_VRM(_name, _suffix, _res_name, _div)		\
	__DEFINE_CLK_RPMH(_name, _name##_suffix, _res_name,		\
			  CLK_RPMH_VRM_EN_OFFSET, 1, _div, true)

#define DEFINE_CLK_RPMH_BCM(_name, _res_name)				\
	static struct clk_rpmh clk_rpmh_##_name = {			\
		.res_name = _res_name,					\
		.valid_state_mask = BIT(RPMH_ACTIVE_ONLY_STATE),	\
		.div = 1,						\
	}

/* Resource name must match resource id present in cmd-db */
DEFINE_CLK_RPMH_ARC(bi_tcxo, "xo.lvl", 0x3, 1);
DEFINE_CLK_RPMH_ARC(bi_tcxo, "xo.lvl", 0x3, 2);
DEFINE_CLK_RPMH_ARC(bi_tcxo, "xo.lvl", 0x3, 4);
DEFINE_CLK_RPMH_ARC(qlink, "qphy.lvl", 0x1, 4);
DEFINE_CLK_RPMH_ARC(xo_pad, "xo.lvl", 0x03, 2);

DEFINE_CLK_RPMH_VRM(ln_bb_clk1, _a2, "lnbclka1", 2);
DEFINE_CLK_RPMH_VRM(ln_bb_clk2, _a2, "lnbclka2", 2);
DEFINE_CLK_RPMH_VRM(ln_bb_clk3, _a2, "lnbclka3", 2);

DEFINE_CLK_RPMH_VRM(ln_bb_clk1, _a4, "lnbclka1", 4);
DEFINE_CLK_RPMH_VRM(ln_bb_clk2, _a4, "lnbclka2", 4);
DEFINE_CLK_RPMH_VRM(ln_bb_clk3, _a4, "lnbclka3", 4);

DEFINE_CLK_RPMH_VRM(ln_bb_clk2, _g4, "lnbclkg2", 4);
DEFINE_CLK_RPMH_VRM(ln_bb_clk3, _g4, "lnbclkg3", 4);

DEFINE_CLK_RPMH_VRM(rf_clk1, _a, "rfclka1", 1);
DEFINE_CLK_RPMH_VRM(rf_clk2, _a, "rfclka2", 1);
DEFINE_CLK_RPMH_VRM(rf_clk3, _a, "rfclka3", 1);
DEFINE_CLK_RPMH_VRM(rf_clk4, _a, "rfclka4", 1);
DEFINE_CLK_RPMH_VRM(rf_clk5, _a, "rfclka5", 1);

DEFINE_CLK_RPMH_VRM(rf_clk3, _a2, "rfclka3", 2);
DEFINE_CLK_RPMH_VRM(rf_clk4, _a2, "rfclka4", 2);
DEFINE_CLK_RPMH_VRM(rf_clk5, _a2, "rfclka5", 2);

DEFINE_CLK_RPMH_VRM(rf_clk1, _d, "rfclkd1", 1);
DEFINE_CLK_RPMH_VRM(rf_clk2, _d, "rfclkd2", 1);
DEFINE_CLK_RPMH_VRM(rf_clk3, _d, "rfclkd3", 1);
DEFINE_CLK_RPMH_VRM(rf_clk4, _d, "rfclkd4", 1);

DEFINE_CLK_RPMH_VRM(slp_clk2, _a, "slpclka2", 1);

DEFINE_CLK_RPMH_VRM(clk1, _a1, "clka1", 1);
DEFINE_CLK_RPMH_VRM(clk2, _a1, "clka2", 1);
DEFINE_CLK_RPMH_VRM(clk3, _a1, "clka3", 1);
DEFINE_CLK_RPMH_VRM(clk4, _a1, "clka4", 1);
DEFINE_CLK_RPMH_VRM(clk5, _a1, "clka5", 1);

DEFINE_CLK_RPMH_VRM(clk3, _a2, "clka3", 2);
DEFINE_CLK_RPMH_VRM(clk4, _a2, "clka4", 2);
DEFINE_CLK_RPMH_VRM(clk5, _a2, "clka5", 2);
DEFINE_CLK_RPMH_VRM(clk6, _a2, "clka6", 2);
DEFINE_CLK_RPMH_VRM(clk7, _a2, "clka7", 2);
DEFINE_CLK_RPMH_VRM(clk8, _a2, "clka8", 2);

DEFINE_CLK_RPMH_VRM(div_clk1, _div2, "divclka1", 2);

DEFINE_CLK_RPMH_VRM(c1a_e0, _div1, "C1A_E0", 1);
DEFINE_CLK_RPMH_VRM(c2a_e0, _div1, "C2A_E0", 1);
DEFINE_CLK_RPMH_VRM(c3a_e0, _div2, "C3A_E0", 2);
DEFINE_CLK_RPMH_VRM(c4a_e0, _div2, "C4A_E0", 2);
DEFINE_CLK_RPMH_VRM(c5a_e0, _div2, "C5A_E0", 2);
DEFINE_CLK_RPMH_VRM(c6a_e0, _div2, "C6A_E0", 2);
DEFINE_CLK_RPMH_VRM(c7a_e0, _div2, "C7A_E0", 2);
DEFINE_CLK_RPMH_VRM(c8a_e0, _div2, "C8A_E0", 2);
DEFINE_CLK_RPMH_VRM(c11a_e0, _div4, "C11A_E0", 4);

// DEFINE_CLK_RPMH_BCM(ce, "CE0");
// DEFINE_CLK_RPMH_BCM(hwkm, "HK0");
DEFINE_CLK_RPMH_BCM(ipa, "IP0");
// DEFINE_CLK_RPMH_BCM(pka, "PKA0");
// DEFINE_CLK_RPMH_BCM(qpic_clk, "QP0");

// DEFINE_CLK_RPMH_FIXED(pineapple, bi_tcxo, bi_tcxo_ao, xo_pad, xo_pad_ao, 2);

static struct clk_rpmh *canoe_rpmh_clocks[] = {
	[RPMH_CXO_PAD_CLK]      = &clk_rpmh_xo_pad_div2,
	[RPMH_CXO_PAD_CLK_A]    = &clk_rpmh_xo_pad_div2_ao,
	// [RPMH_CXO_CLK]          = &pineapple_bi_tcxo,
	// [RPMH_CXO_CLK_A]        = &pineapple_bi_tcxo_ao,
    [RPMH_CXO_CLK]		= &clk_rpmh_bi_tcxo_div2,
	[RPMH_CXO_CLK_A]	= &clk_rpmh_bi_tcxo_div2_ao,
	[RPMH_DIV_CLK1]		= &clk_rpmh_c11a_e0_div4,
	[RPMH_LN_BB_CLK1]	= &clk_rpmh_c6a_e0_div2,
	[RPMH_LN_BB_CLK1_A]	= &clk_rpmh_c6a_e0_div2_ao,
	[RPMH_LN_BB_CLK2]	= &clk_rpmh_c7a_e0_div2,
	[RPMH_LN_BB_CLK2_A]	= &clk_rpmh_c7a_e0_div2_ao,
	[RPMH_LN_BB_CLK3]	= &clk_rpmh_c8a_e0_div2,
	[RPMH_LN_BB_CLK3_A]	= &clk_rpmh_c8a_e0_div2_ao,
	[RPMH_RF_CLK1]		= &clk_rpmh_c1a_e0_div1,
	[RPMH_RF_CLK1_A]	= &clk_rpmh_c1a_e0_div1_ao,
	[RPMH_RF_CLK2]		= &clk_rpmh_c2a_e0_div1,
	[RPMH_RF_CLK2_A]	= &clk_rpmh_c2a_e0_div1_ao,
	[RPMH_RF_CLK3]		= &clk_rpmh_c3a_e0_div2,
	[RPMH_RF_CLK3_A]	= &clk_rpmh_c3a_e0_div2_ao,
	[RPMH_RF_CLK4]		= &clk_rpmh_c4a_e0_div2,
	[RPMH_RF_CLK4_A]	= &clk_rpmh_c4a_e0_div2_ao,
	[RPMH_RF_CLK5]		= &clk_rpmh_c5a_e0_div2,
	[RPMH_RF_CLK5_A]	= &clk_rpmh_c5a_e0_div2_ao,
	[RPMH_IPA_CLK]		= &clk_rpmh_ipa,
};

const struct clk_rpmh_desc clk_rpmh_canoe = {
	.clks = canoe_rpmh_clocks,
	.num_clks = ARRAY_SIZE(canoe_rpmh_clocks),
};

