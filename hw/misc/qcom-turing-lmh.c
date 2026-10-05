/*
 * QCOM Turing QDSP6SS register block
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "hw/core/resettable.h"
#include "hw/core/sysbus.h"
#include "hw/misc/qcom-turing-lmh.h"
#include "qemu/module.h"

static uint64_t turing_lmh_read(void *opaque, hwaddr offset, unsigned size)
{
    TuringLmhState *s = opaque;
    switch (offset) {
    case 0:
        return s->values[0];
    case 4:
        return s->values[1];
    default:
        return 0;
    }
}

static void turing_lmh_write(void *opaque, hwaddr offset, uint64_t val,
                              unsigned size)
{
    TuringLmhState *s = opaque;
    switch (offset) {
    case 0:
        s->values[0] = (uint32_t)val;
        s->values[1] = s->values[0] & 1;
        break;
    default:
        break;
    }
}

static const MemoryRegionOps turing_lmh_ops = {
    .read = turing_lmh_read,
    .write = turing_lmh_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .impl = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
};

static void turing_lmh_reset_hold(Object *obj, ResetType type)
{
    TuringLmhState *s = TURING_LMH(obj);

    s->values[0] = 1;
    s->values[1] = 1;
}

static void turing_lmh_realize(DeviceState *dev, Error **errp)
{
    TuringLmhState *s = TURING_LMH(dev);

    memory_region_init_io(&s->iomem, OBJECT(s), &turing_lmh_ops, s,
                          TYPE_TURING_LMH, TURING_LMH_SIZE);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->iomem);
}

static void turing_lmh_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    ResettableClass *rc = RESETTABLE_CLASS(klass);

    dc->realize = turing_lmh_realize;
    rc->phases.hold = turing_lmh_reset_hold;
    dc->desc = "QCOM Turing register block";
}

static const TypeInfo turing_lmh_info = {
    .name = TYPE_TURING_LMH,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(TuringLmhState),
    .class_init = turing_lmh_class_init,
};

static void turing_lmh_register_types(void)
{
    type_register_static(&turing_lmh_info);
}

type_init(turing_lmh_register_types);
