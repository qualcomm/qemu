/*
 * Qualcomm Turing RSC (Resource State Coordinator)
 * TURINGNSP_0_TURING_RSCC_RSCC_RSC
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "hw/core/irq.h"
#include "hw/core/qdev-properties.h"
#include "hw/core/resettable.h"
#include "hw/core/sysbus.h"
#include "hw/misc/qcom-turing-rsc.h"
#include "qapi/error.h"
#include "qemu/bitops.h"
#include "qemu/module.h"

/* Fixed reset values (not target-dependent) */
#define RSC_RESET_PDC_SEQ_START_ADDR      0x00004520
#define RSC_RESET_PDC_MATCH_VALUE_LO      0x00004510
#define RSC_RESET_PDC_MATCH_VALUE_HI      0x00004514
#define RSC_RESET_PDC_SLAVE_ID            0x00000001
#define RSC_RESET_HIDDEN_TCS_CMD0_ADDR    0x82204514
#define RSC_RESET_HIDDEN_TCS_CMD1_ADDR    0x82204510
#define RSC_RESET_HIDDEN_TCS_CMD2_ADDR    0x82204520
#define RSC_RESET_SECURE_OVERRIDE         0x00000001
#define RSC_RESET_TCS_TIMEOUT_VAL         0x0000FFFF

/* Register read handler */
static uint64_t turing_rsc_read(void *opaque, hwaddr addr, unsigned size)
{
    TuringRscState *s = TURING_RSC(opaque);
    TuringRscDriverState *drv = &s->drivers[0];
    uint32_t offset = addr;
    uint64_t value = 0;
    uint32_t tcs_block_base = 0x0D00 +
                              s->tcs_base_offset;
    uint32_t cmd_base_in_tcs = s->cmd_base_in_tcs;

    if (!drv->present) {
        return 0;
    }

    switch (offset) {
    case 0x0000 ... 0x0048:
        if (offset & 3) {
            break;
        }
        value = drv->low[offset / 4];
        break;

    case 0x00D0:
        value = 1;
        break;

    case 0x00D8:
        value = drv->error_irq_enable;
        break;

    case 0x0104:
        value = drv->secure_override;
        break;

    case 0x0400:
        value = drv->seq_override_start_addr;
        break;
    case 0x0404:
        value = drv->seq_busy;
        if (drv->seq_busy) {
            drv->seq_busy = 0;
            drv->seq_program_counter = 0;
        }
        break;
    case 0x0408:
        value = drv->seq_program_counter;
        break;
    case 0x0410:
        value = drv->seq_comp;
        break;
    case 0x0460:
        value = drv->seq_override_trigger;
        break;
    case 0x0464:
        value = drv->seq_override_trigger_start_address;
        break;

    case 0x0D00:
        value = drv->tcs_amc_mode_irq_enable;
        break;
    case 0x0D04:
    case 0x0D08:
        value = drv->tcs_amc_mode_irq_status;
        break;

    default:
        if (offset == s->tcs_timeout_base) {
            value = drv->tcs_timeout_en;
            break;
        }
        if (s->timeout_clr_offset &&
            offset == s->tcs_timeout_base + s->timeout_clr_offset) {
            value = 0;
            break;
        }
        if (offset == s->tcs_timeout_base + s->timeout_status_offset) {
            value = drv->tcs_timeout_status;
            break;
        }
        if (s->timeout_val_offset &&
            offset == s->tcs_timeout_base + s->timeout_val_offset) {
            value = drv->tcs_timeout_val;
            break;
        }

        /* TCS and command registers, decoded using property-driven layout */
        if (offset >= tcs_block_base &&
            offset < tcs_block_base +
                         (drv->num_tcs * TURING_TCS_SPACING)) {
            uint32_t tcs_rel = offset - tcs_block_base;
            uint32_t tcs_index = tcs_rel / TURING_TCS_SPACING;
            uint32_t reg_in_tcs = tcs_rel % TURING_TCS_SPACING;

            if (tcs_index < drv->num_tcs) {
                if (reg_in_tcs <= 0x0C && !(reg_in_tcs & 3)) {
                    value = drv->tcs_states[tcs_index].words[reg_in_tcs / 4];
                    break;
                } else if (reg_in_tcs >= cmd_base_in_tcs &&
                           reg_in_tcs < cmd_base_in_tcs +
                               (TURING_RSC_MAX_CMDS_PER_TCS *
                                s->cmd_spacing)) {
                    uint32_t cmd_rel = reg_in_tcs - cmd_base_in_tcs;
                    uint32_t cmd_index = cmd_rel / s->cmd_spacing;
                    uint32_t reg_in_cmd = cmd_rel % s->cmd_spacing;

                    if (cmd_index < TURING_RSC_MAX_CMDS_PER_TCS) {
                        TuringRscCommand *cmd =
                            &drv->tcs_states[tcs_index].commands[cmd_index];
                        switch (reg_in_cmd) {
                        case 0x0 ... 0x8:
                            value = cmd->words[reg_in_cmd / 4];
                            break;
                        case 0xC:
                            value = 0x10101;
                            break;
                        case 0x10:
                            value = cmd->read_response_data;
                            break;
                        default:
                            break;
                        }
                        break;
                    }
                }
            }
        }
        break;
    }

    return value;
}

/*
 * When bit 24 is set, simulate instant command completion for a TCS:
 * mark it triggered and raise the AMC IRQ if enabled.
 */
static void turing_rsc_handle_tcs_trigger(TuringRscDriverState *drv,
                                          int tcs_index, uint32_t val)
{
    drv->tcs_states[tcs_index].words[1] = val;
    if (val & TURING_TCS_AMC_MODE_TRIGGER) {
        drv->tcs_states[tcs_index].triggered = true;
        /* Set AMC completion IRQ status bit for this TCS */
        drv->tcs_amc_mode_irq_status |= BIT(tcs_index);
        if (drv->tcs_amc_mode_irq_enable & BIT(tcs_index)) {
            qemu_set_irq(drv->amc_irq, 1);
        }
    }
}

/* Register write handler */
static void turing_rsc_write(void *opaque, hwaddr addr, uint64_t value,
                             unsigned size)
{
    TuringRscState *s = TURING_RSC(opaque);
    TuringRscDriverState *drv = &s->drivers[0];
    uint32_t offset = addr;
    uint32_t val = (uint32_t)value;
    uint32_t tcs_block_base = 0x0D00 +
                              s->tcs_base_offset;
    uint32_t cmd_base_in_tcs = s->cmd_base_in_tcs;

    if (!drv->present) {
        return;
    }

    switch (offset) {
    case 0x0010:
    case 0x001C:
    case 0x0020:
    case 0x0024:
    case 0x0028:
    case 0x002C:
    case 0x0038:
    case 0x0040:
    case 0x0048:
        drv->low[offset / 4] = val;
        break;

    case 0x00D8:
        drv->error_irq_enable = val;
        if (drv->error_irq_status & drv->error_irq_enable) {
            qemu_set_irq(drv->error_irq, 1);
        } else {
            qemu_set_irq(drv->error_irq, 0);
        }
        break;

    case 0x0104:
        drv->secure_override = val;
        break;

    case 0x0400:
        drv->seq_override_start_addr = val;
        break;
    case 0x0404:
        drv->seq_busy = val;
        break;
    case 0x0408:
        drv->seq_program_counter = val;
        break;
    case 0x0410:
        if (val & BIT(31)) {
            drv->seq_comp &= ~1;
        }
        break;
    case 0x0460:
        drv->seq_override_trigger = val;
        if (val & 0x1) {
            drv->seq_busy = 0;
            drv->seq_program_counter = 0;
        }
        break;
    case 0x0464:
        drv->seq_override_trigger_start_address = val;
        drv->seq_program_counter = val;
        drv->seq_busy = 1;
        drv->seq_comp = 1;
        break;

    case 0x0D00:
        drv->tcs_amc_mode_irq_enable = val;
        if (drv->tcs_amc_mode_irq_status & drv->tcs_amc_mode_irq_enable) {
            qemu_set_irq(drv->amc_irq, 1);
        } else {
            qemu_set_irq(drv->amc_irq, 0);
        }
        break;
    case 0x0D08:
        drv->tcs_amc_mode_irq_status &= ~val;
        if ((drv->tcs_amc_mode_irq_status &
             drv->tcs_amc_mode_irq_enable) == 0) {
            qemu_set_irq(drv->amc_irq, 0);
        }
        break;
    case 0x00D4:
        drv->error_irq_status &= ~val;
        if ((drv->error_irq_status & drv->error_irq_enable) == 0) {
            qemu_set_irq(drv->error_irq, 0);
        }
        break;

    default:
        if (offset == s->tcs_timeout_base) {
            drv->tcs_timeout_en = val;
            break;
        }
        if (s->timeout_clr_offset &&
            offset == s->tcs_timeout_base + s->timeout_clr_offset) {
            drv->tcs_timeout_status &= ~val;
            break;
        }
        if (s->timeout_val_offset &&
            offset == s->tcs_timeout_base + s->timeout_val_offset) {
            drv->tcs_timeout_val = val;
            break;
        }

        /* TCS and command registers, decoded using property-driven layout */
        if (offset >= tcs_block_base &&
            offset < tcs_block_base +
                         (drv->num_tcs * TURING_TCS_SPACING)) {
            uint32_t tcs_rel = offset - tcs_block_base;
            uint32_t tcs_index = tcs_rel / TURING_TCS_SPACING;
            uint32_t reg_in_tcs = tcs_rel % TURING_TCS_SPACING;

            if (tcs_index < drv->num_tcs) {
                if (reg_in_tcs == 0x00) {
                    drv->tcs_states[tcs_index].words[0] = val;
                    break;
                } else if (reg_in_tcs == 0x04) {
                    turing_rsc_handle_tcs_trigger(drv, tcs_index, val);
                    break;
                } else if (reg_in_tcs == 0x0C) {
                    drv->tcs_states[tcs_index].words[3] = val;
                    break;
                } else if (reg_in_tcs >= cmd_base_in_tcs &&
                           reg_in_tcs < cmd_base_in_tcs +
                               (TURING_RSC_MAX_CMDS_PER_TCS *
                                s->cmd_spacing)) {
                    uint32_t cmd_rel = reg_in_tcs - cmd_base_in_tcs;
                    uint32_t cmd_index = cmd_rel / s->cmd_spacing;
                    uint32_t reg_in_cmd = cmd_rel % s->cmd_spacing;

                    if (cmd_index < TURING_RSC_MAX_CMDS_PER_TCS) {
                        TuringRscCommand *cmd =
                            &drv->tcs_states[tcs_index].commands[cmd_index];
                        if (reg_in_cmd <= 0x8) {
                            cmd->words[reg_in_cmd / 4] = val;
                        }
                        break;
                    }
                }
            }
        }
        break;
    }
}

static const MemoryRegionOps turing_rsc_ops = {
    .read = turing_rsc_read,
    .write = turing_rsc_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .impl = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
};

static void turing_rsc_reset_enter(Object *obj, ResetType type)
{
    TuringRscState *s = TURING_RSC(obj);
    TuringRscClass *turing_class = TURING_RSC_GET_CLASS(obj);

    /* Call parent reset phase */
    if (turing_class->parent_phases.enter) {
        turing_class->parent_phases.enter(obj, type);
    }

    s->version = s->rsc_id_reset;

    /* Reset driver */
    TuringRscDriverState *drv = &s->drivers[0];
    if (drv->present) {
        memset(drv->low, 0, sizeof(drv->low));
        drv->low[0] = s->rsc_id_reset;
        drv->low[1] = s->solver_config_reset;
        drv->low[2] = s->rsc_config_reset;
        drv->low[3] = s->parentchild_config_reset;
        drv->low[8] = RSC_RESET_PDC_SEQ_START_ADDR;
        drv->low[9] = RSC_RESET_PDC_MATCH_VALUE_LO;
        drv->low[10] = RSC_RESET_PDC_MATCH_VALUE_HI;
        drv->low[11] = RSC_RESET_PDC_SLAVE_ID;
        drv->low[13] = RSC_RESET_HIDDEN_TCS_CMD0_ADDR;
        drv->low[15] = RSC_RESET_HIDDEN_TCS_CMD1_ADDR;
        drv->low[17] = RSC_RESET_HIDDEN_TCS_CMD2_ADDR;

        drv->secure_override = RSC_RESET_SECURE_OVERRIDE;
        drv->tcs_timeout_val = RSC_RESET_TCS_TIMEOUT_VAL;

        drv->num_tcs = extract32(drv->low[3],
                                 TURING_PARENTCHILD_CONFIG_NUM_TCS_DRV0_SHIFT,
                                 6);
        g_assert(drv->num_tcs <= TURING_RSC_MAX_TCS_PER_DRV);

        /* Reset all TCS states */
        memset(drv->tcs_states, 0, sizeof(drv->tcs_states));
        for (int i = 0; i < drv->num_tcs; i++) {
            drv->tcs_states[i].words[2] = TURING_TCS_CONTROLLER_IDLE;
        }

        /* Clear other registers */
        drv->error_irq_status = 0;
        drv->error_irq_enable = 0;
        drv->tcs_amc_mode_irq_enable = 0;
        drv->tcs_amc_mode_irq_status = 0;
        drv->tcs_timeout_en = 0;
        drv->tcs_timeout_status = 0;
    }
}

static const Property turing_rsc_properties[] = {
    DEFINE_PROP_UINT32("rsc-id-reset", TuringRscState, rsc_id_reset, 0),
    DEFINE_PROP_UINT32("solver-config-reset", TuringRscState,
                       solver_config_reset, 0),
    DEFINE_PROP_UINT32("rsc-config-reset", TuringRscState,
                       rsc_config_reset, 0),
    DEFINE_PROP_UINT32("parentchild-config-reset", TuringRscState,
                       parentchild_config_reset, 0),
    DEFINE_PROP_UINT32("cmd-spacing", TuringRscState, cmd_spacing, 0),
    DEFINE_PROP_UINT32("tcs-base-offset", TuringRscState, tcs_base_offset, 0),
    DEFINE_PROP_UINT32("cmd-base-in-tcs", TuringRscState, cmd_base_in_tcs, 0),
    DEFINE_PROP_UINT32("tcs-timeout-base", TuringRscState, tcs_timeout_base, 0),
    DEFINE_PROP_UINT32("timeout-clr-offset", TuringRscState,
                       timeout_clr_offset, 0),
    DEFINE_PROP_UINT32("timeout-status-offset", TuringRscState,
                       timeout_status_offset, 0),
    DEFINE_PROP_UINT32("timeout-val-offset", TuringRscState,
                       timeout_val_offset, 0),
};

static void turing_rsc_realize(DeviceState *dev, Error **errp)
{
    TuringRscState *s = TURING_RSC(dev);
    SysBusDevice *sbd = SYS_BUS_DEVICE(dev);

    if (!s->rsc_id_reset || !s->solver_config_reset ||
        !s->rsc_config_reset || !s->parentchild_config_reset ||
        !s->cmd_spacing || !s->tcs_base_offset || !s->cmd_base_in_tcs ||
        !s->tcs_timeout_base || !s->timeout_status_offset) {
        error_setg(errp, "turing-rsc: required properties must be set to"
                   " non-zero values");
        return;
    }

    s->version = s->rsc_id_reset;

    /* Mark driver as present (register values are set in reset) */
    TuringRscDriverState *drv = &s->drivers[0];
    drv->present = true;

    /* Initialize IRQ lines */
    sysbus_init_irq(sbd, &drv->error_irq);
    sysbus_init_irq(sbd, &drv->amc_irq);

    /* Initialize memory region */
    memory_region_init_io(&s->iomem, OBJECT(s), &turing_rsc_ops, s,
                          TYPE_TURING_RSC, TURING_RSC_REGISTER_SPACE_SIZE);
    sysbus_init_mmio(sbd, &s->iomem);
}

static void turing_rsc_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    ResettableClass *rc = RESETTABLE_CLASS(klass);
    TuringRscClass *turing_class = TURING_RSC_CLASS(klass);

    dc->realize = turing_rsc_realize;
    device_class_set_props(dc, turing_rsc_properties);
    resettable_class_set_parent_phases(rc, turing_rsc_reset_enter, NULL, NULL,
                                       &turing_class->parent_phases);
    set_bit(DEVICE_CATEGORY_MISC, dc->categories);
}

static const TypeInfo turing_rsc_info = {
    .name = TYPE_TURING_RSC,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(TuringRscState),
    .class_size = sizeof(TuringRscClass),
    .class_init = turing_rsc_class_init,
};

static void turing_rsc_register_types(void)
{
    type_register_static(&turing_rsc_info);
}

type_init(turing_rsc_register_types)
