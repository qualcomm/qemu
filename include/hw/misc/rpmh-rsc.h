/*
 * Qualcomm RPMH-RSC (Resource Power Manager Hardware - Resource State
 * Coordinator)
 *
 * Copyright (c) 2024 Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef HW_MISC_RPMH_RSC_H
#define HW_MISC_RPMH_RSC_H

#include "hw/core/sysbus.h"
#include "hw/core/resettable.h"
#include "qom/object.h"

#define TYPE_RPMH_RSC "rpmh-rsc"
OBJECT_DECLARE_TYPE(RpmhRscState, RpmhRscClass, RPMH_RSC)

/* Hardware limits */
#define RPMH_RSC_REGISTER_SPACE_SIZE 0x20000  /* 128KB total */
#define RPMH_RSC_MAX_DRIVERS         4        /* Maximum drivers per RSC */
#define RPMH_RSC_MAX_TCS_PER_DRV     32       /* Maximum TCS per driver */
#define RPMH_RSC_MAX_CMDS_PER_TCS    16       /* Maximum commands per TCS */

/* Register enumeration */
enum rpmh_rsc_regs {
    /* Offset configuration */
    RSC_DRV_TCS_OFFSET,
    RSC_DRV_CMD_OFFSET,

    /* Driver-level registers */
    DRV_ID,
    DRV_SOLVER_CONFIG,
    DRV_PRNT_CHLD_CONFIG,

    /* TCS common registers */
    RSC_DRV_IRQ_ENABLE,
    RSC_DRV_IRQ_STATUS,
    RSC_DRV_IRQ_CLEAR,

    /* TCS specific registers */
    RSC_DRV_CMD_WAIT_FOR_CMPL,
    RSC_DRV_CONTROL,
    RSC_DRV_STATUS,
    RSC_DRV_CMD_ENABLE,

    /* Command registers */
    RSC_DRV_CMD_MSGID,
    RSC_DRV_CMD_ADDR,
    RSC_DRV_CMD_DATA,
    RSC_DRV_CMD_STATUS,
    RSC_DRV_CMD_RESP_DATA,

    RPMH_RSC_MAX_REGS
};

/* Version info */
#define MAJOR_VER_MASK     0xFF
#define MAJOR_VER_SHIFT    16
#define MINOR_VER_MASK     0xFF
#define MINOR_VER_SHIFT    8

/* DRV configuration bits */
#define DRV_HW_SOLVER_MASK   1
#define DRV_HW_SOLVER_SHIFT  24
#define DRV_NUM_TCS_MASK     0x3F
#define DRV_NUM_TCS_SHIFT    6
#define DRV_NCPT_MASK        0x1F
#define DRV_NCPT_SHIFT       27

/* TCS control bits */
#define TCS_AMC_MODE_ENABLE   BIT(16)
#define TCS_AMC_MODE_TRIGGER  BIT(24)

/* Command register bits */
#define CMD_MSGID_LEN         8
#define CMD_MSGID_RESP_REQ    BIT(8)
#define CMD_MSGID_WRITE       BIT(16)
#define CMD_STATUS_ISSUED     BIT(8)
#define CMD_STATUS_COMPL      BIT(16)

/* Command structure */
typedef struct {
    uint32_t addr;
    uint32_t data;
    uint32_t wait;
    uint32_t status;
} RpmhRscCommand;

/* TCS (Trigger Command Set) state */
typedef struct {
    uint32_t control;
    uint32_t status;
    uint32_t cmd_enable;
    bool triggered;
    RpmhRscCommand commands[RPMH_RSC_MAX_CMDS_PER_TCS];
} RpmhRscTcsState;

/* Driver state */
typedef struct {
    bool present;
    uint32_t driver_id;

    /* Register state */
    uint32_t drv_id;
    uint32_t solver_config;
    uint32_t prnt_chld_config;
    uint32_t irq_enable;
    uint32_t irq_status;

    /* Hardware configuration */
    uint32_t num_tcs;
    uint32_t cmds_per_tcs;
    uint32_t tcs_offset;

    /* TCS states */
    RpmhRscTcsState tcs_states[RPMH_RSC_MAX_TCS_PER_DRV];

    /* IRQ line */
    qemu_irq irq;
} RpmhRscDriverState;

struct RpmhRscClass {
    SysBusDeviceClass parent_class;

    /* Reset phases */
    ResettablePhases parent_phases;
};

struct RpmhRscState {
    SysBusDevice parent_obj;

    /* Memory regions - separate for clarity */
    MemoryRegion drv_iomem;  /* Driver registers */
    MemoryRegion tcs_iomem;  /* TCS and command registers */

    /* Device configuration */
    uint32_t version;
    uint32_t regs[RPMH_RSC_MAX_REGS];  /* Register offset table */

    /* Driver management */
    uint32_t num_drivers;
    RpmhRscDriverState drivers[RPMH_RSC_MAX_DRIVERS];

    /* Properties */
    uint32_t tcs_base;
};

#endif /* HW_MISC_RPMH_RSC_H */

