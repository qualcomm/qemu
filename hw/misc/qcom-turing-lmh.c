/*
 * QCOM Turing QDSP6SS register block
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "hw/core/resettable.h"
#include "hw/core/sysbus.h"
#include "hw/misc/qcom-turing-lmh.h"
#include "migration/vmstate.h"
#include "qemu/log.h"
#include "qemu/module.h"

#define REG0 0x0
#define REG1 0x4

static uint64_t turing_lmh_read(void *opaque, hwaddr offset, unsigned size)
{
    TuringLmhState *s = opaque;

    switch (offset) {
    case REG0:
        return s->reg0;
    case REG1:
        return s->reg1;
    default:
        qemu_log_mask(LOG_GUEST_ERROR,
                      "turing-lmh: invalid read at offset 0x%" HWADDR_PRIx "\n",
                      offset);
        return 0;
    }
}

static void turing_lmh_write(void *opaque, hwaddr offset, uint64_t val,
                              unsigned size)
{
    TuringLmhState *s = opaque;

    switch (offset) {
    case REG0:
        s->reg0 = (uint32_t)val;
        s->reg1 = s->reg0 & BIT(0);
        break;
    case REG1:
        qemu_log_mask(LOG_GUEST_ERROR,
                      "turing-lmh: write to read-only reg1\n");
        break;
    default:
        qemu_log_mask(LOG_GUEST_ERROR,
                      "turing-lmh: invalid write at offset 0x%" HWADDR_PRIx "\n",
                      offset);
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

    s->reg0 = 0x1;
    s->reg1 = 0x1;
}

static void turing_lmh_realize(DeviceState *dev, Error **errp)
{
    TuringLmhState *s = TURING_LMH(dev);

    memory_region_init_io(&s->iomem, OBJECT(s), &turing_lmh_ops, s,
                          TYPE_TURING_LMH, TURING_LMH_SIZE);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->iomem);
}

static const VMStateDescription vmstate_turing_lmh = {
    .name = "turing-lmh",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_UINT32(reg0, TuringLmhState),
        VMSTATE_UINT32(reg1, TuringLmhState),
        VMSTATE_END_OF_LIST()
    }
};

static void turing_lmh_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    ResettableClass *rc = RESETTABLE_CLASS(klass);

    dc->realize = turing_lmh_realize;
    dc->vmsd = &vmstate_turing_lmh;
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
