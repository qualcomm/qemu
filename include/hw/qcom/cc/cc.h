/* Qualcomm generic CC device
 *
 * Author: Romain Malmain <rmalmain@qti.qualcomm.com>
 *
 * Generic object to handle CC devices.
 *
 */

#ifndef QEMU_QCOM_CC_H
#define QEMU_QCOM_CC_H

#include "qemu/osdep.h"
#include "qom/object.h"
#include "hw/sysbus-of.h"

#define TYPE_QCOM_CC      "qcom-cc"
OBJECT_DECLARE_SIMPLE_TYPE(QcomCCState, QCOM_CC)

enum qcom_cc_reg_kind {
    /* PLL registers */
    CC_REG_PLL_MODE,

    /* gdscr registers */
    CC_REG_GDSCR,
    CC_REG_GDSCR_CFG,
    CC_REG_HW_CTRL,
    CC_REG_MAX,
};

typedef uint32_t qcom_cc_regs[CC_REG_MAX];

// ==== adapted from the linux kernel ====

#define PLL_MODE(p)		((p)->offset + 0x0)
# define PLL_OUTCTRL		BIT(0)
# define PLL_BYPASSNL		BIT(1)
# define PLL_RESET_N		BIT(2)
# define PLL_OFFLINE_REQ	BIT(7)
# define PLL_LOCK_COUNT_SHIFT	8
# define PLL_LOCK_COUNT_MASK	0x3f
# define PLL_BIAS_COUNT_SHIFT	14
# define PLL_BIAS_COUNT_MASK	0x3f
# define PLL_VOTE_FSM_ENA	BIT(20)
# define PLL_FSM_ENA		BIT(20)
# define PLL_VOTE_FSM_RESET	BIT(21)
# define PLL_UPDATE		BIT(22)
# define PLL_UPDATE_BYPASS	BIT(23)
# define PLL_FSM_LEGACY_MODE	BIT(24)
# define PLL_OFFLINE_ACK	BIT(28)
# define ALPHA_PLL_ACK_LATCH	BIT(29)
# define PLL_ACTIVE_FLAG	BIT(30)
# define PLL_LOCK_DET		BIT(31)

#define PWR_ON_MASK		BIT(31)
#define EN_REST_WAIT_MASK	GENMASK_ULL(23, 20)
#define EN_FEW_WAIT_MASK	GENMASK_ULL(19, 16)
#define CLK_DIS_WAIT_MASK	GENMASK_ULL(15, 12)
#define SW_OVERRIDE_MASK	BIT(2)
#define HW_CONTROL_MASK		BIT(1)
#define SW_COLLAPSE_MASK	BIT(0)
#define GMEM_CLAMP_IO_MASK	BIT(0)
#define GMEM_RESET_MASK		BIT(4)

/* CFG_GDSCR */
#define GDSC_POWER_UP_COMPLETE		BIT(16)
#define GDSC_POWER_DOWN_COMPLETE	BIT(15)
#define GDSC_RETAIN_FF_ENABLE		BIT(11)
#define CFG_GDSCR_OFFSET		0x4

/**
 * struct clk_alpha_pll - phase locked loop (PLL)
 * @offset: base address of registers
 * @regs: alpha pll register map (see @clk_alpha_pll_regs)
 * @vco_table: array of VCO settings
 * @num_vco: number of VCO settings in @vco_table
 * @flags: bitmask to indicate features supported by the hardware
 * @clkr: regmap clock handle
 */
struct clk_alpha_pll {
	uint32_t offset;
	const uint8_t *regs;
	// struct alpha_pll_config *config;
	// const struct pll_vco *vco_table;
	size_t num_vco;
#define SUPPORTS_OFFLINE_REQ		BIT(0)
#define SUPPORTS_FSM_MODE		BIT(2)
#define SUPPORTS_DYNAMIC_UPDATE	BIT(3)
#define SUPPORTS_FSM_LEGACY_MODE	BIT(4)
#define DISABLE_TO_OFF		BIT(5)
#define ENABLE_IN_PREPARE	BIT(6)
	uint8_t flags;

	// struct clk_regmap clkr;
};


/**
 * struct gdsc - Globally Distributed Switch Controller
 * @pd: generic power domain
 * @regmap: regmap for MMIO accesses
 * @gdscr: gsdc control register
 * @collapse_ctrl: APCS collapse-vote register
 * @collapse_mask: APCS collapse-vote mask
 * @gds_hw_ctrl: gds_hw_ctrl register
 * @cxcs: offsets of branch registers to toggle mem/periph bits in
 * @cxc_count: number of @cxcs
 * @pwrsts: Possible powerdomain power states
 * @en_rest_wait_val: transition delay value for receiving enr ack signal
 * @en_few_wait_val: transition delay value for receiving enf ack signal
 * @clk_dis_wait_val: transition delay value for halting clock
 * @resets: ids of resets associated with this gdsc
 * @reset_count: number of @resets
 * @rcdev: reset controller
 */
struct gdsc {
	// struct generic_pm_domain	pd;
	// struct generic_pm_domain	*parent;
	// struct regmap			*regmap;
	unsigned int			gdscr;
	unsigned int			collapse_ctrl;
	unsigned int			collapse_mask;
	unsigned int			gds_hw_ctrl;
	unsigned int			clamp_io_ctrl;
	unsigned int			*cxcs;
	unsigned int			cxc_count;
	unsigned int			en_rest_wait_val;
	unsigned int			en_few_wait_val;
	unsigned int			clk_dis_wait_val;
	const uint8_t			pwrsts;
/* Powerdomain allowable state bitfields */
#define PWRSTS_OFF		BIT(0)
/*
 * There is no SW control to transition a GDSC into
 * PWRSTS_RET. This happens in HW when the parent
 * domain goes down to a low power state
 */
#define PWRSTS_RET		BIT(1)
#define PWRSTS_ON		BIT(2)
#define PWRSTS_OFF_ON		(PWRSTS_OFF | PWRSTS_ON)
#define PWRSTS_RET_ON		(PWRSTS_RET | PWRSTS_ON)
	const uint16_t			flags;
#define VOTABLE		BIT(0)
#define CLAMP_IO	BIT(1)
#define HW_CTRL		BIT(2)
#define SW_RESET	BIT(3)
#define AON_RESET	BIT(4)
#define POLL_CFG_GDSCR	BIT(5)
#define ALWAYS_ON	BIT(6)
#define RETAIN_FF_ENABLE	BIT(7)
#define NO_RET_PERIPH	BIT(8)
#define HW_CTRL_TRIGGER	BIT(9)
#define HW_CTRL_SKIP_DIS	BIT(10)
	struct reset_controller_dev	*rcdev;
	unsigned int			*resets;
	unsigned int			reset_count;

	const char 			*supply;
	struct regulator		*rsupply;

	const char			*path_name;
	struct icc_path			*path;
};

struct qcom_cc_desc {
	// const struct regmap_config *config;
	// struct clk_regmap **clks;
	// struct critical_clk_offset *critical_clk_en;
	// size_t num_critical_clk;
	// size_t num_clks;
	// const struct qcom_reset_map *resets;
	// size_t num_resets;
	struct gdsc **gdscs;
	size_t num_gdscs;
	// struct clk_hw **clk_hws;
	// size_t num_clk_hws;
	// struct clk_vdd_class **clk_regulators;
	// size_t num_clk_regulators;
	// struct icc_path *path;
	// struct qcom_icc_hws_data *icc_hws;
	// size_t num_icc_hws;
	// unsigned int icc_first_node_id;

    /* added fields */
    struct clk_alpha_pll **plls;
    size_t num_plls;

    qcom_cc_regs reset_regs;
};

// ==== it ends here ====

struct QcomCCState {
    OfSysBusDevice parent;

    const char* name;
    size_t mem_size;

    MemoryRegion iomem;

    qcom_cc_regs reg;
};

QcomCCState* cc_create_by_label(void* fdt, void* in_fdt, const char* label, uint64_t mem_size);

#endif
