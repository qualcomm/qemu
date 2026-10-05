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
#define TURING_RSC_REGISTER_SPACE_SIZE 0x10000
#define TURING_RSC_MAX_DRIVERS         1   /* Single driver (DRV0) */
#define TURING_RSC_MAX_TCS_PER_DRV     10  /* 10 TCS per driver (max) */
#define TURING_RSC_MAX_CMDS_PER_TCS    16  /* 16 commands per TCS */

/* Register field definitions */
/* PARENTCHILD_CONFIG fields */
#define TURING_PARENTCHILD_CONFIG_NUM_TCS_DRV0_SHIFT        0

/* TCS Control bits */
#define TURING_TCS_AMC_MODE_TRIGGER     BIT(24)

/* TCS Status bits */
#define TURING_TCS_CONTROLLER_IDLE      BIT(0)

#define TURING_TCS_SPACING                      0x2A0

/*
 * Command structure (word 3 "status" is never stored; reads return a
 * fixed value)
 */
typedef struct {
    uint32_t words[3];
    uint32_t read_response_data;
} TuringRscCommand;

/* TCS (Trigger Command Set) state */
typedef struct {
    uint32_t words[4];
    bool triggered;
    TuringRscCommand commands[TURING_RSC_MAX_CMDS_PER_TCS];
} TuringRscTcsState;

/* Driver state */
typedef struct {
    bool present;

    /* Plain-storage registers at byte offsets 0x00-0x48 (indexed by /4) */
    uint32_t low[19];

    /* Error IRQ registers */
    uint32_t error_irq_status;
    uint32_t error_irq_enable;

    /* Control registers */
    uint32_t secure_override;

    /* Sequencer registers */
    uint32_t seq_override_start_addr;
    uint32_t seq_busy;
    uint32_t seq_program_counter;
    uint32_t seq_comp;
    uint32_t seq_override_trigger;
    uint32_t seq_override_trigger_start_address;

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
};

#endif /* HW_MISC_QCOM_TURING_RSC_H */
