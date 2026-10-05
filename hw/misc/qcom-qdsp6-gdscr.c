/*
 * QCOM Turing GDSCR Registers — QEMU sysbus device
 * GDSCR: Global Distributed Switch Controller Regulator
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "hw/core/resettable.h"
#include "hw/misc/qcom-qdsp6-gdscr.h"
#include "migration/vmstate.h"

/* ------------------------------------------------------------------------- */
/* Read */
static uint64_t gdscr_read(void *opaque, hwaddr addr, unsigned size)
{
    QcomTuringGdscrState *s = opaque;

    if (size != 4) {
        return 0;
    }

    switch (addr) {
    case 0:
        return s->values[0];
    case 4:
        return s->values[1];
    default:
        return 0;
    }
}

/* ------------------------------------------------------------------------- */
/* Write */
static void gdscr_write(void *opaque, hwaddr addr, uint64_t value,
                        unsigned size)
{
    QcomTuringGdscrState *s = opaque;
    uint32_t v = (uint32_t)value;

    if (size != 4) {
        return;
    }

    switch (addr) {
    case 0:
        if (v & 0x1) {
            v &= ~0x80000000;
        } else {
            v |= 0x80000000;
        }
        s->values[0] = v;
        break;
    case 4:
        s->values[1] = v;
        break;
    default:
        break;
    }
}

/* ------------------------------------------------------------------------- */
/* MemoryRegionOps */
static const MemoryRegionOps gdscr_ops = {
    .read = gdscr_read,
    .write = gdscr_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = { .min_access_size = 4, .max_access_size = 4 },
};

/* ------------------------------------------------------------------------- */
/* Reset */
static void gdscr_reset_hold(Object *obj, ResetType type)
{
    QcomTuringGdscrState *s = QCOM_GDSCR(obj);

    s->values[0] = 0x0022f001;
    s->values[1] = 0x98000;
}

/* ------------------------------------------------------------------------- */
/* Realize */
static void gdscr_realize(DeviceState *dev, Error **errp)
{
    QcomTuringGdscrState *s = QCOM_GDSCR(dev);

    memory_region_init_io(&s->mmio, OBJECT(dev), &gdscr_ops, s, TYPE_QCOM_GDSCR,
                          QCOM_GDSCR_MMIO_SIZE);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->mmio);
}

static const VMStateDescription vmstate_qdsp6_gdscr = {
    .name = "qdsp6ss-gdscr",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_UINT32_ARRAY(values, QcomTuringGdscrState, 2),
        VMSTATE_END_OF_LIST()
    }

};
/* ------------------------------------------------------------------------- */
/* Type info */
static void gdscr_class_init(ObjectClass *oc, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);
    ResettableClass *rc = RESETTABLE_CLASS(oc);

    dc->realize = gdscr_realize;
    rc->phases.hold = gdscr_reset_hold;
    dc->vmsd = &vmstate_qdsp6_gdscr;
    dc->desc = "QCOM Turing GDSCR Registers";
}

static const TypeInfo gdscr_info = {
    .name = TYPE_QCOM_GDSCR,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(QcomTuringGdscrState),
    .class_init = gdscr_class_init,
};

static void gdscr_register_types(void)
{
    type_register_static(&gdscr_info);
}

type_init(gdscr_register_types)
