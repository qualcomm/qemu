/*
 * Qualcomm Turing RSC (Resource State Coordinator)
 * TURINGNSP_0_TURING_RSCC_RSCC_RSC
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef HW_MISC_QCOM_TURING_RSC_H
#define HW_MISC_QCOM_TURING_RSC_H

#include "hw/core/sysbus.h"
#include "hw/core/resettable.h"
#include "hw/core/irq.h"
#include "qom/object.h"

#define TYPE_TURING_RSC "turing-rsc"
OBJECT_DECLARE_TYPE(TuringRscState, TuringRscClass, TURING_RSC)

/* Hardware limits based on IP catalog (max across all targets) */
#define TURING_RSC_REGISTER_SPACE_SIZE 0x10000  /* 64KB total */
#define TURING_RSC_MAX_DRIVERS         1   /* Single driver (DRV0) */
#define TURING_RSC_MAX_TCS_PER_DRV     10  /* 10 TCS per driver (max) */
#define TURING_RSC_MAX_CMDS_PER_TCS    16  /* 16 commands per TCS */
#define TURING_RSC_MAX_SEQ_MEM         48  /* 48 sequencer memory words */
#define TURING_RSC_MAX_TIMESTAMP_UNITS 8   /* 8 timestamp units (max) */
#define TURING_RSC_MAX_HW_EVENT_MUX    32  /* 32 HW event mux */
#define TURING_RSC_MAX_DELAY_VAL       4   /* 4 delay value registers */
#define TURING_RSC_MAX_BR_ADDR         16  /* 16 branch addr regs (max) */

/* Register enumeration based on IP catalog */
enum turing_rsc_regs {
    /* ID and Parameter registers */
    TURING_RSC_ID_DRV0                              = 0x0000,
    TURING_RSC_PARAM_SOLVER_CONFIG_DRV0             = 0x0004,
    TURING_RSC_PARAM_RSC_CONFIG_DRV0                = 0x0008,
    TURING_RSC_PARAM_RSC_PARENTCHILD_CONFIG_DRV0    = 0x000C,

    /* Status registers */
    TURING_RSC_STATUS0_DRV0                         = 0x0010,
    TURING_RSC_STATUS1_DRV0                         = 0x0014,
    TURING_RSC_STATUS2_DRV0                         = 0x0018,

    /* Hidden TCS registers */
    TURING_HIDDEN_TCS_CTRL_DRV0                     = 0x001C,
    TURING_PDC_SEQ_START_ADDR_REG_OFFSET_DRV0       = 0x0020,
    TURING_PDC_MATCH_VALUE_LO_REG_OFFSET_DRV0       = 0x0024,
    TURING_PDC_MATCH_VALUE_HI_REG_OFFSET_DRV0       = 0x0028,
    TURING_PDC_SLAVE_ID_DRV0                        = 0x002C,
    TURING_HIDDEN_TCS_STATUS_DRV0                   = 0x0030,
    TURING_HIDDEN_TCS_CMD0_ADDR_DRV0                = 0x0034,
    TURING_HIDDEN_TCS_CMD0_DATA_DRV0                = 0x0038,
    TURING_HIDDEN_TCS_CMD1_ADDR_DRV0                = 0x003C,
    TURING_HIDDEN_TCS_CMD1_DATA_DRV0                = 0x0040,
    TURING_HIDDEN_TCS_CMD2_ADDR_DRV0                = 0x0044,
    TURING_HIDDEN_TCS_CMD2_DATA_DRV0                = 0x0048,

    /* HW Event registers */
    TURING_HW_EVENT_OWNER_DRV0                      = 0x004C,
    TURING_HW_EVENT_MUX0_SELECT_DRV0                = 0x0050,

    /* Error IRQ registers */
    TURING_RSC_ERROR_IRQ_STATUS_DRV0                = 0x00D0,
    TURING_RSC_ERROR_IRQ_CLEAR_DRV0                 = 0x00D4,
    TURING_RSC_ERROR_IRQ_ENABLE_DRV0                = 0x00D8,

    /* Control registers */
    TURING_RSC_ERROR_RESP_CTRL_DRV0                 = 0x0100,
    TURING_RSC_SECURE_OVERRIDE_DRV0                 = 0x0104,
    TURING_RSC_RIF_CLK_GATING_OVERRIDE_DRV0         = 0x0108,

    /* Timestamp registers */
    TURING_RSC_TIMESTAMP_UNIT_OWNER_DRV0            = 0x0200,
    TURING_RSC_TIMESTAMP_UNIT0_EN_DRV0              = 0x0204,
    TURING_RSC_TIMESTAMP_UNIT0_TIMESTAMP_L_DRV0     = 0x0208,
    TURING_RSC_TIMESTAMP_UNIT0_TIMESTAMP_H_DRV0     = 0x020C,
    TURING_RSC_TIMESTAMP_UNIT0_OUTPUT_DRV0          = 0x0210,

    /* Sequencer registers */
    TURING_RSC_SEQ_OVERRIDE_START_ADDR_DRV0         = 0x0400,
    TURING_RSC_SEQ_BUSY_DRV0                        = 0x0404,
    TURING_RSC_SEQ_PROGRAM_COUNTER_DRV0             = 0x0408,
    TURING_RSC_SEQ_COMP_DRV0                        = 0x0410,
    TURING_RSC_SEQ_CFG_DELAY_VAL_0_DRV0             = 0x0450,
    TURING_RSC_SEQ_OVERRIDE_TRIGGER_DRV0            = 0x0460,
    TURING_RSC_SEQ_OVERRIDE_TRIGGER_START_ADDRESS_DRV0 = 0x0464,
    TURING_RSC_SEQ_DBG_BREAKPOINT_ADDR_DRV0         = 0x0490,
    TURING_RSC_SEQ_DBG_STEP_DRV0                    = 0x0494,
    TURING_RSC_SEQ_DBG_CONTINUE_DRV0                = 0x0498,
    TURING_RSC_SEQ_DBG_STAT_DRV0                    = 0x049C,
    TURING_RSC_SEQ_OVERRIDE_PWR_CNTRL_MASK_DRV0     = 0x04A0,
    TURING_RSC_SEQ_OVERRIDE_PWR_CNTRL_VAL_DRV0      = 0x04A4,
    TURING_RSC_SEQ_OVERRIDE_WAIT_EVENT_MASK_DRV0    = 0x04A8,
    TURING_RSC_SEQ_OVERRIDE_WAIT_EVENT_VAL_DRV0     = 0x04AC,
    TURING_RSC_SEQ_PWR_CTRL_STATUS_DRV0             = 0x04B0,
    TURING_RSC_SEQ_PWR_EVENT_STATUS_DRV0            = 0x04B4,
    TURING_RSC_SEQ_BR_EVENT_STATUS_DRV0             = 0x04B8,
    TURING_RSC_SEQ_CFG_BR_ADDR_0_DRV0               = 0x0500,
    TURING_SEQ_MEM_0_DRV0                           = 0x0600,

    /* TCS AMC Mode registers */
    TURING_TCS_AMC_MODE_IRQ_ENABLE_DRV0             = 0x0D00,
    TURING_TCS_AMC_MODE_IRQ_STATUS_DRV0             = 0x0D04,
    TURING_TCS_AMC_MODE_IRQ_CLEAR_DRV0              = 0x0D08,

    /* TCS registers (TCS0 base) */
    TURING_TCS0_DRV0_CMD_WAIT_FOR_CMPL              = 0x0D10,
    TURING_TCS0_DRV0_CONTROL                        = 0x0D14,
    TURING_TCS0_DRV0_STATUS                         = 0x0D18,
    TURING_TCS0_DRV0_CMD_ENABLE                     = 0x0D1C,

    /* TCS Command registers (CMD0 base) */
    TURING_TCS0_CMD0_DRV0_MSGID                     = 0x0D30,
    TURING_TCS0_CMD0_DRV0_ADDR                      = 0x0D34,
    TURING_TCS0_CMD0_DRV0_DATA                      = 0x0D38,
    TURING_TCS0_CMD0_DRV0_STATUS                    = 0x0D3C,
    TURING_TCS0_CMD0_DRV0_READ_RESPONSE_DATA        = 0x0D40,

    /* Timeout registers */
    TURING_TCS_TIMEOUT_EN_DRV0                      = 0x3D44,
    TURING_TCS_TIMEOUT_CLR_DRV0                     = 0x3D48,
    TURING_TCS_TIMEOUT_STATUS_DRV0                  = 0x3D4C,
    TURING_TCS_TIMEOUT_VAL_DRV0                     = 0x3D50,
};

/* Register field definitions */
/* RSC_ID fields */
#define TURING_RSC_ID_MAJOR_VER_MASK    0xFF0000
#define TURING_RSC_ID_MAJOR_VER_SHIFT   16
#define TURING_RSC_ID_MINOR_VER_MASK    0xFF00
#define TURING_RSC_ID_MINOR_VER_SHIFT   8
#define TURING_RSC_ID_STEP_VER_MASK     0xFF
#define TURING_RSC_ID_STEP_VER_SHIFT    0

/* SOLVER_CONFIG fields */
#define TURING_SOLVER_CONFIG_SEQ_EXTENDED_MASK      BIT(26)
#define TURING_SOLVER_CONFIG_IS_CHILD_RSC_MASK      BIT(25)
#define TURING_SOLVER_CONFIG_HW_SOLVER_MASK         BIT(24)
#define TURING_SOLVER_CONFIG_SOLVER_SLOTS_MASK      0x1F0000
#define TURING_SOLVER_CONFIG_SOLVER_SLOTS_SHIFT     16
#define TURING_SOLVER_CONFIG_SOLVER_MODES_MASK      0x1F00
#define TURING_SOLVER_CONFIG_SOLVER_MODES_SHIFT     8
#define TURING_SOLVER_CONFIG_NUM_TIMERS_MASK        0x1F
#define TURING_SOLVER_CONFIG_NUM_TIMERS_SHIFT       0

/* RSC_CONFIG fields */
#define TURING_RSC_CONFIG_NUM_DRV_MASK              0x7000000
#define TURING_RSC_CONFIG_NUM_DRV_SHIFT             24
#define TURING_RSC_CONFIG_NUM_SEQ_CMD_WORDS_MASK    0xFF0000
#define TURING_RSC_CONFIG_NUM_SEQ_CMD_WORDS_SHIFT   16
#define TURING_RSC_CONFIG_NUM_TS_EVENTS_MASK        0xF00
#define TURING_RSC_CONFIG_NUM_TS_EVENTS_SHIFT       8
#define TURING_RSC_CONFIG_DELAY_CNTR_BITWIDTH_MASK  0x1F
#define TURING_RSC_CONFIG_DELAY_CNTR_BITWIDTH_SHIFT 0

/* PARENTCHILD_CONFIG fields */
#define TURING_PARENTCHILD_CONFIG_NUM_CMDS_PER_TCS_MASK     0xF8000000
#define TURING_PARENTCHILD_CONFIG_NUM_CMDS_PER_TCS_SHIFT    27
#define TURING_PARENTCHILD_CONFIG_NUM_TCS_DRV0_MASK         0x3F
#define TURING_PARENTCHILD_CONFIG_NUM_TCS_DRV0_SHIFT        0

/* TCS Control bits */
#define TURING_TCS_AMC_MODE_EN          BIT(16)
#define TURING_TCS_AMC_MODE_TRIGGER     BIT(24)

/* TCS Status bits */
#define TURING_TCS_CONTROLLER_IDLE      BIT(0)

/* Command MSGID bits */
#define TURING_CMD_MSGID_MSG_LENGTH_MASK        0xF
#define TURING_CMD_MSGID_MSG_LENGTH_SHIFT       0
#define TURING_CMD_MSGID_RES_REQ                BIT(8)
#define TURING_CMD_MSGID_READ_OR_WRITE          BIT(16)

/* Command ADDR bits */
#define TURING_CMD_ADDR_OFFSET_MASK             0xFFFF
#define TURING_CMD_ADDR_OFFSET_SHIFT            0
#define TURING_CMD_ADDR_SLV_ID_MASK             0x70000
#define TURING_CMD_ADDR_SLV_ID_SHIFT            16

/* Command STATUS bits */
#define TURING_CMD_STATUS_TRIGGERED             BIT(0)
#define TURING_CMD_STATUS_ISSUED                BIT(8)
#define TURING_CMD_STATUS_COMPLETED             BIT(16)

#define TURING_TCS_SPACING                      0x2A0

/* Timestamp unit structure */
typedef struct {
    uint32_t enable;
    uint32_t timestamp_l;
    uint32_t timestamp_h;
    uint32_t output;
} TuringRscTimestampUnit;

/* Hidden TCS command structure */
typedef struct {
    uint32_t addr;
    uint32_t data;
} TuringRscHiddenTcsCommand;

/* Command structure */
typedef struct {
    uint32_t msgid;
    uint32_t addr;
    uint32_t data;
    uint32_t status;
    uint32_t read_response_data;
} TuringRscCommand;

/* TCS (Trigger Command Set) state */
typedef struct {
    uint32_t cmd_wait_for_cmpl;
    uint32_t control;
    uint32_t status;
    uint32_t cmd_enable;
    bool triggered;
    TuringRscCommand commands[TURING_RSC_MAX_CMDS_PER_TCS];
} TuringRscTcsState;

/* Driver state */
typedef struct {
    bool present;
    uint32_t driver_id;

    /* ID and Parameter registers */
    uint32_t rsc_id;
    uint32_t solver_config;
    uint32_t rsc_config;
    uint32_t parentchild_config;

    /* Status registers */
    uint32_t status0;
    uint32_t status1;
    uint32_t status2;

    /* Hidden TCS registers */
    uint32_t hidden_tcs_ctrl;
    uint32_t pdc_seq_start_addr_reg_offset;
    uint32_t pdc_match_value_lo_reg_offset;
    uint32_t pdc_match_value_hi_reg_offset;
    uint32_t pdc_slave_id;
    uint32_t hidden_tcs_status;
    TuringRscHiddenTcsCommand hidden_tcs_cmds[3];

    /* HW Event registers */
    uint32_t hw_event_owner;
    uint32_t hw_event_mux_select[TURING_RSC_MAX_HW_EVENT_MUX];

    /* Error IRQ registers */
    uint32_t error_irq_status;
    uint32_t error_irq_enable;

    /* Control registers */
    uint32_t error_resp_ctrl;
    uint32_t secure_override;
    uint32_t rif_clk_gating_override;

    /* Timestamp registers */
    uint32_t timestamp_unit_owner;
    TuringRscTimestampUnit timestamp_units[TURING_RSC_MAX_TIMESTAMP_UNITS];

    /* Sequencer registers */
    uint32_t seq_override_start_addr;
    uint32_t seq_busy;
    uint32_t seq_program_counter;
    uint32_t seq_comp;
    uint32_t seq_cfg_delay_val[TURING_RSC_MAX_DELAY_VAL];
    uint32_t seq_override_trigger;
    uint32_t seq_override_trigger_start_address;
    uint32_t seq_dbg_breakpoint_addr;
    uint32_t seq_dbg_step;
    uint32_t seq_dbg_continue;
    uint32_t seq_dbg_stat;
    uint32_t seq_override_pwr_cntrl_mask;
    uint32_t seq_override_pwr_cntrl_val;
    uint32_t seq_override_wait_event_mask;
    uint32_t seq_override_wait_event_val;
    uint32_t seq_pwr_ctrl_status;
    uint32_t seq_pwr_event_status;
    uint32_t seq_br_event_status;
    uint32_t seq_cfg_br_addr[TURING_RSC_MAX_BR_ADDR];
    uint32_t seq_mem[TURING_RSC_MAX_SEQ_MEM];

    /* TCS AMC Mode registers */
    uint32_t tcs_amc_mode_irq_enable;
    uint32_t tcs_amc_mode_irq_status;

    /* Timeout registers */
    uint32_t tcs_timeout_en;
    uint32_t tcs_timeout_status;
    uint32_t tcs_timeout_val;

    /* Hardware configuration */
    uint32_t num_tcs;

    /* TCS states */
    TuringRscTcsState tcs_states[TURING_RSC_MAX_TCS_PER_DRV];

    /* IRQ lines */
    qemu_irq error_irq;
    qemu_irq amc_irq;
} TuringRscDriverState;

struct TuringRscClass {
    SysBusDeviceClass parent_class;

    /* Reset phases */
    ResettablePhases parent_phases;
};

struct TuringRscState {
    SysBusDevice parent_obj;

    /* Memory region */
    MemoryRegion iomem;

    /* Device configuration */
    uint32_t version;

    /* Driver (single DRV0 only) */
    TuringRscDriverState drivers[TURING_RSC_MAX_DRIVERS];

    /* Configurable parameters (set via properties per target) */
    uint32_t rsc_id_reset;
    uint32_t solver_config_reset;
    uint32_t rsc_config_reset;
    uint32_t parentchild_config_reset;
    uint32_t cmd_spacing;
    uint32_t tcs_base_offset;
    uint32_t cmd_base_in_tcs;
    uint32_t tcs_timeout_base;
    uint32_t timeout_clr_offset;
    uint32_t timeout_status_offset;
    uint32_t timeout_val_offset;
    uint32_t num_br_addr;
    uint32_t num_timestamp_units;
};

#endif /* HW_MISC_QCOM_TURING_RSC_H */
