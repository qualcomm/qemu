/*
 * TURING QDSP6SS PLL — simple QEMU PLL device
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Add Lucid PLL register set.
 *   Lucid is a general purpose PLL.
 */

#include "qemu/osdep.h"
#include "hw/core/boards.h"
#include "hw/core/irq.h"
#include "hw/misc/qcom-turing-cdsp-pll.h"
#include "migration/vmstate.h"

#define TURING_QDSP6SS_PLL_MMIO_SIZE 0x2C

static uint64_t turing_qdsp6ss_pll_read(void *opaque, hwaddr addr,
                                        unsigned size)
{
    TuringQdsp6PllState *s = (TuringQdsp6PllState *)opaque;

    if (size != 4 || addr > 0x28 || (addr & 3)) {
        return 0;
    }

    return s->values[addr / 4];
}

static void turing_qdsp6ss_pll_write(void *opaque, hwaddr addr, uint64_t value,
                                     unsigned size)
{
    TuringQdsp6PllState *s = (TuringQdsp6PllState *)opaque;
    uint32_t v = (uint32_t)value;

    if (size != 4 || addr > 0x28 || (addr & 3)) {
        return;
    }

    switch (addr) {
    case 0x08:
    case 0x0C:
        /* Read-only */
        break;
    case 0x18:
        s->values[0x18 / 4] = v;
        if (v & 1) {
            s->values[0] |= (1u << 2) | (1u << 31);
        }
        break;
    default:
        s->values[addr / 4] = v;
        break;
    }
}

static const MemoryRegionOps turing_qdsp6ss_pll_ops = {
    .read = turing_qdsp6ss_pll_read,
    .write = turing_qdsp6ss_pll_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
};

static void turing_qdsp6ss_pll_reset_hold(Object *obj, ResetType type)
{
    TuringQdsp6PllState *s = TURING_QDSP6SS_PLL(obj);
    static const uint32_t reset[11] = {
        0x00000100, 0x00000000, 0x00000004, 0x16000000, 0x00000010,
        0x00000000, 0x00000001, 0x00000805, 0x20485699, 0x00002261,
        0x02AA699C,
    };

    memcpy(s->values, reset, sizeof(reset));
}

static void turing_qdsp6ss_pll_realize(DeviceState *dev, Error **errp)
{
    TuringQdsp6PllState *s = TURING_QDSP6SS_PLL(dev);

    memory_region_init_io(&s->mmio, OBJECT(dev), &turing_qdsp6ss_pll_ops, s,
                          TYPE_TURING_QDSP6SS_PLL,
                          TURING_QDSP6SS_PLL_MMIO_SIZE);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->mmio);
}

static const VMStateDescription vmstate_qdsp6ss_pll = {
    .name = "qdsp6ss-pll",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_UINT32_ARRAY(values, TuringQdsp6PllState, 11),
        VMSTATE_END_OF_LIST()
    }
};

static void turing_qdsp6ss_pll_class_init(ObjectClass *oc, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);
    ResettableClass *rc = RESETTABLE_CLASS(oc);
    dc->realize = turing_qdsp6ss_pll_realize;
    dc->vmsd = &vmstate_qdsp6ss_pll;
    rc->phases.hold = turing_qdsp6ss_pll_reset_hold;
    dc->desc = "Turing QDSP6SS PLL";
}

static const TypeInfo turing_qdsp6ss_pll_info = {
    .name = TYPE_TURING_QDSP6SS_PLL,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(TuringQdsp6PllState),
    .class_init = turing_qdsp6ss_pll_class_init,
};

static void turing_qdsp6ss_pll_register_types(void)
{
    type_register_static(&turing_qdsp6ss_pll_info);
}

type_init(turing_qdsp6ss_pll_register_types);
