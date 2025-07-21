/*
 * Hexagon Global System Registers
 *
 * Copyright(c) 2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "hw/hexagon/hexagon_sysreg.h"
#include "hw/qdev-properties.h"
#include "migration/vmstate.h"
#include "qom/object.h"
#include "target/hexagon/cpu.h"
#include "target/hexagon/hex_regs.h"

static void hexagon_sysreg_init(Object *obj)
{
    HexagonSysregState *s = HEXAGON_SYSREG(obj);

    memset(s->regs, 0, sizeof(target_ulong) * NUM_SREGS);
}

uint32_t hexagon_sysreg_read(HexagonSysregState *s, uint32_t reg)
{
    g_assert(reg < NUM_SREGS);
    g_assert(reg >= HEX_SREG_GLB_START);
    return s->regs[reg];
}

void hexagon_sysreg_write(HexagonSysregState *s, uint32_t reg, uint32_t value)
{
    g_assert(reg < NUM_SREGS);
    g_assert(reg >= HEX_SREG_GLB_START);
    s->regs[reg] = value;
}

void hexagon_sysreg_reset(HexagonSysregState *s)
{
    g_assert(s);
    memset(s->regs, 0, sizeof(target_ulong) * NUM_SREGS);
}

static void hexagon_sysreg_reset_device(DeviceState *dev)
{
    HexagonSysregState *s = HEXAGON_SYSREG(dev);
    hexagon_sysreg_reset(s);
}

static const VMStateDescription vmstate_hexagon_sysreg = {
    .name = "hexagon_sysreg",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (const VMStateField[]){ VMSTATE_UINT32_ARRAY(
                                          regs, HexagonSysregState, NUM_SREGS),
                                      VMSTATE_END_OF_LIST() }
};

static void hexagon_sysreg_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    device_class_set_legacy_reset(dc, hexagon_sysreg_reset_device);
    dc->vmsd = &vmstate_hexagon_sysreg;
    dc->user_creatable = false;
}

static const TypeInfo hexagon_sysreg_info = {
    .name = TYPE_HEXAGON_SYSREG,
    .parent = TYPE_DEVICE,
    .instance_size = sizeof(HexagonSysregState),
    .instance_init = hexagon_sysreg_init,
    .class_init = hexagon_sysreg_class_init,
};

static void hexagon_sysreg_register_types(void)
{
    type_register_static(&hexagon_sysreg_info);
}

type_init(hexagon_sysreg_register_types)
