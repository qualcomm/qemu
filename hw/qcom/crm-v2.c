#include "hw/sysbus-of.h"
#include "qemu/osdep.h"
#include "qapi/error.h"
#include "hw/sysbus.h"
#include "hw/qdev-properties.h"
#include "hw/qcom/crm-v2.h"

#define CRM_V2_LOG(dev, fmt, ...) QDEV_LOG_INFO(dev, fmt __VA_OPT__(,) __VA_ARGS__)
#define CRM_V2_LOG_ERROR(dev, fmt, ...) QDEV_LOG_ERROR(dev, fmt __VA_OPT__(,) __VA_ARGS__)

#define field_set(_mask, _val) (((_val) & (_mask >> (ffs(_mask) - 1))) << (ffs(_mask) - 1))

// === taken from drivers/soc/qcom/crm-v2.c  ===
// === copy from the kernel tree starts here ===

#define field_get(_mask, _reg) (((_reg) & (_mask)) >> (ffs(_mask) - 1))

/* Capability flags  */
#define PERF_OL_VOTING_FLAG	BIT(0)
#define BW_VOTING_FLAG		BIT(1)
#define BW_PT_VOTING_FLAG	BIT(2)

#define VPAGE_SHIFT_BITS		0xFFF

/* Applicable for HW & SW DRVs BW Registers */
#define PERF_OL_VALUE_BITS		0x7

/* Applicable for HW & SW DRVs BW Registers */
#define BW_VOTE_VALID			BIT(29)
/* Applicable only for SW DRVs BW PT Registers */
#define BW_PT_VOTE_VALID		BIT(29)
#define BW_PT_VOTE_TRIGGER		BIT(0)
/* Applicable only for SW DRVs BW Registers */
#define BW_VOTE_RESP_REQ		BIT(31)

/* Set 1 to Enable IRQ for each VCD */
#define IRQ_ENABLE_BIT			BIT(0)
#define IRQ_CLEAR_BIT			BIT(0)

/* Offsets for CURR_PERF_OL Register */
#define CURR_PER_OL_MASK		0x7

/* Set 1 to Enable CHN_BEHAVE and CHANNEL_SWITCH_CTRL for each HW DRV */
#define CHN_BEHAVE_BIT			BIT(0)
#define CHN_SWITCH_CTRL		BIT(1)

/* SW DRV has ACTIVE, SLEEP and WAKE PWR STATES */
#define MAX_SW_DRV_PWR_STATES		3

/* Time out for ACTIVE Only PWR STATE completion IRQ */
#define CRM_TIMEOUT_MS			msecs_to_jiffies(CONFIG_QCOM_RPMH_TIMEOUT)

#define CH0				0
#define CH0_CHN_BUSY			BIT(0)
#define CH1				1
#define CH1_CHN_BUSY			BIT(1)

enum {
	CRM_VERSION,
	MAJOR_VERSION,
	MINOR_VERSION,
	CRM_CFG_PARAM_1,
	NUM_OF_NODES_PT,
	NUM_VCD_VOTED_BY_BW,
	NUM_SW_DRVS,
	NUM_HW_DRVS,
	NUM_OF_RAILS,
	NUM_VCD_VOTED_BY_PERF_OL,
	NUM_CHANNELS,
	NUM_PWR_STATES_PER_CH,
	CRM_CFG_PARAM_2,
	NUM_OF_NODES,
	CRM_ENABLE,
	CFG_REG_MAX,
};

enum {
/* CRM DRV Register */
	DRV_BASE,
	DRV_DISTANCE,
/* VCD or ND Distance */
	DRV_RESOURCE_DISTANCE,
/* DRV's PWR_ST Registers */
	PWR_ST0,
	PWR_ST1,
	PWR_ST2,
	PWR_ST3,
	PWR_ST4,
/* DRV's PWR_ST Passthrough Registers */
	PWR_ST0_PT = PWR_ST0,
	PWR_ST1_PT = PWR_ST1,
	PWR_ST2_PT = PWR_ST2,
	PWR_ST3_PT = PWR_ST3,
	PWR_ST4_PT = PWR_ST4,
/* Offset for power state distances in a channel */
	PWR_ST_CHN_DISTANCE,
/* VCD's IRQ Registers, one per VCD at VCD_DISTANCE */
	IRQ_STATUS,
	IRQ_CLEAR,
	IRQ_ENABLE,
	FSM_STATUS,
	CRMB_PT_TRIGGER,
	STATUS,
	PWR_IDX_STATUS,
	CRM_CLIENT_REG_MAX,
};

enum {
/* SW DRV's PWR_ST mapped to PWR_ST0/1/2 for ACTIVE/SLEEP/WAKE */
	ACTIVE_VOTE = PWR_ST0,
	SLEEP_VOTE = PWR_ST1,
	WAKE_VOTE = PWR_ST2,
};

enum {
/* DRV's Channel Registers, one per DRV at CH_DRV_DISTANCE */
	CHN_BUSY,
	CHN_UPDATE,
	CHN_BEHAVE,
	CHN_DRV_DISTANCE,
	CHN_REG_MAX,
};

enum {
/* CRM DRV Register */
	CRM_BASE,
	CRM_DISTANCE,
	CRMB_BASE = CRM_BASE,
	CRMB_DISTANCE = CRM_DISTANCE,
/* CRMB Registers */
	STATUS_BE,
	STATUS_FE,
	CRMB_REG_MAX,
};

enum {
/* CRM DRV Register */
	CRMB_PT_BASE = CRM_BASE,
	CRMB_PT_DISTANCE = CRM_DISTANCE,
/* CRMB_PT Registers */
	TCS_CMD_DATA,
	TCS_CMD_ADDR,
	TCS_CMD_CTRL,
	TCS_CMD_STATUS,
	TCS_CMD_ENABLE,
	CRMB_PT_FSM_STATUS,
	CRMB_PT_REG_MAX,
};

enum {
/* CRM DRV Register */
	CRMC_BASE = CRM_BASE,
	CRMC_DISTANCE = CRM_DISTANCE,
/* CRMC Registers */
	AGGR_PERF_OL,
	AGGR_PERF_OL_RESOURCE_DISTANCE,
	CURR_PERF_OL,
	CURR_PERF_OL_RESOURCE_DISTANCE,
	SEQ_STATUS,
	SEQ_STATUS_RESOURCE_DISTANCE,
	CRMC_REG_MAX,
};

enum {
/* CRM DRV Register */
	CRMV_BASE = CRM_BASE,
	CRMV_DISTANCE = CRM_DISTANCE,
/* CRMV Registers */
	AGGR_VOL_STS,
	SEQ_VOL_STS,
	CURR_VOL_STS,
	RAIL_FSM_STS,
	RAIL_TCS_STS,
	CRMV_REG_MAX,
};

enum channel_type {
	CHN_IN_USE,
	CHN_FREE,
};

struct crm_desc {
	bool set_chn_behave;
	bool set_hw_chn_switch_ctrl;
	uint32_t crm_capability;
	uint32_t cfg_regs[CFG_REG_MAX];
	uint32_t chn_regs[CHN_REG_MAX];
	uint32_t crmb_regs[CRMB_REG_MAX];
	uint32_t crmb_pt_regs[CRMB_PT_REG_MAX];
	uint32_t crmc_regs[CRMC_REG_MAX];
	uint32_t crmv_regs[CRMV_REG_MAX];
	uint32_t hw_drv_perf_ol_vcd_regs[CRM_CLIENT_REG_MAX];
	uint32_t hw_drv_bw_vote_vcd_regs[CRM_CLIENT_REG_MAX];
	uint32_t hw_drv_bw_pt_vote_vcd_regs[CRM_CLIENT_REG_MAX];
	uint32_t sw_drv_perf_ol_vcd_regs[CRM_CLIENT_REG_MAX];
	uint32_t sw_drv_bw_vote_vcd_regs[CRM_CLIENT_REG_MAX];
	uint32_t sw_drv_bw_pt_vote_vcd_regs[CRM_CLIENT_REG_MAX];
};

static const struct crm_desc cam_crm_desc_v3 = {
	.set_chn_behave = true,
	.set_hw_chn_switch_ctrl = false,
	.crm_capability = PERF_OL_VOTING_FLAG | BW_VOTING_FLAG,
	.cfg_regs = {
		[CRM_VERSION]			= 0x0,
		[MAJOR_VERSION]			= GENMASK(23, 16),
		[MINOR_VERSION]			= GENMASK(15, 8),
		[CRM_CFG_PARAM_1]		= 0x4,
		[NUM_OF_NODES_PT]		= GENMASK(30, 26),
		[NUM_VCD_VOTED_BY_BW]		= GENMASK(25, 23),
		[NUM_SW_DRVS]			= GENMASK(22, 18),
		[NUM_HW_DRVS]			= GENMASK(17, 14),
		[NUM_OF_RAILS]			= GENMASK(13, 10),
		[NUM_VCD_VOTED_BY_PERF_OL]	= GENMASK(9, 6),
		[NUM_CHANNELS]			= GENMASK(5, 4),
		[NUM_PWR_STATES_PER_CH]		= GENMASK(3, 0),
		[CRM_CFG_PARAM_2]		= 0x8,
		[NUM_OF_NODES]			= GENMASK(30, 26),
		[CRM_ENABLE]			= 0xC,
	},
	.chn_regs = {
		[CHN_BUSY]			 = 0xDC,
		[CHN_UPDATE]			 = 0xE0,
		[CHN_BEHAVE]			 = 0xE4,
		[CHN_DRV_DISTANCE]		 = 0x29C,
	},
	.crmb_regs = {
		[CRM_BASE]			 = 0x0,
		[CRM_DISTANCE]			 = 0x50,
		[STATUS_BE]			 = 0x18,
		[STATUS_FE]			 = 0x1C,
	},
	.crmc_regs = {
		[CRM_BASE]			 = 0x0,
		[CRM_DISTANCE]			 = 0x0,
		[AGGR_PERF_OL]			 = 0x4,
		[AGGR_PERF_OL_RESOURCE_DISTANCE] = 0xC,
		[CURR_PERF_OL]			 = 0x6C,
		[CURR_PERF_OL_RESOURCE_DISTANCE] = 0x210,
		[SEQ_STATUS]			 = 0x94,
		[SEQ_STATUS_RESOURCE_DISTANCE]	 = 0x210,
	},
	.crmv_regs = {
		[CRM_BASE]			= 0x0,
		[CRM_DISTANCE]			= 0x40,
		[AGGR_VOL_STS]			= 0x4,
		[SEQ_VOL_STS]			= 0x8,
		[CURR_VOL_STS]			= 0xC,
		[RAIL_FSM_STS]			= 0x14,
		[RAIL_TCS_STS]			= 0x3C,
	},
	.hw_drv_perf_ol_vcd_regs = {
		[DRV_BASE]			 = 0x0,
		[DRV_DISTANCE]			 = 0x29C,
		[DRV_RESOURCE_DISTANCE]		 = 0x14,
		[PWR_ST0]			 = 0x0,
		[PWR_ST1]			 = 0x4,
		[PWR_ST_CHN_DISTANCE]		 = 0x8,
		[STATUS]			 = 0x10,
		[PWR_IDX_STATUS]		 = 0xE8,
	},
	.hw_drv_bw_vote_vcd_regs = {
		[DRV_BASE]			 = 0x0,
		[DRV_DISTANCE]			 = 0x29C,
		[DRV_RESOURCE_DISTANCE]		 = 0x14,
		[PWR_ST0]			 = 0xA0,
		[PWR_ST1]			 = 0xA4,
		[PWR_ST_CHN_DISTANCE]		 = 0x8,
		[STATUS]			 = 0xB0,
	},
	.hw_drv_bw_pt_vote_vcd_regs = {
		[DRV_BASE]			 = 0x0,
		[DRV_DISTANCE]			 = 0x29C,
		[DRV_RESOURCE_DISTANCE]		 = 0x14,
		[PWR_ST0_PT]			 = 0xC8,
		[PWR_ST1_PT]			 = 0xCC,
		[PWR_ST_CHN_DISTANCE]		 = 0x8,
		[STATUS]			 = 0xD8,
	},
	.sw_drv_perf_ol_vcd_regs = {
		[DRV_BASE]			 = 0x11C,
		[DRV_DISTANCE]			 = 0x29C,
		[DRV_RESOURCE_DISTANCE]		 = 0x20,
		[PWR_ST0]			 = 0x0,
		[PWR_ST1]			 = 0x4,
		[PWR_ST2]			 = 0x8,
		[PWR_ST_CHN_DISTANCE]		 = 0x0,
		[STATUS]			 = 0xC,
		[IRQ_STATUS]			 = 0x10,
		[IRQ_CLEAR]			 = 0x14,
		[IRQ_ENABLE]			 = 0x18,
		[FSM_STATUS]			 = 0x1C,
	},
	.sw_drv_bw_vote_vcd_regs = {
		[DRV_BASE]			 = 0x11C,
		[DRV_DISTANCE]			 = 0x29C,
		[DRV_RESOURCE_DISTANCE]		 = 0x10,
		[PWR_ST0]			 = 0x100,
		[PWR_ST1]			 = 0x104,
		[PWR_ST2]			 = 0x108,
		[PWR_ST_CHN_DISTANCE]		 = 0x0,
		[STATUS]			 = 0x10C,
		[IRQ_STATUS]			 = 0x130,
		[IRQ_CLEAR]			 = 0x134,
		[IRQ_ENABLE]			 = 0x138,
		[FSM_STATUS]			 = 0x13C,
	},
	.sw_drv_bw_pt_vote_vcd_regs = {
		[DRV_BASE]			 = 0x11C,
		[DRV_DISTANCE]			 = 0x29C,
		[DRV_RESOURCE_DISTANCE]		 = 0x10,
		[PWR_ST0_PT]			 = 0x120,
		[PWR_ST1_PT]			 = 0x124,
		[PWR_ST2_PT]			 = 0x128,
		[PWR_ST_CHN_DISTANCE]		 = 0x0,
		[STATUS]			 = 0x12C,
		[IRQ_STATUS]			 = 0x140,
		[IRQ_CLEAR]			 = 0x144,
		[IRQ_ENABLE]			 = 0x148,
		[CRMB_PT_TRIGGER]		 = 0x150,
	},
};

static const struct crm_desc disp_crm_desc_v3 = {
	.set_chn_behave = false,
	.set_hw_chn_switch_ctrl = true,
	.crm_capability = BW_VOTING_FLAG | BW_PT_VOTING_FLAG,
	.cfg_regs = {
		[CRM_VERSION]			= 0x0,
		[MAJOR_VERSION]			= GENMASK(23, 16),
		[MINOR_VERSION]			= GENMASK(15, 8),
		[CRM_CFG_PARAM_1]		= 0x4,
		[NUM_OF_NODES_PT]		= GENMASK(30, 26),
		[NUM_VCD_VOTED_BY_BW]		= GENMASK(25, 23),
		[NUM_SW_DRVS]			= GENMASK(22, 18),
		[NUM_HW_DRVS]			= GENMASK(17, 14),
		[NUM_OF_RAILS]			= GENMASK(13, 10),
		[NUM_VCD_VOTED_BY_PERF_OL]	= GENMASK(9, 6),
		[NUM_CHANNELS]			= GENMASK(5, 4),
		[NUM_PWR_STATES_PER_CH]		= GENMASK(3, 0),
		[CRM_CFG_PARAM_2]		= 0x8,
		[NUM_OF_NODES]			= GENMASK(30, 26),
		[CRM_ENABLE]			= 0xC,
	},
	.chn_regs = {
		[CHN_BUSY]			 = 0xA0,
		[CHN_UPDATE]			 = 0xA4,
		[CHN_BEHAVE]			 = 0xA8,
		[CHN_DRV_DISTANCE]		 = 0x1000,
	},
	.crmb_regs = {
		[CRM_BASE]			 = 0x0,
		[CRM_DISTANCE]			 = 0x90,
		[STATUS_BE]			 = 0x18,
		[STATUS_FE]			 = 0x1C,
	},
	.crmb_pt_regs = {
		[CRM_BASE]			 = 0x0,
		[CRM_DISTANCE]			 = 0x14,
		[TCS_CMD_DATA]			 = 0x0,
		[TCS_CMD_ADDR]			 = 0x4,
		[TCS_CMD_CTRL]			 = 0x8,
		[TCS_CMD_STATUS]		 = 0xC,
		[TCS_CMD_ENABLE]		 = 0x10,
		[CRMB_PT_FSM_STATUS]		 = 0x7c,
	},
	.crmc_regs = {
		[CRM_BASE]			 = 0x0,
		[CRM_DISTANCE]			 = 0x0,
		[AGGR_PERF_OL]			 = 0x4,
		[AGGR_PERF_OL_RESOURCE_DISTANCE] = 0xC,
		[CURR_PERF_OL]			 = 0x18,
		[CURR_PERF_OL_RESOURCE_DISTANCE] = 0x268,
		[SEQ_STATUS]			 = 0x40,
		[SEQ_STATUS_RESOURCE_DISTANCE]	 = 0x268,
	},
	.crmv_regs = {
		[CRM_BASE]			= 0x0,
		[CRM_DISTANCE]			= 0x40,
		[AGGR_VOL_STS]			= 0x4,
		[SEQ_VOL_STS]			= 0x8,
		[CURR_VOL_STS]			= 0xC,
		[RAIL_FSM_STS]			= 0x14,
		[RAIL_TCS_STS]			= 0x3C,
	},
	.hw_drv_perf_ol_vcd_regs = {
		[DRV_BASE]			 = 0x0,
		[DRV_DISTANCE]			 = 0x1000,
		[DRV_RESOURCE_DISTANCE]		 = 0x14,
		[PWR_ST0]			 = 0x0,
		[PWR_ST1]			 = 0x4,
		[PWR_ST_CHN_DISTANCE]		 = 0x8,
		[STATUS]			 = 0x10,
		[PWR_IDX_STATUS]		 = 0xAC,
	},
	.hw_drv_bw_vote_vcd_regs = {
		[DRV_BASE]			 = 0x0,
		[DRV_DISTANCE]			 = 0x1000,
		[DRV_RESOURCE_DISTANCE]		 = 0x14,
		[PWR_ST0]			 = 0x14,
		[PWR_ST1]			 = 0x18,
		[PWR_ST_CHN_DISTANCE]		 = 0x8,
		[STATUS]			 = 0x24,
	},
	.hw_drv_bw_pt_vote_vcd_regs = {
		[DRV_BASE]			 = 0x0,
		[DRV_DISTANCE]			 = 0x1000,
		[DRV_RESOURCE_DISTANCE]		 = 0x14,
		[PWR_ST0_PT]			 = 0x28,
		[PWR_ST1_PT]			 = 0x2C,
		[PWR_ST_CHN_DISTANCE]		 = 0x8,
		[STATUS]			 = 0x38,
	},
	.sw_drv_perf_ol_vcd_regs = {
		[DRV_BASE]			 = 0xE0,
		[DRV_DISTANCE]			 = 0x1000,
		[DRV_RESOURCE_DISTANCE]		 = 0x20,
		[PWR_ST0]			 = 0x0,
		[PWR_ST1]			 = 0x4,
		[PWR_ST2]			 = 0x8,
		[PWR_ST_CHN_DISTANCE]		 = 0x0,
		[STATUS]			 = 0xC,
		[IRQ_STATUS]			 = 0x10,
		[IRQ_CLEAR]			 = 0x14,
		[IRQ_ENABLE]			 = 0x18,
		[FSM_STATUS]			 = 0x1C,
	},
	.sw_drv_bw_vote_vcd_regs = {
		[DRV_BASE]			 = 0xE0,
		[DRV_DISTANCE]			 = 0x1000,
		[DRV_RESOURCE_DISTANCE]		 = 0x10,
		[PWR_ST0]			 = 0x20,
		[PWR_ST1]			 = 0x24,
		[PWR_ST2]			 = 0x28,
		[PWR_ST_CHN_DISTANCE]		 = 0x0,
		[STATUS]			 = 0x2C,
		[IRQ_STATUS]			 = 0x90,
		[IRQ_CLEAR]			 = 0x94,
		[IRQ_ENABLE]			 = 0x98,
		[FSM_STATUS]			 = 0x9C,
	},
	.sw_drv_bw_pt_vote_vcd_regs = {
		[DRV_BASE]			 = 0xE0,
		[DRV_DISTANCE]			 = 0x1000,
		[DRV_RESOURCE_DISTANCE]		 = 0x10,
		[PWR_ST0_PT]			 = 0x30,
		[PWR_ST1_PT]			 = 0x34,
		[PWR_ST2_PT]			 = 0x38,
		[PWR_ST_CHN_DISTANCE]		 = 0x0,
		[STATUS]			 = 0x3C,
		[IRQ_STATUS]			 = 0xA0,
		[IRQ_CLEAR]			 = 0xA4,
		[IRQ_ENABLE]			 = 0xA8,
		[CRMB_PT_TRIGGER]		 = 0x100,
	},
};

static const struct crm_desc pcie_crm_desc_v3 = {
	.set_chn_behave = false,
	.set_hw_chn_switch_ctrl = false,
	.crm_capability = PERF_OL_VOTING_FLAG | BW_PT_VOTING_FLAG,
	.cfg_regs = {
		[CRM_VERSION]			= 0x0,
		[MAJOR_VERSION]			= GENMASK(23, 16),
		[MINOR_VERSION]			= GENMASK(15, 8),
		[CRM_CFG_PARAM_1]		= 0x4,
		[NUM_OF_NODES_PT]		= GENMASK(30, 26),
		[NUM_VCD_VOTED_BY_BW]		= GENMASK(25, 23),
		[NUM_SW_DRVS]			= GENMASK(22, 18),
		[NUM_HW_DRVS]			= GENMASK(17, 14),
		[NUM_OF_RAILS]			= GENMASK(13, 10),
		[NUM_VCD_VOTED_BY_PERF_OL]	= GENMASK(9, 6),
		[NUM_CHANNELS]			= GENMASK(5, 4),
		[NUM_PWR_STATES_PER_CH]		= GENMASK(3, 0),
		[CRM_CFG_PARAM_2]		= 0x8,
		[NUM_OF_NODES]			= GENMASK(30, 26),
		[CRM_ENABLE]			= 0xC,
	},
	.chn_regs = {
		[CHN_BUSY]			 = 0x370,
		[CHN_UPDATE]			 = 0x374,
		[CHN_BEHAVE]			 = 0x378,
		[CHN_DRV_DISTANCE]		 = 0x1000,
	},
	.crmb_regs = {
		[CRM_BASE]			 = 0x0,
		[CRM_DISTANCE]			 = 0x50,
		[STATUS_BE]			 = 0x18,
		[STATUS_FE]			 = 0x1C,
	},
	.crmb_pt_regs = {
		[CRM_BASE]			 = 0x0,
		[CRM_DISTANCE]			 = 0x14,
		[TCS_CMD_DATA]			 = 0x0,
		[TCS_CMD_ADDR]			 = 0x4,
		[TCS_CMD_CTRL]			 = 0x8,
		[TCS_CMD_STATUS]		 = 0xC,
		[TCS_CMD_ENABLE]		 = 0x10,
		[CRMB_PT_FSM_STATUS]		 = 0x144,
	},
	.crmc_regs = {
		[CRM_BASE]			 = 0x0,
		[CRM_DISTANCE]			 = 0,
		[AGGR_PERF_OL]			 = 0x4,
		[AGGR_PERF_OL_RESOURCE_DISTANCE] = 0xC,
		[CURR_PERF_OL]			 = 0x24,
		[CURR_PERF_OL_RESOURCE_DISTANCE] = 0x2B0,
		[SEQ_STATUS]			 = 0x4C,
		[SEQ_STATUS_RESOURCE_DISTANCE]	 = 0x2B0,
	},
	.crmv_regs = {
		[CRM_BASE]			= 0x0,
		[CRM_DISTANCE]			= 0x40,
		[AGGR_VOL_STS]			= 0x4,
		[SEQ_VOL_STS]			= 0x8,
		[CURR_VOL_STS]			= 0xC,
		[RAIL_FSM_STS]			= 0x14,
		[RAIL_TCS_STS]			= 0x3C,
	},
	.hw_drv_perf_ol_vcd_regs = {
		[DRV_BASE]			 = 0x0,
		[DRV_DISTANCE]			 = 0x1000,
		[DRV_RESOURCE_DISTANCE]		 = 0x2C,
		[PWR_ST0]			 = 0x0,
		[PWR_ST1]			 = 0x4,
		[PWR_ST2]			 = 0x8,
		[PWR_ST3]			 = 0xC,
		[PWR_ST4]			 = 0x10,
		[PWR_ST_CHN_DISTANCE]		 = 0x14,
		[STATUS]			 = 0x28,
		[PWR_IDX_STATUS]		 = 0x37C,
	},
	.hw_drv_bw_vote_vcd_regs = {
		[DRV_BASE]			 = 0x0,
		[DRV_DISTANCE]			 = 0x1000,
		[DRV_RESOURCE_DISTANCE]		 = 0x2C,
		[PWR_ST0]			 = 0x58,
		[PWR_ST1]			 = 0x5C,
		[PWR_ST2]			 = 0x60,
		[PWR_ST3]			 = 0x64,
		[PWR_ST4]			 = 0x68,
		[PWR_ST_CHN_DISTANCE]		 = 0x14,
		[STATUS]			 = 0x80,
	},
	.hw_drv_bw_pt_vote_vcd_regs = {
		[DRV_BASE]			 = 0x0,
		[DRV_DISTANCE]			 = 0x1000,
		[DRV_RESOURCE_DISTANCE]		 = 0x2C,
		[PWR_ST0_PT]			 = 0xB0,
		[PWR_ST1_PT]			 = 0xB4,
		[PWR_ST2_PT]			 = 0xB8,
		[PWR_ST3_PT]			 = 0xBC,
		[PWR_ST4_PT]			 = 0xC0,
		[PWR_ST_CHN_DISTANCE]		 = 0x14,
		[STATUS]			 = 0xD8,
	},
	.sw_drv_perf_ol_vcd_regs = {
		[DRV_BASE]			 = 0x3B0,
		[DRV_DISTANCE]			 = 0x1000,
		[DRV_RESOURCE_DISTANCE]		 = 0x20,
		[PWR_ST0]			 = 0x0,
		[PWR_ST1]			 = 0x4,
		[PWR_ST2]			 = 0x8,
		[PWR_ST_CHN_DISTANCE]		 = 0x0,
		[STATUS]			 = 0xC,
		[IRQ_STATUS]			 = 0x10,
		[IRQ_CLEAR]			 = 0x14,
		[IRQ_ENABLE]			 = 0x18,
		[FSM_STATUS]			 = 0X1C,
	},
	.sw_drv_bw_vote_vcd_regs = {
		[DRV_BASE]			 = 0x3B0,
		[DRV_DISTANCE]			 = 0x1000,
		[DRV_RESOURCE_DISTANCE]		 = 0x10,
		[PWR_ST0]			 = 0x40,
		[PWR_ST1]			 = 0x44,
		[PWR_ST2]			 = 0x48,
		[PWR_ST_CHN_DISTANCE]		 = 0x0,
		[STATUS]			 = 0x4C,
		[IRQ_STATUS]			 = 0x160,
		[IRQ_CLEAR]			 = 0x164,
		[IRQ_ENABLE]			 = 0x168,
		[FSM_STATUS]			 = 0X16C,
	},
	.sw_drv_bw_pt_vote_vcd_regs = {
		[DRV_BASE]			 = 0x3B0,
		[DRV_DISTANCE]			 = 0x1000,
		[DRV_RESOURCE_DISTANCE]		 = 0x10,
		[PWR_ST0_PT]			 = 0x60,
		[PWR_ST1_PT]			 = 0x64,
		[PWR_ST2_PT]			 = 0x68,
		[PWR_ST_CHN_DISTANCE]		 = 0x0,
		[STATUS]			 = 0x6C,
		[IRQ_STATUS]			 = 0x170,
		[IRQ_CLEAR]			 = 0x174,
		[IRQ_ENABLE]			 = 0x178,
		[CRMB_PT_TRIGGER]		 = 0x270,
	},
};

// === copy from the kernel tree ends here ===

enum {
	CRM_DRV_BASE,
	CRM_CRMB_MGR,
	CRM_CRMB_PT_MGR,
	CRM_CRMC_MGR,
	CRM_CRMV_MGR,
	CRM_COMMON,
};

static uint64_t read_drv_base(QcomCrmState* cdev, const uint32_t* reg_param, int enum_idx)
{
    CRM_V2_LOG(cdev, "[drv_base]: read detected @idx %d\n", enum_idx);

	return 0;
}

static void write_drv_base(QcomCrmState* cdev, const uint32_t* reg_param, int enum_idx, uint64_t value)
{
    CRM_V2_LOG(cdev, "[drv_base]: write@idx %d of value 0x%lx\n", enum_idx, value);
}

static uint64_t read_crmb_mgr(QcomCrmState* cdev, const uint32_t* reg_param, int enum_idx)
{
    CRM_V2_LOG(cdev, "[crmb_mgr]: read detected @idx %d\n", enum_idx);

	return 0;
}

static void write_crmb_mgr(QcomCrmState* cdev, const uint32_t* reg_param, int enum_idx, uint64_t value)
{
    CRM_V2_LOG(cdev, "[crmb_mgr]: write@idx %d of value 0x%lx\n", enum_idx, value);
}

static uint64_t read_crmb_pt_mgr(QcomCrmState* cdev, const uint32_t* reg_param, int enum_idx)
{
    CRM_V2_LOG(cdev, "[crmb_pt_mgr]: read detected @idx %d\n", enum_idx);

	return 0;
}

static void write_crmb_pt_mgr(QcomCrmState* cdev, const uint32_t* reg_param, int enum_idx, uint64_t value)
{
    CRM_V2_LOG(cdev, "[crmb_pt_mgr]: write@idx %d of value 0x%lx\n", enum_idx, value);
}

static uint64_t read_crmc_mgr(QcomCrmState* cdev, const uint32_t* reg_param, int enum_idx)
{
    CRM_V2_LOG(cdev, "[crmc_mgr]: read detected @idx %d\n", enum_idx);

	return 0;
}

static void write_crmc_mgr(QcomCrmState* cdev, const uint32_t* reg_param, int enum_idx, uint64_t value)
{
    CRM_V2_LOG(cdev, "[crmc_mgr]: write@idx %d of value 0x%lx\n", enum_idx, value);
}

static uint64_t read_crmv_mgr(QcomCrmState* cdev, const uint32_t* reg_param, int enum_idx)
{
    CRM_V2_LOG(cdev, "[crmv_mgr]: read detected @idx %d\n", enum_idx);

	return 0;
}

static void write_crmv_mgr(QcomCrmState* cdev, const uint32_t* reg_param, int enum_idx, uint64_t value)
{
    CRM_V2_LOG(cdev, "[crmv_mgr]: write@idx %d of value 0x%lx\n", enum_idx, value);
}

static uint64_t read_common(QcomCrmState* cdev, const uint32_t* reg_param, int enum_idx)
{
    switch(enum_idx) {
		case CRM_ENABLE:
            CRM_V2_LOG(cdev, "[common] CRM enabled.\n");
            return 1;
        case CRM_VERSION:
            CRM_V2_LOG(cdev, "[common] Version skipped - unused in driver.\n");
            return 0;
        case CRM_CFG_PARAM_1: {
            uint32_t cfg = 0;
            cfg |= field_set(reg_param[NUM_HW_DRVS], 9);
            cfg |= field_set(reg_param[NUM_SW_DRVS], 9);
            cfg |= field_set(reg_param[NUM_CHANNELS], 2);

            CRM_V2_LOG(cdev, "[common] param 1 cfg = 0x%x\n", cfg);
            return cfg;
        }
        default:
            CRM_V2_LOG(cdev, "[common]: unknown read detected @idx %d\n", enum_idx);
            return 0;
    }

    abort();
}

static void write_common(QcomCrmState* cdev, const uint32_t* reg_param, int enum_idx, uint64_t value)
{
    CRM_V2_LOG(cdev, "[common]: write@idx %d of value 0x%lx\n", enum_idx, value);
}

typedef uint64_t(*CrmRdHdlr)(QcomCrmState*, const uint32_t*, int);
typedef void(*CrmWrHdlr)(QcomCrmState*, const uint32_t*, int, uint64_t);

struct crm_kind_handlers {
	CrmRdHdlr read_handler;
	CrmWrHdlr write_handler;
    const uint32_t* reg_array;
    size_t reg_array_size;
};

struct crm_kind_handlers crm_regs_handlers[] = {
	[CRM_DRV_BASE] = {
		.read_handler = read_drv_base,
		.write_handler = write_drv_base,
	},
	[CRM_CRMB_MGR] = {
		.read_handler = read_crmb_mgr,
		.write_handler = write_crmb_mgr,
	},
	[CRM_CRMB_PT_MGR] = {
		.read_handler = read_crmb_pt_mgr,
		.write_handler = write_crmb_pt_mgr,
	},
	[CRM_CRMC_MGR] = {
		.read_handler = read_crmc_mgr,
		.write_handler = write_crmc_mgr,
	},
	[CRM_CRMV_MGR] = {
		.read_handler = read_crmv_mgr,
		.write_handler = write_crmv_mgr,
	},
	[CRM_COMMON] = {
		.read_handler = read_common,
		.write_handler = write_common,
	},
};


static uint64_t qcom_crm_read(void *opaque, hwaddr addr, unsigned size)
{
	QcomCrmState* cs = QCOM_CRM(opaque);
	OfSysBusDevice* ofdev = OF_SYS_BUS_DEVICE(cs);
    // const struct crm_desc* desc = ofdev->data;

	hwaddr offset = -1;
	size_t i;
	for (i = 0; i < ARRAY_SIZE(crm_regs_handlers); ++i) {
		if (of_sysbus_access_in_reg(ofdev, i, addr, size)) {
			offset = addr - ofdev->regs[i].addr;
			break;
		}
	}

	assert(offset != -1);

    // now, search for the offset in the reg table
    size_t j;
    for (j = 0; j < crm_regs_handlers[i].reg_array_size; ++j) {
        if (crm_regs_handlers[i].reg_array[j] == offset) {
            break;
        }
    }

    assert(j != crm_regs_handlers[i].reg_array_size);

	return crm_regs_handlers[i].read_handler(cs, crm_regs_handlers[i].reg_array, j);
}

static void qcom_crm_write(void *opaque, hwaddr addr,
                              uint64_t value, unsigned int size)
{
    CRM_V2_LOG(opaque, "[*] crm write detected @addr 0x%lx\n", addr);
}

static void qcom_crm_init(Object* obj)
{
    QcomCrmState* cdev = QCOM_CRM(obj);
    SysBusDevice* sbd = SYS_BUS_DEVICE(obj);

    for (size_t i = 0; i < ARRAY_SIZE(cdev->irq); i++) {
        sysbus_init_irq(sbd, &cdev->irq[i]);
    }
}

static const MemoryRegionOps qcom_crm_ops = {
    .read = qcom_crm_read,
    .write = qcom_crm_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .impl = {
        .min_access_size = 1,
        .max_access_size = 8,
    },
};

static void qcom_crm_realize(OfSysBusDevice* ofdev, Error **errp)
{
    QcomCrmState *s = QCOM_CRM(ofdev);
    SysBusDevice* sbd = SYS_BUS_DEVICE(ofdev);

    const struct crm_desc* desc = ofdev->data;

	// according to the driver
	assert(ofdev->nb_regs == 6);

    crm_regs_handlers[CRM_DRV_BASE].reg_array = desc->chn_regs;
    crm_regs_handlers[CRM_DRV_BASE].reg_array_size = ARRAY_SIZE(desc->chn_regs);

    crm_regs_handlers[CRM_CRMB_MGR].reg_array = desc->crmb_regs;
    crm_regs_handlers[CRM_CRMB_MGR].reg_array_size = ARRAY_SIZE(desc->crmb_regs);

    crm_regs_handlers[CRM_CRMB_PT_MGR].reg_array = desc->crmb_pt_regs;
    crm_regs_handlers[CRM_CRMB_PT_MGR].reg_array_size = ARRAY_SIZE(desc->crmb_pt_regs);

    crm_regs_handlers[CRM_CRMC_MGR].reg_array = desc->crmc_regs;
    crm_regs_handlers[CRM_CRMC_MGR].reg_array_size = ARRAY_SIZE(desc->crmc_regs);

    crm_regs_handlers[CRM_CRMV_MGR].reg_array = desc->crmv_regs;
    crm_regs_handlers[CRM_CRMV_MGR].reg_array_size = ARRAY_SIZE(desc->crmv_regs);

    crm_regs_handlers[CRM_COMMON].reg_array = desc->cfg_regs;
    crm_regs_handlers[CRM_COMMON].reg_array_size = ARRAY_SIZE(desc->cfg_regs);

	assert(ofdev->mem_size);
    memory_region_init_io(&s->iomem, OBJECT(ofdev), &qcom_crm_ops, s, TYPE_QCOM_CRM, ofdev->mem_size);
    sysbus_init_mmio(sbd, &s->iomem);
}

static const struct of_device_id crm_drv_match[] = {
	{ .compatible = "qcom,cam-crm-v3", .data = &cam_crm_desc_v3},
	{ .compatible = "qcom,pcie-crm-v3", .data = &pcie_crm_desc_v3},
	{ .compatible = "qcom,disp-crm-v3", .data = &disp_crm_desc_v3},
	{ }
};

static void qcom_crm_class_init(ObjectClass* oc, void* data)
{
	OfSysBusDeviceClass* kofdev = OF_SYS_BUS_DEVICE_CLASS(oc);

    kofdev->realize = qcom_crm_realize;

	kofdev->of_match_table = crm_drv_match;
}

static const TypeInfo qcom_crm_info = {
    .name = TYPE_QCOM_CRM,
    .parent = TYPE_OF_SYS_BUS_DEVICE,
    .instance_size = sizeof(QcomCrmState),
    .instance_init = qcom_crm_init,
    .class_init = qcom_crm_class_init,
};

static void qcom_crm_register_types(void)
{
    type_register_static(&qcom_crm_info);
}

type_init(qcom_crm_register_types);
