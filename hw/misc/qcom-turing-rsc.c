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
#include "migration/vmstate.h"
#include "qapi/error.h"
#include "qemu/bitops.h"
#include "qemu/log.h"
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
    uint32_t tcs_block_base = TURING_TCS_AMC_MODE_IRQ_ENABLE_DRV0 +
                              s->tcs_base_offset;
    uint32_t cmd_base_in_tcs = s->cmd_base_in_tcs;

    if (!drv->present) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "Turing RSC: No driver for offset 0x%" HWADDR_PRIx "\n",
                      addr);
        return 0;
    }

    switch (offset) {
    /* ID and Parameter registers */
    case TURING_RSC_ID_DRV0:
        value = drv->rsc_id;
        break;
    case TURING_RSC_PARAM_SOLVER_CONFIG_DRV0:
        value = drv->solver_config;
        break;
    case TURING_RSC_PARAM_RSC_CONFIG_DRV0:
        value = drv->rsc_config;
        break;
    case TURING_RSC_PARAM_RSC_PARENTCHILD_CONFIG_DRV0:
        value = drv->parentchild_config;
        break;

    /* Status registers */
    case TURING_RSC_STATUS0_DRV0:
        value = drv->status0;
        break;
    case TURING_RSC_STATUS1_DRV0:
        value = drv->status1;
        break;
    case TURING_RSC_STATUS2_DRV0:
        value = drv->status2;
        break;

    /* Hidden TCS registers */
    case TURING_HIDDEN_TCS_CTRL_DRV0:
        value = drv->hidden_tcs_ctrl;
        break;
    case TURING_PDC_SEQ_START_ADDR_REG_OFFSET_DRV0:
        value = drv->pdc_seq_start_addr_reg_offset;
        break;
    case TURING_PDC_MATCH_VALUE_LO_REG_OFFSET_DRV0:
        value = drv->pdc_match_value_lo_reg_offset;
        break;
    case TURING_PDC_MATCH_VALUE_HI_REG_OFFSET_DRV0:
        value = drv->pdc_match_value_hi_reg_offset;
        break;
    case TURING_PDC_SLAVE_ID_DRV0:
        value = drv->pdc_slave_id;
        break;
    case TURING_HIDDEN_TCS_STATUS_DRV0:
        value = drv->hidden_tcs_status;
        break;
    case TURING_HIDDEN_TCS_CMD0_ADDR_DRV0:
        value = drv->hidden_tcs_cmds[0].addr;
        break;
    case TURING_HIDDEN_TCS_CMD0_DATA_DRV0:
        value = drv->hidden_tcs_cmds[0].data;
        break;
    case TURING_HIDDEN_TCS_CMD1_ADDR_DRV0:
        value = drv->hidden_tcs_cmds[1].addr;
        break;
    case TURING_HIDDEN_TCS_CMD1_DATA_DRV0:
        value = drv->hidden_tcs_cmds[1].data;
        break;
    case TURING_HIDDEN_TCS_CMD2_ADDR_DRV0:
        value = drv->hidden_tcs_cmds[2].addr;
        break;
    case TURING_HIDDEN_TCS_CMD2_DATA_DRV0:
        value = drv->hidden_tcs_cmds[2].data;
        break;

    /* Status IRQ registers */
    case TURING_RSC_ERROR_IRQ_STATUS_DRV0:
        value = 1;
        break;

    /* Error IRQ registers */
    case TURING_RSC_ERROR_IRQ_ENABLE_DRV0:
        value = drv->error_irq_enable;
        break;

    /* Control registers */
    case TURING_RSC_SECURE_OVERRIDE_DRV0:
        value = drv->secure_override;
        break;

    /* Sequencer registers */
    case TURING_RSC_SEQ_OVERRIDE_START_ADDR_DRV0:
        value = drv->seq_override_start_addr;
        break;
    case TURING_RSC_SEQ_BUSY_DRV0:
        value = drv->seq_busy;
        if (drv->seq_busy) {
            drv->seq_busy = 0;
            drv->seq_program_counter = 0;
        }
        break;
    case TURING_RSC_SEQ_PROGRAM_COUNTER_DRV0:
        value = drv->seq_program_counter;
        break;
    case TURING_RSC_SEQ_COMP_DRV0:
        value = drv->seq_comp;
        break;
    case TURING_RSC_SEQ_OVERRIDE_TRIGGER_DRV0:
        value = drv->seq_override_trigger;
        break;
    case TURING_RSC_SEQ_OVERRIDE_TRIGGER_START_ADDRESS_DRV0:
        value = drv->seq_override_trigger_start_address;
        break;

    /* TCS AMC Mode registers */
    case TURING_TCS_AMC_MODE_IRQ_ENABLE_DRV0:
        value = drv->tcs_amc_mode_irq_enable;
        break;

    case TURING_TCS_AMC_MODE_IRQ_STATUS_DRV0:
        value = drv->tcs_amc_mode_irq_status;
        break;
    case TURING_TCS_AMC_MODE_IRQ_CLEAR_DRV0:
        qemu_log_mask(LOG_GUEST_ERROR,
                      "Turing RSC: Read from IRQ Clear register at offset 0x%"
                      HWADDR_PRIx " is not meaningful\n", addr);
        value = drv->tcs_amc_mode_irq_status;
        break;

    default:
        /* Timeout registers (location varies by target) */
        if (offset == s->tcs_timeout_base) {
            value = drv->tcs_timeout_en;
            break;
        }
        if (s->timeout_clr_offset &&
            offset == s->tcs_timeout_base + s->timeout_clr_offset) {
            qemu_log_mask(LOG_GUEST_ERROR,
                          "Turing RSC: Read from write-only CLR register"
                          " at offset 0x%x\n", offset);
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

        /* TCS per-set registers: decode using property-driven layout */
        if (offset >= tcs_block_base &&
            offset < tcs_block_base +
                         (drv->num_tcs * TURING_TCS_SPACING)) {
            uint32_t tcs_rel = offset - tcs_block_base;
            uint32_t tcs_index = tcs_rel / TURING_TCS_SPACING;
            uint32_t reg_in_tcs = tcs_rel % TURING_TCS_SPACING;

            if (tcs_index < drv->num_tcs) {
                /* Per-TCS control regs at offsets 0..base_offset-1 */
                if (reg_in_tcs == 0x00) {
                    value = drv->tcs_states[tcs_index].cmd_wait_for_cmpl;
                    break;
                } else if (reg_in_tcs == 0x04) {
                    value = drv->tcs_states[tcs_index].control;
                    break;
                } else if (reg_in_tcs == 0x08) {
                    value = drv->tcs_states[tcs_index].status;
                    break;
                } else if (reg_in_tcs == 0x0C) {
                    value = drv->tcs_states[tcs_index].cmd_enable;
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
                        case 0x0: /* MSGID */
                            value = cmd->msgid;
                            break;
                        case 0x4: /* ADDR */
                            value = cmd->addr;
                            break;
                        case 0x8: /* DATA */
                            value = cmd->data;
                            break;
                        case 0xC: /* STATUS */
                            value = 0x10101;
                            break;
                        case 0x10: /* READ_RESPONSE_DATA */
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

        qemu_log_mask(LOG_GUEST_ERROR,
                      "Turing RSC: Invalid register read offset 0x%x\n",
                      offset);
        break;
    }

    return value;
}

/*
 * Handle a TCS CONTROL register write. When the AMC_MODE_TRIGGER bit is set,
 * simulate instant command completion: set the AMC completion IRQ status bit
 * for this TCS and assert the interrupt if enabled. The TCS status is left at
 * controller-idle since the emulation completes commands immediately.
 */
static void turing_rsc_handle_tcs_trigger(TuringRscDriverState *drv,
                                          int tcs_index, uint32_t val)
{
    drv->tcs_states[tcs_index].control = val;
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
    uint32_t tcs_block_base = TURING_TCS_AMC_MODE_IRQ_ENABLE_DRV0 +
                              s->tcs_base_offset;
    uint32_t cmd_base_in_tcs = s->cmd_base_in_tcs;

    if (!drv->present) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "Turing RSC: No driver for offset 0x%" HWADDR_PRIx "\n",
                      addr);
        return;
    }

    switch (offset) {
    /* Status registers (some writable fields) */
    case TURING_RSC_STATUS0_DRV0:
        drv->status0 = val;
        break;

    /* Hidden TCS registers */
    case TURING_HIDDEN_TCS_CTRL_DRV0:
        drv->hidden_tcs_ctrl = val;
        break;
    case TURING_PDC_SEQ_START_ADDR_REG_OFFSET_DRV0:
        drv->pdc_seq_start_addr_reg_offset = val;
        break;
    case TURING_PDC_MATCH_VALUE_LO_REG_OFFSET_DRV0:
        drv->pdc_match_value_lo_reg_offset = val;
        break;
    case TURING_PDC_MATCH_VALUE_HI_REG_OFFSET_DRV0:
        drv->pdc_match_value_hi_reg_offset = val;
        break;
    case TURING_PDC_SLAVE_ID_DRV0:
        drv->pdc_slave_id = val;
        break;
    case TURING_HIDDEN_TCS_CMD0_DATA_DRV0:
        drv->hidden_tcs_cmds[0].data = val;
        break;
    case TURING_HIDDEN_TCS_CMD1_DATA_DRV0:
        drv->hidden_tcs_cmds[1].data = val;
        break;
    case TURING_HIDDEN_TCS_CMD2_DATA_DRV0:
        drv->hidden_tcs_cmds[2].data = val;
        break;

    /* Error IRQ registers */
    case TURING_RSC_ERROR_IRQ_ENABLE_DRV0:
        drv->error_irq_enable = val;
        if (drv->error_irq_status & drv->error_irq_enable) {
            qemu_set_irq(drv->error_irq, 1);
        } else {
            qemu_set_irq(drv->error_irq, 0);
        }
        break;

    /* Control registers */
    case TURING_RSC_SECURE_OVERRIDE_DRV0:
        drv->secure_override = val;
        break;

    /* Sequencer registers */
    case TURING_RSC_SEQ_OVERRIDE_START_ADDR_DRV0:
        drv->seq_override_start_addr = val;
        break;
    case TURING_RSC_SEQ_BUSY_DRV0:
        drv->seq_busy = val;
        break;
    case TURING_RSC_SEQ_PROGRAM_COUNTER_DRV0:
        drv->seq_program_counter = val;
        break;
    case TURING_RSC_SEQ_COMP_DRV0:
        if (val & BIT(31)) {
            drv->seq_comp &= ~1;
        }
        break;
    case TURING_RSC_SEQ_OVERRIDE_TRIGGER_DRV0:
        drv->seq_override_trigger = val;
        if (val & 0x1) {
            /* Sequencer completes instantly in emulation */
            drv->seq_busy = 0;
            drv->seq_program_counter = 0;
        }
        break;
    case TURING_RSC_SEQ_OVERRIDE_TRIGGER_START_ADDRESS_DRV0:
        drv->seq_override_trigger_start_address = val;
        drv->seq_program_counter = val;
        drv->seq_busy = 1;
        drv->seq_comp = 1;
        break;

    /* TCS AMC Mode registers */
    case TURING_TCS_AMC_MODE_IRQ_ENABLE_DRV0:
        drv->tcs_amc_mode_irq_enable = val;
        if (drv->tcs_amc_mode_irq_status & drv->tcs_amc_mode_irq_enable) {
            qemu_set_irq(drv->amc_irq, 1);
        } else {
            qemu_set_irq(drv->amc_irq, 0);
        }
        break;
    case TURING_TCS_AMC_MODE_IRQ_CLEAR_DRV0:
        drv->tcs_amc_mode_irq_status &= ~val;
        if ((drv->tcs_amc_mode_irq_status &
             drv->tcs_amc_mode_irq_enable) == 0) {
            qemu_set_irq(drv->amc_irq, 0);
        }
        break;
    case TURING_RSC_ERROR_IRQ_CLEAR_DRV0:
        drv->error_irq_status &= ~val;
        if ((drv->error_irq_status & drv->error_irq_enable) == 0) {
            qemu_set_irq(drv->error_irq, 0);
        }
        break;

    default:
        /* Timeout registers (location varies by target) */
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

        /* TCS per-set registers: decode using property-driven layout */
        if (offset >= tcs_block_base &&
            offset < tcs_block_base +
                         (drv->num_tcs * TURING_TCS_SPACING)) {
            uint32_t tcs_rel = offset - tcs_block_base;
            uint32_t tcs_index = tcs_rel / TURING_TCS_SPACING;
            uint32_t reg_in_tcs = tcs_rel % TURING_TCS_SPACING;

            if (tcs_index < drv->num_tcs) {
                if (reg_in_tcs == 0x00) {
                    drv->tcs_states[tcs_index].cmd_wait_for_cmpl = val;
                    break;
                } else if (reg_in_tcs == 0x04) {
                    turing_rsc_handle_tcs_trigger(drv, tcs_index, val);
                    break;
                } else if (reg_in_tcs == 0x0C) {
                    drv->tcs_states[tcs_index].cmd_enable = val;
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
                        case 0x0: /* MSGID */
                            cmd->msgid = val;
                            break;
                        case 0x4: /* ADDR */
                            cmd->addr = val;
                            break;
                        case 0x8: /* DATA */
                            cmd->data = val;
                            break;
                        case 0xC: /* STATUS - read-only */
                        case 0x10: /* READ_RESPONSE_DATA - read-only */
                            qemu_log_mask(LOG_GUEST_ERROR,
                                "Turing RSC: Write to read-only register "
                                "0x%x (TCS%d CMD%d)\n",
                                offset, tcs_index, cmd_index);
                            break;
                        default:
                            break;
                        }
                        break;
                    }
                }
            }
        }

        qemu_log_mask(LOG_GUEST_ERROR,
                      "Turing RSC: Invalid register write offset 0x%x\n",
                      offset);
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

    /* Clamp property-driven bounds to array maximums */
    if (s->num_br_addr > TURING_RSC_MAX_BR_ADDR) {
        s->num_br_addr = TURING_RSC_MAX_BR_ADDR;
    }
    if (s->num_timestamp_units > TURING_RSC_MAX_TIMESTAMP_UNITS) {
        s->num_timestamp_units = TURING_RSC_MAX_TIMESTAMP_UNITS;
    }

    /* Reset driver */
    TuringRscDriverState *drv = &s->drivers[0];
    if (drv->present) {
        drv->rsc_id = s->rsc_id_reset;
        drv->solver_config = s->solver_config_reset;
        drv->rsc_config = s->rsc_config_reset;
        drv->parentchild_config = s->parentchild_config_reset;

        drv->status0 = 0;
        drv->status1 = 0;
        drv->status2 = 0;

        drv->hidden_tcs_ctrl = 0;
        drv->pdc_seq_start_addr_reg_offset = RSC_RESET_PDC_SEQ_START_ADDR;
        drv->pdc_match_value_lo_reg_offset = RSC_RESET_PDC_MATCH_VALUE_LO;
        drv->pdc_match_value_hi_reg_offset = RSC_RESET_PDC_MATCH_VALUE_HI;
        drv->pdc_slave_id = RSC_RESET_PDC_SLAVE_ID;
        drv->hidden_tcs_status = 0;

        drv->hidden_tcs_cmds[0].addr = RSC_RESET_HIDDEN_TCS_CMD0_ADDR;
        drv->hidden_tcs_cmds[0].data = 0;
        drv->hidden_tcs_cmds[1].addr = RSC_RESET_HIDDEN_TCS_CMD1_ADDR;
        drv->hidden_tcs_cmds[1].data = 0;
        drv->hidden_tcs_cmds[2].addr = RSC_RESET_HIDDEN_TCS_CMD2_ADDR;
        drv->hidden_tcs_cmds[2].data = 0;

        drv->secure_override = RSC_RESET_SECURE_OVERRIDE;
        drv->tcs_timeout_val = RSC_RESET_TCS_TIMEOUT_VAL;

        drv->num_tcs = extract32(drv->parentchild_config,
                                 TURING_PARENTCHILD_CONFIG_NUM_TCS_DRV0_SHIFT,
                                 6);
        g_assert(drv->num_tcs <= TURING_RSC_MAX_TCS_PER_DRV);

        /* Reset all TCS states */
        memset(drv->tcs_states, 0, sizeof(drv->tcs_states));
        for (int i = 0; i < drv->num_tcs; i++) {
            drv->tcs_states[i].status = TURING_TCS_CONTROLLER_IDLE;
        }

        /* Clear other registers */
        drv->error_irq_status = 0;
        drv->error_irq_enable = 0;
        drv->error_resp_ctrl = 0;
        drv->rif_clk_gating_override = 0;
        drv->hw_event_owner = 0;
        drv->timestamp_unit_owner = 0;
        drv->tcs_amc_mode_irq_enable = 0;
        drv->tcs_amc_mode_irq_status = 0;
        drv->tcs_timeout_en = 0;
        drv->tcs_timeout_status = 0;

        memset(drv->hw_event_mux_select, 0, sizeof(drv->hw_event_mux_select));
        memset(drv->timestamp_units, 0, sizeof(drv->timestamp_units));
        memset(drv->seq_cfg_delay_val, 0, sizeof(drv->seq_cfg_delay_val));
        memset(drv->seq_cfg_br_addr, 0, sizeof(drv->seq_cfg_br_addr));
        memset(drv->seq_mem, 0, sizeof(drv->seq_mem));
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
    DEFINE_PROP_UINT32("num-br-addr", TuringRscState, num_br_addr, 0),
    DEFINE_PROP_UINT32("num-timestamp-units", TuringRscState,
                       num_timestamp_units, 0),
};

static void turing_rsc_realize(DeviceState *dev, Error **errp)
{
    TuringRscState *s = TURING_RSC(dev);
    SysBusDevice *sbd = SYS_BUS_DEVICE(dev);

    if (!s->rsc_id_reset || !s->solver_config_reset ||
        !s->rsc_config_reset || !s->parentchild_config_reset ||
        !s->cmd_spacing || !s->tcs_base_offset || !s->cmd_base_in_tcs ||
        !s->tcs_timeout_base || !s->timeout_status_offset ||
        !s->num_br_addr || !s->num_timestamp_units) {
        error_setg(errp, "turing-rsc: required properties must be set to"
                   " non-zero values");
        return;
    }

    s->version = s->rsc_id_reset;

    /* Mark driver as present (register values are set in reset) */
    TuringRscDriverState *drv = &s->drivers[0];
    drv->present = true;
    drv->driver_id = 0;

    /* Initialize IRQ lines */
    sysbus_init_irq(sbd, &drv->error_irq);
    sysbus_init_irq(sbd, &drv->amc_irq);

    /* Initialize memory region */
    memory_region_init_io(&s->iomem, OBJECT(s), &turing_rsc_ops, s,
                          TYPE_TURING_RSC, TURING_RSC_REGISTER_SPACE_SIZE);
    sysbus_init_mmio(sbd, &s->iomem);
}

static const VMStateDescription vmstate_turing_rsc_timestamp_unit = {
    .name = "turing-rsc-timestamp-unit",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields =
        (VMStateField[]){ VMSTATE_UINT32(enable, TuringRscTimestampUnit),
                          VMSTATE_UINT32(timestamp_l, TuringRscTimestampUnit),
                          VMSTATE_UINT32(timestamp_h, TuringRscTimestampUnit),
                          VMSTATE_UINT32(output, TuringRscTimestampUnit),
                          VMSTATE_END_OF_LIST() }
};

static const VMStateDescription vmstate_turing_rsc_hidden_tcs_command = {
    .name = "turing-rsc-hidden-tcs-command",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]){ VMSTATE_UINT32(addr, TuringRscHiddenTcsCommand),
                                VMSTATE_UINT32(data, TuringRscHiddenTcsCommand),
                                VMSTATE_END_OF_LIST() }
};

static const VMStateDescription vmstate_turing_rsc_command = {
    .name = "turing-rsc-command",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields =
        (VMStateField[]){ VMSTATE_UINT32(msgid, TuringRscCommand),
                          VMSTATE_UINT32(addr, TuringRscCommand),
                          VMSTATE_UINT32(data, TuringRscCommand),
                          VMSTATE_UINT32(status, TuringRscCommand),
                          VMSTATE_UINT32(read_response_data, TuringRscCommand),
                          VMSTATE_END_OF_LIST() }
};

static const VMStateDescription vmstate_turing_rsc_tcs = {
    .name = "turing-rsc-tcs",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields =
        (VMStateField[]){ VMSTATE_UINT32(cmd_wait_for_cmpl, TuringRscTcsState),
                          VMSTATE_UINT32(control, TuringRscTcsState),
                          VMSTATE_UINT32(status, TuringRscTcsState),
                          VMSTATE_UINT32(cmd_enable, TuringRscTcsState),
                          VMSTATE_BOOL(triggered, TuringRscTcsState),
                          VMSTATE_STRUCT_ARRAY(commands, TuringRscTcsState,
                                               TURING_RSC_MAX_CMDS_PER_TCS, 0,
                                               vmstate_turing_rsc_command,
                                               TuringRscCommand),
                          VMSTATE_END_OF_LIST() }
};

static const VMStateDescription vmstate_turing_rsc_driver = {
    .name = "turing-rsc-driver",
    .version_id = 2,
    .minimum_version_id = 2,
    .fields =
        (VMStateField[]){
            VMSTATE_BOOL(present, TuringRscDriverState),
            VMSTATE_UINT32(driver_id, TuringRscDriverState),
            VMSTATE_UINT32(rsc_id, TuringRscDriverState),
            VMSTATE_UINT32(solver_config, TuringRscDriverState),
            VMSTATE_UINT32(rsc_config, TuringRscDriverState),
            VMSTATE_UINT32(parentchild_config, TuringRscDriverState),
            VMSTATE_UINT32(status0, TuringRscDriverState),
            VMSTATE_UINT32(status1, TuringRscDriverState),
            VMSTATE_UINT32(status2, TuringRscDriverState),
            VMSTATE_UINT32(hidden_tcs_ctrl, TuringRscDriverState),
            VMSTATE_UINT32(pdc_seq_start_addr_reg_offset, TuringRscDriverState),
            VMSTATE_UINT32(pdc_match_value_lo_reg_offset, TuringRscDriverState),
            VMSTATE_UINT32(pdc_match_value_hi_reg_offset, TuringRscDriverState),
            VMSTATE_UINT32(pdc_slave_id, TuringRscDriverState),
            VMSTATE_UINT32(hidden_tcs_status, TuringRscDriverState),
            VMSTATE_STRUCT_ARRAY(hidden_tcs_cmds, TuringRscDriverState, 3, 0,
                                 vmstate_turing_rsc_hidden_tcs_command,
                                 TuringRscHiddenTcsCommand),
            VMSTATE_UINT32(hw_event_owner, TuringRscDriverState),
            VMSTATE_UINT32_ARRAY(hw_event_mux_select, TuringRscDriverState,
                                 TURING_RSC_MAX_HW_EVENT_MUX),
            VMSTATE_UINT32(error_irq_status, TuringRscDriverState),
            VMSTATE_UINT32(error_irq_enable, TuringRscDriverState),
            VMSTATE_UINT32(error_resp_ctrl, TuringRscDriverState),
            VMSTATE_UINT32(secure_override, TuringRscDriverState),
            VMSTATE_UINT32(rif_clk_gating_override, TuringRscDriverState),
            VMSTATE_UINT32(timestamp_unit_owner, TuringRscDriverState),
            VMSTATE_STRUCT_ARRAY(timestamp_units, TuringRscDriverState,
                                 TURING_RSC_MAX_TIMESTAMP_UNITS, 0,
                                 vmstate_turing_rsc_timestamp_unit,
                                 TuringRscTimestampUnit),
            VMSTATE_UINT32_ARRAY(seq_cfg_delay_val, TuringRscDriverState,
                                 TURING_RSC_MAX_DELAY_VAL),
            VMSTATE_UINT32_ARRAY(seq_cfg_br_addr, TuringRscDriverState,
                                 TURING_RSC_MAX_BR_ADDR),
            VMSTATE_UINT32_ARRAY(seq_mem, TuringRscDriverState,
                                 TURING_RSC_MAX_SEQ_MEM),
            VMSTATE_UINT32(tcs_amc_mode_irq_enable, TuringRscDriverState),
            VMSTATE_UINT32(tcs_amc_mode_irq_status, TuringRscDriverState),
            VMSTATE_UINT32(tcs_timeout_en, TuringRscDriverState),
            VMSTATE_UINT32(tcs_timeout_status, TuringRscDriverState),
            VMSTATE_UINT32(tcs_timeout_val, TuringRscDriverState),
            VMSTATE_UINT32(num_tcs, TuringRscDriverState),
            VMSTATE_STRUCT_ARRAY(tcs_states, TuringRscDriverState,
                                 TURING_RSC_MAX_TCS_PER_DRV, 0,
                                 vmstate_turing_rsc_tcs, TuringRscTcsState),
            VMSTATE_END_OF_LIST() }
};

static const VMStateDescription vmstate_turing_rsc = {
    .name = "turing-rsc",
    .version_id = 2,
    .minimum_version_id = 2,
    .fields = (VMStateField[]){ VMSTATE_UINT32(version, TuringRscState),
                                VMSTATE_STRUCT_ARRAY(drivers, TuringRscState,
                                                     TURING_RSC_MAX_DRIVERS, 0,
                                                     vmstate_turing_rsc_driver,
                                                     TuringRscDriverState),
                                VMSTATE_END_OF_LIST() }
};

static void turing_rsc_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    ResettableClass *rc = RESETTABLE_CLASS(klass);
    TuringRscClass *turing_class = TURING_RSC_CLASS(klass);

    dc->realize = turing_rsc_realize;
    dc->vmsd = &vmstate_turing_rsc;
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
