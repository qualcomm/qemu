/*
 * Qualcomm RPMH-RSC (Resource Power Manager Hardware - Resource State
 * Coordinator)
 *
 * Copyright (c) 2024 Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "hw/core/irq.h"
#include "hw/misc/rpmh-rsc.h"
#include "hw/core/qdev-properties.h"
#include "hw/core/resettable.h"
#include "hw/core/sysbus.h"
#include "migration/vmstate.h"
#include "qemu/log.h"
#include "qemu/module.h"
#include "qapi/error.h"
#include "trace.h"

/* Register offset tables for different hardware versions */
static const uint32_t rpmh_rsc_reg_offset_ver_2_7[RPMH_RSC_MAX_REGS] = {
    [RSC_DRV_TCS_OFFSET]        = 672,
    [RSC_DRV_CMD_OFFSET]        = 20,
    /* DRV space registers */
    [DRV_ID]                    = 0x00,
    [DRV_SOLVER_CONFIG]         = 0x04,
    [DRV_PRNT_CHLD_CONFIG]      = 0x0C,
    [RSC_DRV_IRQ_ENABLE]        = 0x20,
    [RSC_DRV_IRQ_CLEAR]         = 0x24,
    /* TCS space registers */
    [RSC_DRV_IRQ_STATUS]        = 0x04,
    [RSC_DRV_CMD_WAIT_FOR_CMPL] = 0x10,
    [RSC_DRV_CONTROL]           = 0x14,
    [RSC_DRV_STATUS]            = 0x18,
    [RSC_DRV_CMD_ENABLE]        = 0x1C,
    [RSC_DRV_CMD_MSGID]         = 0x30,
    [RSC_DRV_CMD_ADDR]          = 0x34,
    [RSC_DRV_CMD_DATA]          = 0x38,
    [RSC_DRV_CMD_STATUS]        = 0x3C,
    [RSC_DRV_CMD_RESP_DATA]     = 0x40,
};


/* Default reset values for different RSC types */
static const uint32_t rpmh_rsc_apps_reset_values[RPMH_RSC_MAX_REGS] = {
    [DRV_ID]                    = 0x00040300,
    [DRV_SOLVER_CONFIG]         = 0x04010100,
    [DRV_PRNT_CHLD_CONFIG]      = 0x800C8104,
    [RSC_DRV_IRQ_ENABLE]        = 0x00000000,
    [RSC_DRV_IRQ_STATUS]        = 0x00000000,
    [RSC_DRV_STATUS]            = 0x00000001,
};


/* Helper functions for driver and TCS access */
static RpmhRscDriverState *get_driver_by_offset(RpmhRscState *s, hwaddr offset,
                                                hwaddr *driver_offset)
{
    /* For simplified implementation, assume single driver at base */
    if (s->num_drivers > 0 && s->drivers[0].present) {
        *driver_offset = offset;
        return &s->drivers[0];
    }
    return NULL;
}

static RpmhRscTcsState *get_tcs_by_offset(RpmhRscDriverState *drv,
                                          hwaddr offset,
                                          hwaddr *tcs_offset)
{
    uint32_t tcs_id = offset / 672; /* Default TCS spacing */
    if (tcs_id < drv->num_tcs) {
        *tcs_offset = offset - (tcs_id * 672);
        return &drv->tcs_states[tcs_id];
    }
    return NULL;
}

static RpmhRscCommand *get_command_by_offset(RpmhRscTcsState *tcs,
                                            hwaddr offset,
                                            uint32_t cmd_offset_size,
                                            hwaddr *cmd_offset)
{
    hwaddr cmd_base_offset = 0x30; /* CMD_MSGID offset */
    if (offset < cmd_base_offset) {
        return NULL;
    }

    hwaddr cmd_area_offset = offset - cmd_base_offset;
    uint32_t cmd_id = cmd_area_offset / cmd_offset_size;

    if (cmd_id < RPMH_RSC_MAX_CMDS_PER_TCS) {
        *cmd_offset = cmd_area_offset - (cmd_id * cmd_offset_size);
        return &tcs->commands[cmd_id];
    }
    return NULL;
}

/* DRV register space read handler */
static uint64_t rpmh_rsc_drv_read(void *opaque, hwaddr addr, unsigned size)
{
    RpmhRscState *s = RPMH_RSC(opaque);
    hwaddr driver_offset;
    RpmhRscDriverState *drv = get_driver_by_offset(s, addr, &driver_offset);

    if (!drv) {
        qemu_log_mask(LOG_UNIMP,
                      "RPMH-RSC DRV: No driver for offset 0x%" HWADDR_PRIx ", "
                      "num_drivers=%d\n",
                      addr, s->num_drivers);
        return 0;
    }

    qemu_log_mask(LOG_UNIMP,
                  "RPMH-RSC DRV: Found driver %d for offset 0x%" HWADDR_PRIx
                  ", driver_offset=0x%" HWADDR_PRIx "\n",
                  drv->driver_id, addr, driver_offset);

    uint32_t offset = driver_offset;
    uint64_t value = 0;

    if (offset == s->regs[DRV_ID]) {
        value = drv->drv_id;
    } else if (offset == s->regs[DRV_SOLVER_CONFIG]) {
        value = drv->solver_config;
    } else if (offset == s->regs[DRV_PRNT_CHLD_CONFIG]) {
        value = drv->prnt_chld_config;
        qemu_log_mask(LOG_UNIMP,
                      "RPMH-RSC DRV: DRV_PRNT_CHLD_CONFIG read: 0x%08x\n",
                      (uint32_t)value);
    } else if (offset == s->regs[RSC_DRV_IRQ_ENABLE]) {
        value = drv->irq_enable;
    } else {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "RPMH-RSC DRV: Invalid register offset 0x%x\n", offset);
    }

    trace_rpmh_rsc_drv_read((uint32_t)value, offset);
    return value;
}

/* TCS register space read handler */
static uint64_t rpmh_rsc_tcs_read(void *opaque, hwaddr addr, unsigned size)
{
    RpmhRscState *s = RPMH_RSC(opaque);
    hwaddr driver_offset;
    RpmhRscDriverState *drv = get_driver_by_offset(s, addr, &driver_offset);

    if (!drv) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "RPMH-RSC TCS: No driver for offset 0x%" HWADDR_PRIx "\n",
                      addr);
        return 0;
    }

    uint32_t offset = driver_offset;
    uint64_t value = 0;

    /* Handle TCS-level IRQ status first */
    if (offset == s->regs[RSC_DRV_IRQ_STATUS]) {
        value = drv->irq_status;
        trace_rpmh_rsc_irq_status_read((uint32_t)value);
        return value;
    }

    hwaddr tcs_offset;
    RpmhRscTcsState *tcs = get_tcs_by_offset(drv, offset, &tcs_offset);

    if (!tcs) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "RPMH-RSC TCS: Invalid TCS offset 0x%x\n", offset);
        return 0;
    }

    if (tcs_offset == s->regs[RSC_DRV_CONTROL]) {
        value = tcs->control;
    } else if (tcs_offset == s->regs[RSC_DRV_STATUS]) {
        value = tcs->status;
    } else if (tcs_offset == s->regs[RSC_DRV_CMD_ENABLE]) {
        value = tcs->cmd_enable;
    } else {
        /* Check for command register access */
        hwaddr cmd_offset;
        RpmhRscCommand *cmd = get_command_by_offset(tcs, tcs_offset,
                                                   s->regs[RSC_DRV_CMD_OFFSET],
                                                   &cmd_offset);
        if (cmd) {
            if (cmd_offset == 0) { /* CMD_MSGID */
                value = (cmd->wait ? CMD_MSGID_RESP_REQ : 0) |
                       (cmd->data ? CMD_MSGID_WRITE : 0);
            } else if (cmd_offset == 4) { /* CMD_ADDR */
                value = cmd->addr;
            } else if (cmd_offset == 8) { /* CMD_DATA */
                value = cmd->data;
            } else if (cmd_offset == 12) { /* CMD_STATUS */
                value = cmd->status;
            }
        } else {
            qemu_log_mask(LOG_GUEST_ERROR,
                          "RPMH-RSC TCS: Invalid register "
                          "offset 0x%" HWADDR_PRIx "\n", tcs_offset);
        }
    }

    trace_rpmh_rsc_tcs_read((uint32_t)value, offset);
    return value;
}

/* DRV register space write handler */
static void rpmh_rsc_drv_write(void *opaque, hwaddr addr, uint64_t value,
                              unsigned size)
{
    RpmhRscState *s = RPMH_RSC(opaque);
    hwaddr driver_offset;
    RpmhRscDriverState *drv = get_driver_by_offset(s, addr, &driver_offset);

    if (!drv) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "RPMH-RSC DRV: No driver for offset 0x%" HWADDR_PRIx "\n",
                      addr);
        return;
    }

    uint32_t offset = driver_offset;
    uint32_t val = (uint32_t)value;

    if (offset == s->regs[RSC_DRV_IRQ_ENABLE]) {
        drv->irq_enable = val;
        trace_rpmh_rsc_irq_enable_write(val);
    } else if (offset == s->regs[RSC_DRV_IRQ_CLEAR]) {
        drv->irq_status &= ~val;
        if (drv->irq_status == 0) {
            qemu_set_irq(drv->irq, 0);
        }
        trace_rpmh_rsc_irq_clear_write(val);
    } else {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "RPMH-RSC DRV: Write to read-only/invalid "
                      "register 0x%x\n", offset);
    }
}

/* TCS register space write handler */
static void rpmh_rsc_tcs_write(void *opaque, hwaddr addr, uint64_t value,
                              unsigned size)
{
    RpmhRscState *s = RPMH_RSC(opaque);
    hwaddr driver_offset;
    RpmhRscDriverState *drv = get_driver_by_offset(s, addr, &driver_offset);

    if (!drv) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "RPMH-RSC TCS: No driver for offset 0x%" HWADDR_PRIx "\n",
                      addr);
        return;
    }

    uint32_t offset = driver_offset;
    uint32_t val = (uint32_t)value;

    hwaddr tcs_offset;
    RpmhRscTcsState *tcs = get_tcs_by_offset(drv, offset, &tcs_offset);

    if (!tcs) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "RPMH-RSC TCS: Invalid TCS offset 0x%x\n", offset);
        return;
    }

    uint32_t tcs_id = offset / 672; /* Calculate TCS ID for IRQ handling */

    if (tcs_offset == s->regs[RSC_DRV_CONTROL]) {
        tcs->control = val;

        /* Handle TCS trigger */
        if (val & TCS_AMC_MODE_TRIGGER) {
            trace_rpmh_rsc_tcs_trigger(tcs_id, tcs->cmd_enable);
            tcs->triggered = true;
            tcs->status = 0; /* Clear busy bit */

            /* Mark all enabled commands as completed */
            for (int i = 0; i < RPMH_RSC_MAX_CMDS_PER_TCS; i++) {
                if (tcs->cmd_enable & BIT(i)) {
                    tcs->commands[i].status = CMD_STATUS_ISSUED |
                                              CMD_STATUS_COMPL;
                }
            }

            /* Set IRQ status and trigger IRQ */
            drv->irq_status |= (1 << tcs_id);
            if (drv->irq_enable & (1 << tcs_id)) {
                qemu_set_irq(drv->irq, 1);
            }
        }
    } else if (tcs_offset == s->regs[RSC_DRV_CMD_ENABLE]) {
        tcs->cmd_enable = val;
    } else {
        /* Check for command register access */
        hwaddr cmd_offset;
        RpmhRscCommand *cmd = get_command_by_offset(tcs, tcs_offset,
                                                   s->regs[RSC_DRV_CMD_OFFSET],
                                                   &cmd_offset);
        if (cmd) {
            if (cmd_offset == 0) { /* CMD_MSGID */
                cmd->wait = !!(val & CMD_MSGID_RESP_REQ);
            } else if (cmd_offset == 4) { /* CMD_ADDR */
                cmd->addr = val;
            } else if (cmd_offset == 8) { /* CMD_DATA */
                cmd->data = val;
            }
        } else {
            qemu_log_mask(LOG_GUEST_ERROR,
                          "RPMH-RSC TCS: Write to invalid register "
                          "offset 0x%" HWADDR_PRIx "\n", tcs_offset);
        }
    }
}

static const MemoryRegionOps rpmh_rsc_drv_ops = {
    .read = rpmh_rsc_drv_read,
    .write = rpmh_rsc_drv_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .impl = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
};

static const MemoryRegionOps rpmh_rsc_tcs_ops = {
    .read = rpmh_rsc_tcs_read,
    .write = rpmh_rsc_tcs_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .impl = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
};

static void rpmh_rsc_reset_enter(Object *obj, ResetType type)
{
    RpmhRscState *s = RPMH_RSC(obj);
    RpmhRscClass *rpmh_class = RPMH_RSC_GET_CLASS(obj);

    /* Call parent reset phase */
    if (rpmh_class->parent_phases.enter) {
        rpmh_class->parent_phases.enter(obj, type);
    }

    /* Reset device state */
    s->version = (2 << MAJOR_VER_SHIFT) | (7 << MINOR_VER_SHIFT);
    s->tcs_base = 0x00000D00; /* Default TCS base */

    /* Reset all drivers */
    for (int i = 0; i < s->num_drivers; i++) {
        RpmhRscDriverState *drv = &s->drivers[i];
        if (drv->present) {
            /* Use APPS reset values by default */
            const uint32_t *reset_vals = rpmh_rsc_apps_reset_values;

            drv->drv_id = reset_vals[DRV_ID];
            drv->solver_config = reset_vals[DRV_SOLVER_CONFIG];
            drv->prnt_chld_config = reset_vals[DRV_PRNT_CHLD_CONFIG];
            drv->irq_enable = 0;
            drv->irq_status = 0;

            /* Extract configuration from parent-child config */
            drv->num_tcs = (drv->prnt_chld_config >> DRV_NUM_TCS_SHIFT) &
                           DRV_NUM_TCS_MASK;
            drv->cmds_per_tcs = (drv->prnt_chld_config >> DRV_NCPT_SHIFT) &
                                DRV_NCPT_MASK;

            /* Reset all TCS states */
            memset(drv->tcs_states, 0, sizeof(drv->tcs_states));
        }
    }
}

static const Property rpmh_rsc_properties[] = {
    DEFINE_PROP_UINT32("tcs-base", RpmhRscState, tcs_base, 0x00000D00),
    DEFINE_PROP_UINT32("num-drivers", RpmhRscState, num_drivers, 1),
};

static void rpmh_rsc_realize(DeviceState *dev, Error **errp)
{
    RpmhRscState *s = RPMH_RSC(dev);
    SysBusDevice *sbd = SYS_BUS_DEVICE(dev);

    /* Use version 2.7 register offsets by default */
    memcpy(s->regs, rpmh_rsc_reg_offset_ver_2_7, sizeof(s->regs));

    /* Clamp number of drivers */
    if (s->num_drivers > RPMH_RSC_MAX_DRIVERS) {
        s->num_drivers = RPMH_RSC_MAX_DRIVERS;
    }

    /* Initialize drivers */
    for (int i = 0; i < s->num_drivers; i++) {
        RpmhRscDriverState *drv = &s->drivers[i];
        drv->present = true;
        drv->driver_id = i;

        /* Initialize driver registers with reset values */
        const uint32_t *reset_vals = rpmh_rsc_apps_reset_values;
        drv->drv_id = reset_vals[DRV_ID];
        drv->solver_config = reset_vals[DRV_SOLVER_CONFIG];
        drv->prnt_chld_config = reset_vals[DRV_PRNT_CHLD_CONFIG];
        drv->irq_enable = 0;
        drv->irq_status = 0;

        /* Extract configuration from parent-child config */
        drv->num_tcs = (drv->prnt_chld_config >> 6) & 0x3F;
        drv->cmds_per_tcs = (drv->prnt_chld_config >> 27) & 0x1F;

        /* Reset all TCS states */
        memset(drv->tcs_states, 0, sizeof(drv->tcs_states));

        /* Initialize IRQ line */
        sysbus_init_irq(sbd, &drv->irq);
    }

    /* Initialize DRV register space (covers base driver registers) */
    memory_region_init_io(&s->drv_iomem, OBJECT(s), &rpmh_rsc_drv_ops, s,
                          TYPE_RPMH_RSC "-drv", s->tcs_base);
    sysbus_init_mmio(sbd, &s->drv_iomem);

    /* Initialize TCS register space (covers TCS and command registers) */
    memory_region_init_io(&s->tcs_iomem, OBJECT(s), &rpmh_rsc_tcs_ops, s,
                          TYPE_RPMH_RSC "-tcs",
                          RPMH_RSC_REGISTER_SPACE_SIZE - s->tcs_base);
    sysbus_init_mmio(sbd, &s->tcs_iomem);
}

static const VMStateDescription vmstate_rpmh_rsc_command = {
    .name = "rpmh-rsc-command",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_UINT32(addr, RpmhRscCommand),
        VMSTATE_UINT32(data, RpmhRscCommand),
        VMSTATE_UINT32(wait, RpmhRscCommand),
        VMSTATE_UINT32(status, RpmhRscCommand),
        VMSTATE_END_OF_LIST()
    }
};

static const VMStateDescription vmstate_rpmh_rsc_tcs = {
    .name = "rpmh-rsc-tcs",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_UINT32(control, RpmhRscTcsState),
        VMSTATE_UINT32(status, RpmhRscTcsState),
        VMSTATE_UINT32(cmd_enable, RpmhRscTcsState),
        VMSTATE_BOOL(triggered, RpmhRscTcsState),
        VMSTATE_STRUCT_ARRAY(commands, RpmhRscTcsState,
                             RPMH_RSC_MAX_CMDS_PER_TCS, 0,
                             vmstate_rpmh_rsc_command, RpmhRscCommand),
        VMSTATE_END_OF_LIST()
    }
};

static const VMStateDescription vmstate_rpmh_rsc_driver = {
    .name = "rpmh-rsc-driver",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_BOOL(present, RpmhRscDriverState),
        VMSTATE_UINT32(driver_id, RpmhRscDriverState),
        VMSTATE_UINT32(drv_id, RpmhRscDriverState),
        VMSTATE_UINT32(solver_config, RpmhRscDriverState),
        VMSTATE_UINT32(prnt_chld_config, RpmhRscDriverState),
        VMSTATE_UINT32(irq_enable, RpmhRscDriverState),
        VMSTATE_UINT32(irq_status, RpmhRscDriverState),
        VMSTATE_UINT32(num_tcs, RpmhRscDriverState),
        VMSTATE_UINT32(cmds_per_tcs, RpmhRscDriverState),
        VMSTATE_UINT32(tcs_offset, RpmhRscDriverState),
        VMSTATE_STRUCT_ARRAY(tcs_states, RpmhRscDriverState,
                             RPMH_RSC_MAX_TCS_PER_DRV, 0,
                             vmstate_rpmh_rsc_tcs, RpmhRscTcsState),
        VMSTATE_END_OF_LIST()
    }
};

static const VMStateDescription vmstate_rpmh_rsc = {
    .name = "rpmh-rsc",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_UINT32(version, RpmhRscState),
        VMSTATE_UINT32_ARRAY(regs, RpmhRscState, RPMH_RSC_MAX_REGS),
        VMSTATE_UINT32(num_drivers, RpmhRscState),
        VMSTATE_STRUCT_ARRAY(drivers, RpmhRscState,
                             RPMH_RSC_MAX_DRIVERS, 0,
                             vmstate_rpmh_rsc_driver, RpmhRscDriverState),
        VMSTATE_UINT32(tcs_base, RpmhRscState),
        VMSTATE_END_OF_LIST()
    }
};

static void rpmh_rsc_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    ResettableClass *rc = RESETTABLE_CLASS(klass);
    RpmhRscClass *rpmh_class = RPMH_RSC_CLASS(klass);

    dc->realize = rpmh_rsc_realize;
    dc->vmsd = &vmstate_rpmh_rsc;
    device_class_set_props(dc, rpmh_rsc_properties);
    resettable_class_set_parent_phases(rc, rpmh_rsc_reset_enter, NULL, NULL,
                                       &rpmh_class->parent_phases);
    set_bit(DEVICE_CATEGORY_MISC, dc->categories);
}

static const TypeInfo rpmh_rsc_info = {
    .name          = TYPE_RPMH_RSC,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(RpmhRscState),
    .class_size    = sizeof(RpmhRscClass),
    .class_init    = rpmh_rsc_class_init,
};

static void rpmh_rsc_register_types(void)
{
    type_register_static(&rpmh_rsc_info);
}

type_init(rpmh_rsc_register_types)

