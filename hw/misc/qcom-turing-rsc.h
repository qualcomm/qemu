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

    /* Error IRQ registers */
    TURING_RSC_ERROR_IRQ_STATUS_DRV0                = 0x00D0,
    TURING_RSC_ERROR_IRQ_CLEAR_DRV0                 = 0x00D4,
    TURING_RSC_ERROR_IRQ_ENABLE_DRV0                = 0x00D8,

    /* Control registers */
    TURING_RSC_SECURE_OVERRIDE_DRV0                 = 0x0104,

    /* Sequencer registers */
    TURING_RSC_SEQ_OVERRIDE_START_ADDR_DRV0         = 0x0400,
    TURING_RSC_SEQ_BUSY_DRV0                        = 0x0404,
    TURING_RSC_SEQ_PROGRAM_COUNTER_DRV0             = 0x0408,
    TURING_RSC_SEQ_COMP_DRV0                        = 0x0410,
    TURING_RSC_SEQ_OVERRIDE_TRIGGER_DRV0            = 0x0460,
    TURING_RSC_SEQ_OVERRIDE_TRIGGER_START_ADDRESS_DRV0 = 0x0464,

    /* TCS AMC Mode registers */
    TURING_TCS_AMC_MODE_IRQ_ENABLE_DRV0             = 0x0D00,
    TURING_TCS_AMC_MODE_IRQ_STATUS_DRV0             = 0x0D04,
    TURING_TCS_AMC_MODE_IRQ_CLEAR_DRV0              = 0x0D08,
};

/* Register field definitions */
/* PARENTCHILD_CONFIG fields */
#define TURING_PARENTCHILD_CONFIG_NUM_TCS_DRV0_SHIFT        0

/* TCS Control bits */
#define TURING_TCS_AMC_MODE_TRIGGER     BIT(24)

/* TCS Status bits */
#define TURING_TCS_CONTROLLER_IDLE      BIT(0)

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
