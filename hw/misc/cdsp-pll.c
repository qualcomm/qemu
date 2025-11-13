/*
 * CDSP PLL device
 *
 * Copyright(c) 2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "hw/misc/cdsp-pll.h"
#include "hw/core/qdev-properties.h"
#include "hw/core/sysbus.h"
#include "migration/vmstate.h"
#include "qemu/log.h"
#include "qemu/timer.h"
#include "trace.h"

/* Register index mapping */
enum {
    PLL_MODE_IDX = 0,
    PLL_L_VAL_IDX,
    PLL_CAL_L_VAL_IDX,
    PLL_USER_CTL_IDX,
    PLL_USER_CTL_U_IDX,
    PLL_CONFIG_CTL_IDX,
    PLL_CONFIG_CTL_U_IDX,
    PLL_TEST_CTL_IDX,
    PLL_TEST_CTL_U_IDX,
    PLL_STATUS_IDX,
    PLL_FREQ_CTL_IDX,
    PLL_OPMODE_IDX,
    PLL_STATE_IDX,
    PLL_DROOP_IDX,
    PLL_SPARE_IDX,
    PLL_SSC_DELTA_ALPHA_IDX,
    PLL_SSC_UPDATE_RATE_IDX,
    PLL_SSC_NUM_STEPS_IDX,
    PLL_RCG_UPDATE_STATUS_IDX,
    PLL_RCG_UPDATE_CFG_IDX,
    PLL_RCG_UPDATE_DLYCTL_IDX,
    PLL_RCG_UPDATE_CMD_IDX,
    CORE_CFG_RCGR_IDX,
    CORE_CMD_RCGR_IDX
};

/* Register offset to index mapping */
static const struct {
    hwaddr offset;
    int index;
} register_map[] = {
    { CDSP_PLL_MODE, PLL_MODE_IDX },
    { CDSP_PLL_L_VAL, PLL_L_VAL_IDX },
    { CDSP_PLL_CAL_L_VAL, PLL_CAL_L_VAL_IDX },
    { CDSP_PLL_USER_CTL, PLL_USER_CTL_IDX },
    { CDSP_PLL_USER_CTL_U, PLL_USER_CTL_U_IDX },
    { CDSP_PLL_CONFIG_CTL, PLL_CONFIG_CTL_IDX },
    { CDSP_PLL_CONFIG_CTL_U, PLL_CONFIG_CTL_U_IDX },
    { CDSP_PLL_TEST_CTL, PLL_TEST_CTL_IDX },
    { CDSP_PLL_TEST_CTL_U, PLL_TEST_CTL_U_IDX },
    { CDSP_PLL_STATUS, PLL_STATUS_IDX },
    { CDSP_PLL_FREQ_CTL, PLL_FREQ_CTL_IDX },
    { CDSP_PLL_OPMODE, PLL_OPMODE_IDX },
    { CDSP_PLL_STATE, PLL_STATE_IDX },
    { CDSP_PLL_DROOP, PLL_DROOP_IDX },
    { CDSP_PLL_SPARE, PLL_SPARE_IDX },
    { CDSP_PLL_SSC_DELTA_ALPHA, PLL_SSC_DELTA_ALPHA_IDX },
    { CDSP_PLL_SSC_UPDATE_RATE, PLL_SSC_UPDATE_RATE_IDX },
    { CDSP_PLL_SSC_NUM_STEPS, PLL_SSC_NUM_STEPS_IDX },
    { CDSP_PLL_RCG_UPDATE_STATUS, PLL_RCG_UPDATE_STATUS_IDX },
    { CDSP_PLL_RCG_UPDATE_CFG, PLL_RCG_UPDATE_CFG_IDX },
    { CDSP_PLL_RCG_UPDATE_DLYCTL, PLL_RCG_UPDATE_DLYCTL_IDX },
    { CDSP_PLL_RCG_UPDATE_CMD, PLL_RCG_UPDATE_CMD_IDX },
    { CDSP_PLL_CORE_CFG_RCGR, CORE_CFG_RCGR_IDX },
    { CDSP_PLL_CORE_CMD_RCGR, CORE_CMD_RCGR_IDX },
};

static int offset_to_index(hwaddr offset)
{
    for (int i = 0; i < ARRAY_SIZE(register_map); i++) {
        if (register_map[i].offset == offset) {
            return register_map[i].index;
        }
    }
    return -1;
}

static void cdsp_pll_update_lock_status(CdspPLLState *s)
{
    uint32_t mode = s->regs[PLL_MODE_IDX];
    uint32_t opmode = s->regs[PLL_OPMODE_IDX];

    /*
     * PLL locks when:
     * - Reset is deasserted (RESET_N = 1)
     * - Operation mode is RUN (1)
     */
    bool should_lock = (mode & CDSP_PLL_MODE_RESET_N) &&
                       (opmode == CDSP_PLL_OPMODE_RUN);

    if (should_lock && !s->pll_locked) {
        s->pll_locked = true;
        s->regs[PLL_MODE_IDX] |= CDSP_PLL_MODE_LOCK_DET;
        trace_cdsp_pll_locked();
    } else if (!should_lock && s->pll_locked) {
        s->pll_locked = false;
        s->regs[PLL_MODE_IDX] &= ~CDSP_PLL_MODE_LOCK_DET;
        trace_cdsp_pll_unlocked();
    }
}

static void cdsp_pll_update_outputs(CdspPLLState *s)
{
    uint32_t mode = s->regs[PLL_MODE_IDX];
    uint32_t user_ctl = s->regs[PLL_USER_CTL_IDX];

    /*
     * Outputs enabled when:
     * - Global output control is enabled (OUTCTRL = 1)
     * - Main output is enabled (PLLOUT_MAIN = 1)
     * - PLL is locked
     */
    s->outputs_enabled = (mode & CDSP_PLL_MODE_OUTCTRL) &&
                        (user_ctl & CDSP_PLL_USER_CTL_PLLOUT_MAIN) &&
                        s->pll_locked;
}

static uint64_t cdsp_pll_read(void *opaque, hwaddr offset, unsigned size)
{
    CdspPLLState *s = CDSP_PLL(opaque);
    int reg_idx = offset_to_index(offset);
    uint32_t value = 0;

    if (reg_idx >= 0 && reg_idx < CDSP_PLL_NUM_REGS) {
        value = s->regs[reg_idx];
    } else {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "cdsp-pll: Bad read offset 0x%x\n", (int)offset);
    }

    trace_cdsp_pll_read(offset, value);
    return value;
}

static void cdsp_pll_write(void *opaque, hwaddr offset, uint64_t value,
                             unsigned size)
{
    CdspPLLState *s = CDSP_PLL(opaque);
    int reg_idx = offset_to_index(offset);
    uint32_t old_value;

    trace_cdsp_pll_write(offset, (uint32_t)value);

    if (reg_idx < 0 || reg_idx >= CDSP_PLL_NUM_REGS) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "cdsp-pll: Bad write offset 0x%x\n", (int)offset);
        return;
    }

    old_value = s->regs[reg_idx];
    s->regs[reg_idx] = value;

    /* Handle special register behaviors */
    switch (reg_idx) {
    case PLL_MODE_IDX:
        /* Handle latch update mechanism */
        if ((value & CDSP_PLL_MODE_UPDATE) &&
            !(old_value & CDSP_PLL_MODE_UPDATE)) {
            /* Latch L_VAL into CAL_L_VAL and acknowledge */
            s->regs[PLL_CAL_L_VAL_IDX] = s->regs[PLL_L_VAL_IDX];
            s->regs[PLL_MODE_IDX] |= CDSP_PLL_MODE_ACK_LATCH;
        }
        if (!(value & CDSP_PLL_MODE_UPDATE) &&
            (old_value & CDSP_PLL_MODE_UPDATE)) {
            /* Clear ack when update bit is cleared */
            s->regs[PLL_MODE_IDX] &= ~CDSP_PLL_MODE_ACK_LATCH;
        }
        cdsp_pll_update_lock_status(s);
        cdsp_pll_update_outputs(s);
        break;

    case PLL_OPMODE_IDX:
        cdsp_pll_update_lock_status(s);
        break;

    case PLL_USER_CTL_IDX:
        cdsp_pll_update_outputs(s);
        break;

    case CORE_CMD_RCGR_IDX:
        /* Simulate RCG command completion */
        if (value & 1) {
            /* Command bit auto-clears when operation completes */
            s->regs[CORE_CMD_RCGR_IDX] &= ~1;
        }
        break;

    default:
        break;
    }
}

static const MemoryRegionOps cdsp_pll_ops = {
    .read = cdsp_pll_read,
    .write = cdsp_pll_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
};

static void cdsp_pll_reset(DeviceState *dev)
{
    CdspPLLState *s = CDSP_PLL(dev);

    memset(s->regs, 0, sizeof(s->regs));

    /* Initialize with default configuration values */
    s->regs[PLL_CONFIG_CTL_IDX] = HAL_CLK_UPDATED_CONFIG_CTL_VAL;
    s->regs[PLL_CONFIG_CTL_U_IDX] = HAL_CLK_UPDATED_CONFIG_CTL_U_VAL;
    s->regs[PLL_TEST_CTL_IDX] = HAL_CLK_UPDATED_TEST_CTL_VAL;
    s->regs[PLL_TEST_CTL_U_IDX] = HAL_CLK_UPDATED_TEST_CTL_U_VAL;
    s->regs[PLL_USER_CTL_U_IDX] = HAL_CLK_UPDATED_USER_CTL_U_VAL;
    s->regs[PLL_L_VAL_IDX] = s->default_l_val;
    s->regs[PLL_CAL_L_VAL_IDX] = s->default_l_val;

    /* PLL starts in reset state */
    s->regs[PLL_MODE_IDX] = 0; /* RESET_N = 0 */
    s->regs[PLL_OPMODE_IDX] = CDSP_PLL_OPMODE_STANDBY;

    s->pll_locked = false;
    s->outputs_enabled = false;
}

static void cdsp_pll_instance_init(Object *obj)
{
    CdspPLLState *s = CDSP_PLL(obj);

    /* Set default values for properties */
    s->base_freq = 19200000;
    s->default_l_val = 62;
}

static void cdsp_pll_realize(DeviceState *dev, Error **errp)
{
    CdspPLLState *s = CDSP_PLL(dev);
    SysBusDevice *sbd = SYS_BUS_DEVICE(dev);

    memory_region_init_io(&s->iomem, OBJECT(s), &cdsp_pll_ops, s,
                          TYPE_CDSP_PLL, 0x1000);
    sysbus_init_mmio(sbd, &s->iomem);
}

static const VMStateDescription vmstate_cdsp_pll = {
    .name = "cdsp-pll",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (const VMStateField[]) {
        VMSTATE_UINT32_ARRAY(regs, CdspPLLState, CDSP_PLL_NUM_REGS),
        VMSTATE_BOOL(pll_locked, CdspPLLState),
        VMSTATE_BOOL(outputs_enabled, CdspPLLState),
        VMSTATE_END_OF_LIST()
    }
};

static const Property cdsp_pll_properties[] = {
    DEFINE_PROP_UINT32("base-freq", CdspPLLState, base_freq, 19200000),
    DEFINE_PROP_UINT32("default-l-val", CdspPLLState, default_l_val, 62),
};

static void cdsp_pll_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = cdsp_pll_realize;
    device_class_set_legacy_reset(dc, cdsp_pll_reset);
    dc->vmsd = &vmstate_cdsp_pll;
    device_class_set_props(dc, cdsp_pll_properties);
    dc->desc = "CDSP PLL";
}

static const TypeInfo cdsp_pll_info = {
    .name = TYPE_CDSP_PLL,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(CdspPLLState),
    .instance_init = cdsp_pll_instance_init,
    .class_init = cdsp_pll_class_init,
};

static void cdsp_pll_register_types(void)
{
    type_register_static(&cdsp_pll_info);
}

type_init(cdsp_pll_register_types)
