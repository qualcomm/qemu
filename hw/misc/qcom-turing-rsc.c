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
#include "qemu/log.h"
#include "qemu/module.h"

/* Default reset values based on IP catalog */
static const uint32_t turing_rsc_reset_values[] = {
    [TURING_RSC_ID_DRV0] = 0x00020400,
    [TURING_RSC_PARAM_SOLVER_CONFIG_DRV0] = 0x00010100,
    [TURING_RSC_PARAM_RSC_CONFIG_DRV0] = 0x01300214,
    [TURING_RSC_PARAM_RSC_PARENTCHILD_CONFIG_DRV0] = 0x80000006,
    [TURING_RSC_STATUS0_DRV0] = 0x00000000,
    [TURING_RSC_STATUS1_DRV0] = 0x00000000,
    [TURING_RSC_STATUS2_DRV0] = 0x00000000,
    [TURING_HIDDEN_TCS_CTRL_DRV0] = 0x00000000,
    [TURING_PDC_SEQ_START_ADDR_REG_OFFSET_DRV0] = 0x00004520,
    [TURING_PDC_MATCH_VALUE_LO_REG_OFFSET_DRV0] = 0x00004510,
    [TURING_PDC_MATCH_VALUE_HI_REG_OFFSET_DRV0] = 0x00004514,
    [TURING_PDC_SLAVE_ID_DRV0] = 0x00000001,
    [TURING_HIDDEN_TCS_STATUS_DRV0] = 0x00000000,
    [TURING_HIDDEN_TCS_CMD0_ADDR_DRV0] = 0x82204514,
    [TURING_HIDDEN_TCS_CMD0_DATA_DRV0] = 0x00000000,
    [TURING_HIDDEN_TCS_CMD1_ADDR_DRV0] = 0x82204510,
    [TURING_HIDDEN_TCS_CMD1_DATA_DRV0] = 0x00000000,
    [TURING_HIDDEN_TCS_CMD2_ADDR_DRV0] = 0x82204520,
    [TURING_HIDDEN_TCS_CMD2_DATA_DRV0] = 0x00000000,
    [TURING_RSC_SECURE_OVERRIDE_DRV0] = 0x00000001,
    [TURING_TCS0_DRV0_STATUS] = 0x00000001,
    [TURING_TCS_TIMEOUT_VAL_DRV0] = 0x0000FFFF,
};

/* Register read handler */
static uint64_t turing_rsc_read(void *opaque, hwaddr addr, unsigned size)
{
    TuringRscState *s = TURING_RSC(opaque);
    TuringRscDriverState *drv = &s->drivers[0];
    uint32_t offset = addr;
    uint64_t value = 0;

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

    /* TCS AMC Mode registers */
    case TURING_TCS_AMC_MODE_IRQ_ENABLE_DRV0:
        value = drv->tcs_amc_mode_irq_enable;
        break;

    case TURING_TCS_AMC_MODE_IRQ_STATUS_DRV0:
        value = drv->tcs_amc_mode_irq_status;
        break;
    case TURING_TCS_AMC_MODE_IRQ_CLEAR_DRV0:
        qemu_log_mask(LOG_GUEST_ERROR,
                      "Turing RSC: Read from IRQ Clear register at offset 0x%" HWADDR_PRIx
                      " is not meaningful\n", addr);
        value = drv->tcs_amc_mode_irq_status;
        break;

    /* TCS registers */
    case TURING_TCS0_DRV0_STATUS:
        value = drv->tcs_states[0].status;
        break;
    case TURING_TCS0_DRV0_STATUS + 1 * 0x2a0:
        value = drv->tcs_states[1].status;
        break;
    case TURING_TCS0_DRV0_STATUS + 2 * 0x2a0:
        value = drv->tcs_states[2].status;
        break;
    case TURING_TCS0_DRV0_STATUS + 3 * 0x2a0:
        value = drv->tcs_states[3].status;
        break;
    case TURING_TCS0_DRV0_STATUS + 4 * 0x2a0:
        value = drv->tcs_states[4].status;
        break;
    case TURING_TCS0_DRV0_STATUS + 5 * 0x2a0:
        value = drv->tcs_states[5].status;
        break;

    /* TCS Control registers */
    case TURING_TCS0_DRV0_CMD_WAIT_FOR_CMPL:
        value = drv->tcs_states[0].cmd_wait_for_cmpl;
        break;
    case TURING_TCS0_DRV0_CMD_WAIT_FOR_CMPL + 1 * 0x2a0:
        value = drv->tcs_states[1].cmd_wait_for_cmpl;
        break;
    case TURING_TCS0_DRV0_CMD_WAIT_FOR_CMPL + 2 * 0x2a0:
        value = drv->tcs_states[2].cmd_wait_for_cmpl;
        break;
    case TURING_TCS0_DRV0_CMD_WAIT_FOR_CMPL + 3 * 0x2a0:
        value = drv->tcs_states[3].cmd_wait_for_cmpl;
        break;
    case TURING_TCS0_DRV0_CMD_WAIT_FOR_CMPL + 4 * 0x2a0:
        value = drv->tcs_states[4].cmd_wait_for_cmpl;
        break;
    case TURING_TCS0_DRV0_CMD_WAIT_FOR_CMPL + 5 * 0x2a0:
        value = drv->tcs_states[5].cmd_wait_for_cmpl;
        break;
    case TURING_TCS0_DRV0_CONTROL:
        value = drv->tcs_states[0].control;
        break;
    case TURING_TCS0_DRV0_CONTROL + 1 * 0x2a0:
        value = drv->tcs_states[1].control;
        break;
    case TURING_TCS0_DRV0_CONTROL + 2 * 0x2a0:
        value = drv->tcs_states[2].control;
        break;
    case TURING_TCS0_DRV0_CONTROL + 3 * 0x2a0:
        value = drv->tcs_states[3].control;
        break;
    case TURING_TCS0_DRV0_CONTROL + 4 * 0x2a0:
        value = drv->tcs_states[4].control;
        break;
    case TURING_TCS0_DRV0_CONTROL + 5 * 0x2a0:
        value = drv->tcs_states[5].control;
        break;

    case TURING_TCS0_DRV0_CMD_ENABLE:
        value = drv->tcs_states[0].cmd_enable;
        break;
    case TURING_TCS0_DRV0_CMD_ENABLE + 1 * 0x2a0:
        value = drv->tcs_states[1].cmd_enable;
        break;
    case TURING_TCS0_DRV0_CMD_ENABLE + 2 * 0x2a0:
        value = drv->tcs_states[2].cmd_enable;
        break;
    case TURING_TCS0_DRV0_CMD_ENABLE + 3 * 0x2a0:
        value = drv->tcs_states[3].cmd_enable;
        break;
    case TURING_TCS0_DRV0_CMD_ENABLE + 4 * 0x2a0:
        value = drv->tcs_states[4].cmd_enable;
        break;
    case TURING_TCS0_DRV0_CMD_ENABLE + 5 * 0x2a0:
        value = drv->tcs_states[5].cmd_enable;
        break;

    /* Timeout registers */
    case TURING_TCS_TIMEOUT_VAL_DRV0:
        value = drv->tcs_timeout_val;
        break;

    /*
     * TCS Command registers - handle all TCS (m=0..5) and CMD (n=0..15)
     * combinations
     */
    default:
        /* Check if this is a TCS command register */
        if (offset >= TURING_TCS0_CMD0_DRV0_MSGID &&
            offset < TURING_TCS0_CMD0_DRV0_MSGID +
                         (TURING_RSC_MAX_TCS_PER_DRV * TURING_TCS_SPACING)) {
            /* Calculate TCS index (m) and command index (n) from offset */
            uint32_t tcs_base_offset = offset - TURING_TCS0_CMD0_DRV0_MSGID;
            uint32_t tcs_index = tcs_base_offset / TURING_TCS_SPACING;
            uint32_t cmd_offset_in_tcs = tcs_base_offset % TURING_TCS_SPACING;

            /*
             * Check if this is within the command register range for this TCS
             */
            if (cmd_offset_in_tcs <
                (TURING_RSC_MAX_CMDS_PER_TCS * TURING_CMD_SPACING)) {
                uint32_t cmd_index = cmd_offset_in_tcs / TURING_CMD_SPACING;
                uint32_t reg_offset_in_cmd =
                    cmd_offset_in_tcs % TURING_CMD_SPACING;

                /* Validate indices */
                if (tcs_index < TURING_RSC_MAX_TCS_PER_DRV &&
                    cmd_index < TURING_RSC_MAX_CMDS_PER_TCS) {
                    TuringRscCommand *cmd =
                        &drv->tcs_states[tcs_index].commands[cmd_index];

                    switch (reg_offset_in_cmd) {
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
                    case 0x10: /* read_response_data */
                        value = cmd->read_response_data;
                        break;
                    default:
                        qemu_log_mask(LOG_GUEST_ERROR,
                                      "Turing RSC: Invalid TCS command "
                                      "register offset 0x%x (TCS%d CMD%d)\n",
                                      offset, tcs_index, cmd_index);
                        break;
                    }
                    break; /* Exit the switch statement */
                }
            }
        }

        /* If we reach here, it's an unhandled register */
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
        /* Check if we need to trigger IRQ based on current status */
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

    /* TCS AMC Mode registers */
    case TURING_TCS_AMC_MODE_IRQ_ENABLE_DRV0:
        drv->tcs_amc_mode_irq_enable = val;
        /* Check if we need to trigger IRQ based on current status */
        if (drv->tcs_amc_mode_irq_status & drv->tcs_amc_mode_irq_enable) {
            qemu_set_irq(drv->amc_irq, 1);
        } else {
            qemu_set_irq(drv->amc_irq, 0);
        }
        break;
    case TURING_TCS_AMC_MODE_IRQ_CLEAR_DRV0:
        /* Clear bits in status register */
        drv->tcs_amc_mode_irq_status &= ~val;
        /* Deassert IRQ if no enabled status bits remain */
        if ((drv->tcs_amc_mode_irq_status & drv->tcs_amc_mode_irq_enable) == 0) {
            qemu_set_irq(drv->amc_irq, 0);
        }
        break;
    case TURING_RSC_ERROR_IRQ_CLEAR_DRV0:
        /* Clear bits in error status register */
        drv->error_irq_status &= ~val;
        /* Deassert IRQ if no enabled status bits remain */
        if ((drv->error_irq_status & drv->error_irq_enable) == 0) {
            qemu_set_irq(drv->error_irq, 0);
        }
        break;

    /* TCS Control registers */
    case TURING_TCS0_DRV0_CMD_WAIT_FOR_CMPL:
        drv->tcs_states[0].cmd_wait_for_cmpl = val;
        break;
    case TURING_TCS0_DRV0_CMD_WAIT_FOR_CMPL + 1 * 0x2a0:
        drv->tcs_states[1].cmd_wait_for_cmpl = val;
        break;
    case TURING_TCS0_DRV0_CMD_WAIT_FOR_CMPL + 2 * 0x2a0:
        drv->tcs_states[2].cmd_wait_for_cmpl = val;
        break;
    case TURING_TCS0_DRV0_CMD_WAIT_FOR_CMPL + 3 * 0x2a0:
        drv->tcs_states[3].cmd_wait_for_cmpl = val;
        break;
    case TURING_TCS0_DRV0_CMD_WAIT_FOR_CMPL + 4 * 0x2a0:
        drv->tcs_states[4].cmd_wait_for_cmpl = val;
        break;
    case TURING_TCS0_DRV0_CMD_WAIT_FOR_CMPL + 5 * 0x2a0:
        drv->tcs_states[5].cmd_wait_for_cmpl = val;
        break;

    case TURING_TCS0_DRV0_CONTROL:
        turing_rsc_handle_tcs_trigger(drv, 0, val);
        break;
    case TURING_TCS0_DRV0_CONTROL + 1 * 0x2a0:
        turing_rsc_handle_tcs_trigger(drv, 1, val);
        break;
    case TURING_TCS0_DRV0_CONTROL + 2 * 0x2a0:
        turing_rsc_handle_tcs_trigger(drv, 2, val);
        break;
    case TURING_TCS0_DRV0_CONTROL + 3 * 0x2a0:
        turing_rsc_handle_tcs_trigger(drv, 3, val);
        break;
    case TURING_TCS0_DRV0_CONTROL + 4 * 0x2a0:
        turing_rsc_handle_tcs_trigger(drv, 4, val);
        break;
    case TURING_TCS0_DRV0_CONTROL + 5 * 0x2a0:
        turing_rsc_handle_tcs_trigger(drv, 5, val);
        break;

    case TURING_TCS0_DRV0_CMD_ENABLE:
        drv->tcs_states[0].cmd_enable = val;
        break;
    case TURING_TCS0_DRV0_CMD_ENABLE + 1 * 0x2a0:
        drv->tcs_states[1].cmd_enable = val;
        break;
    case TURING_TCS0_DRV0_CMD_ENABLE + 2 * 0x2a0:
        drv->tcs_states[2].cmd_enable = val;
        break;
    case TURING_TCS0_DRV0_CMD_ENABLE + 3 * 0x2a0:
        drv->tcs_states[3].cmd_enable = val;
        break;
    case TURING_TCS0_DRV0_CMD_ENABLE + 4 * 0x2a0:
        drv->tcs_states[4].cmd_enable = val;
        break;
    case TURING_TCS0_DRV0_CMD_ENABLE + 5 * 0x2a0:
        drv->tcs_states[5].cmd_enable = val;
        break;

    /* Timeout registers */
    case TURING_TCS_TIMEOUT_VAL_DRV0:
        drv->tcs_timeout_val = val;
        break;

    /*
     * TCS Command registers - handle all TCS (m=0..5) and CMD (n=0..15)
     * combinations
     */
    default:
        /* Check if this is a TCS command register */
        if (offset >= TURING_TCS0_CMD0_DRV0_MSGID &&
            offset < TURING_TCS0_CMD0_DRV0_MSGID +
                         (TURING_RSC_MAX_TCS_PER_DRV * TURING_TCS_SPACING)) {
            /* Calculate TCS index (m) and command index (n) from offset */
            uint32_t tcs_base_offset = offset - TURING_TCS0_CMD0_DRV0_MSGID;
            uint32_t tcs_index = tcs_base_offset / TURING_TCS_SPACING;
            uint32_t cmd_offset_in_tcs = tcs_base_offset % TURING_TCS_SPACING;

            /* Check if this is within the command register range for this TCS */
            if (cmd_offset_in_tcs <
                (TURING_RSC_MAX_CMDS_PER_TCS * TURING_CMD_SPACING)) {
                uint32_t cmd_index = cmd_offset_in_tcs / TURING_CMD_SPACING;
                uint32_t reg_offset_in_cmd =
                    cmd_offset_in_tcs % TURING_CMD_SPACING;

                /* Validate indices */
                if (tcs_index < TURING_RSC_MAX_TCS_PER_DRV &&
                    cmd_index < TURING_RSC_MAX_CMDS_PER_TCS) {
                    TuringRscCommand *cmd =
                        &drv->tcs_states[tcs_index].commands[cmd_index];

                    switch (reg_offset_in_cmd) {
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
                        qemu_log_mask(
                            LOG_GUEST_ERROR,
                            "Turing RSC: Invalid write to Read-Only TCS status "
                            "register 0x%x (TCS%d CMD%d)\n",
                            offset, tcs_index, cmd_index);
                        break;
                    case 0x10:
                        qemu_log_mask(
                            LOG_GUEST_ERROR,
                            "Turing RSC: Invalid write to Read-Only TCS read "
                            "response data register 0x%x (TCS%d CMD%d)\n",
                            offset, tcs_index, cmd_index);
                        break;
                    default:
                        qemu_log_mask(
                            LOG_GUEST_ERROR,
                            "Turing RSC: Invalid TCS command register write "
                            "offset 0x%x (TCS%d CMD%d)\n",
                            offset, tcs_index, cmd_index);
                        break;
                    }
                    break; /* Exit the switch statement */
                }
            }
        }

        /* If we reach here, it's an unhandled register */
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

    /* Reset device state */
    s->version = (2 << 16) | (4 << 8) | 0; /* Version 2.4.0 */

    /* Reset driver */
    TuringRscDriverState *drv = &s->drivers[0];
    if (drv->present) {
        /* Initialize with reset values */
        drv->rsc_id = turing_rsc_reset_values[TURING_RSC_ID_DRV0];
        drv->solver_config =
            turing_rsc_reset_values[TURING_RSC_PARAM_SOLVER_CONFIG_DRV0];
        drv->rsc_config =
            turing_rsc_reset_values[TURING_RSC_PARAM_RSC_CONFIG_DRV0];
        drv->parentchild_config = turing_rsc_reset_values
            [TURING_RSC_PARAM_RSC_PARENTCHILD_CONFIG_DRV0];

        drv->status0 = turing_rsc_reset_values[TURING_RSC_STATUS0_DRV0];
        drv->status1 = turing_rsc_reset_values[TURING_RSC_STATUS1_DRV0];
        drv->status2 = turing_rsc_reset_values[TURING_RSC_STATUS2_DRV0];

        drv->hidden_tcs_ctrl =
            turing_rsc_reset_values[TURING_HIDDEN_TCS_CTRL_DRV0];
        drv->pdc_seq_start_addr_reg_offset =
            turing_rsc_reset_values[TURING_PDC_SEQ_START_ADDR_REG_OFFSET_DRV0];
        drv->pdc_match_value_lo_reg_offset =
            turing_rsc_reset_values[TURING_PDC_MATCH_VALUE_LO_REG_OFFSET_DRV0];
        drv->pdc_match_value_hi_reg_offset =
            turing_rsc_reset_values[TURING_PDC_MATCH_VALUE_HI_REG_OFFSET_DRV0];
        drv->pdc_slave_id = turing_rsc_reset_values[TURING_PDC_SLAVE_ID_DRV0];
        drv->hidden_tcs_status =
            turing_rsc_reset_values[TURING_HIDDEN_TCS_STATUS_DRV0];

        drv->hidden_tcs_cmds[0].addr =
            turing_rsc_reset_values[TURING_HIDDEN_TCS_CMD0_ADDR_DRV0];
        drv->hidden_tcs_cmds[0].data =
            turing_rsc_reset_values[TURING_HIDDEN_TCS_CMD0_DATA_DRV0];
        drv->hidden_tcs_cmds[1].addr =
            turing_rsc_reset_values[TURING_HIDDEN_TCS_CMD1_ADDR_DRV0];
        drv->hidden_tcs_cmds[1].data =
            turing_rsc_reset_values[TURING_HIDDEN_TCS_CMD1_DATA_DRV0];
        drv->hidden_tcs_cmds[2].addr =
            turing_rsc_reset_values[TURING_HIDDEN_TCS_CMD2_ADDR_DRV0];
        drv->hidden_tcs_cmds[2].data =
            turing_rsc_reset_values[TURING_HIDDEN_TCS_CMD2_DATA_DRV0];

        drv->secure_override =
            turing_rsc_reset_values[TURING_RSC_SECURE_OVERRIDE_DRV0];
        drv->tcs_timeout_val =
            turing_rsc_reset_values[TURING_TCS_TIMEOUT_VAL_DRV0];

        /* Extract configuration from parentchild config */
        drv->num_tcs = (drv->parentchild_config >>
                        TURING_PARENTCHILD_CONFIG_NUM_TCS_DRV0_SHIFT) &
                       TURING_PARENTCHILD_CONFIG_NUM_TCS_DRV0_MASK;
        drv->cmds_per_tcs = (drv->parentchild_config >>
                             TURING_PARENTCHILD_CONFIG_NUM_CMDS_PER_TCS_SHIFT) &
                            (TURING_PARENTCHILD_CONFIG_NUM_CMDS_PER_TCS_MASK >>
                             TURING_PARENTCHILD_CONFIG_NUM_CMDS_PER_TCS_SHIFT);

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

        /* Clear arrays */
        memset(drv->hw_event_mux_select, 0, sizeof(drv->hw_event_mux_select));
        memset(drv->timestamp_units, 0, sizeof(drv->timestamp_units));
        memset(drv->seq_cfg_delay_val, 0, sizeof(drv->seq_cfg_delay_val));
        memset(drv->seq_cfg_br_addr, 0, sizeof(drv->seq_cfg_br_addr));
        memset(drv->seq_mem, 0, sizeof(drv->seq_mem));
    }
}

static const Property turing_rsc_properties[] = {
    DEFINE_PROP_UINT32("num-drivers", TuringRscState, num_drivers, 1),
};

static void turing_rsc_realize(DeviceState *dev, Error **errp)
{
    TuringRscState *s = TURING_RSC(dev);
    SysBusDevice *sbd = SYS_BUS_DEVICE(dev);

    /* Clamp number of drivers */
    if (s->num_drivers > TURING_RSC_MAX_DRIVERS) {
        s->num_drivers = TURING_RSC_MAX_DRIVERS;
    }

    /* Initialize driver */
    TuringRscDriverState *drv = &s->drivers[0];
    drv->present = true;
    drv->driver_id = 0;

    /* Initialize driver registers with reset values */
    drv->rsc_id = turing_rsc_reset_values[TURING_RSC_ID_DRV0];
    drv->solver_config =
        turing_rsc_reset_values[TURING_RSC_PARAM_SOLVER_CONFIG_DRV0];
    drv->rsc_config = turing_rsc_reset_values[TURING_RSC_PARAM_RSC_CONFIG_DRV0];
    drv->parentchild_config =
        turing_rsc_reset_values[TURING_RSC_PARAM_RSC_PARENTCHILD_CONFIG_DRV0];

    /* Extract configuration from parentchild config */
    drv->num_tcs = (drv->parentchild_config >>
                    TURING_PARENTCHILD_CONFIG_NUM_TCS_DRV0_SHIFT) &
                   TURING_PARENTCHILD_CONFIG_NUM_TCS_DRV0_MASK;
    drv->cmds_per_tcs = (drv->parentchild_config >>
                         TURING_PARENTCHILD_CONFIG_NUM_CMDS_PER_TCS_SHIFT) &
                        (TURING_PARENTCHILD_CONFIG_NUM_CMDS_PER_TCS_MASK >>
                         TURING_PARENTCHILD_CONFIG_NUM_CMDS_PER_TCS_SHIFT);

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
    .version_id = 1,
    .minimum_version_id = 1,
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
            VMSTATE_UINT32(cmds_per_tcs, TuringRscDriverState),
            VMSTATE_STRUCT_ARRAY(tcs_states, TuringRscDriverState,
                                 TURING_RSC_MAX_TCS_PER_DRV, 0,
                                 vmstate_turing_rsc_tcs, TuringRscTcsState),
            VMSTATE_END_OF_LIST() }
};

static const VMStateDescription vmstate_turing_rsc = {
    .name = "turing-rsc",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]){ VMSTATE_UINT32(version, TuringRscState),
                                VMSTATE_UINT32(num_drivers, TuringRscState),
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
