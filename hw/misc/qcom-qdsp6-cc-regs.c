/*
 * TURING QDSP6SS Clock Controller (CLKCTL) — QEMU sysbus device
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "hw/core/resettable.h"
#include "hw/misc/qcom-qdsp6-cc-regs.h"
#include "migration/vmstate.h"
#include "qemu/log.h"

static inline void clkctl_bad_access(const char *dir, hwaddr addr,
                                     unsigned size)
{
    qemu_log_mask(LOG_GUEST_ERROR,
                  TYPE_QDSP6SS_CLKCTL ": bad %s addr=0x%" HWADDR_PRIx
                                      " size=%u\n",
                  dir, addr, size);
}

/* ------------------------------------------------------------------------- */
/* Read                                                                      */
/* ------------------------------------------------------------------------- */
static uint64_t clkctl_read(void *opaque, hwaddr addr, unsigned size)
{
    TuringQdsp6ClkctlState *s = opaque;
    if (size != 4) {
        clkctl_bad_access("read", addr, size);
        return 0;
    }

    switch (addr) {
    case 0x20:
        if (s->values[0] & 1) {
            s->values[0] &= ~1;
        }
        return s->values[0];
    case 0x24:
        return s->values[1];

    case 0x40:
        return s->values[2];
    case 0x100:
        return s->values[3];

    case 0xe0:
        return s->values[4];
    case 0x1a8:
        return s->values[5];

    default:
        clkctl_bad_access("read", addr, size);
        return 0;
    }
}

/* ------------------------------------------------------------------------- */
/* Write                                                                     */
/* ------------------------------------------------------------------------- */
static void clkctl_write(void *opaque, hwaddr addr, uint64_t value,
                         unsigned size)
{
    TuringQdsp6ClkctlState *s = opaque;
    uint32_t v = (uint32_t)value;

    if (size != 4) {
        clkctl_bad_access("write", addr, size);
        return;
    }

    switch (addr) {
    case 0x20:
        s->values[0] = v;
        break;
    case 0x24:
        s->values[1] = v;
        break;

    case 0x40:
        s->values[2] = v;
        break;
    case 0x100:
        s->values[3] = v;
        break;

    case 0xe0:
        s->values[4] = v;
        break;
    case 0x1a8:
        s->values[5] = v;
        break;

    default:
        clkctl_bad_access("write", addr, size);
        break;
    }
}

static const MemoryRegionOps clkctl_ops = {
    .read = clkctl_read,
    .write = clkctl_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = { .min_access_size = 4, .max_access_size = 4 },
};

/* ------------------------------------------------------------------------- */
/* Reset                                                                     */
/* ------------------------------------------------------------------------- */
static void clkctl_reset_hold(Object *obj, ResetType type)
{
    TuringQdsp6ClkctlState *s = QDSP6SS_CLKCTL(obj);

    s->values[0] = 0x80000000;
    s->values[1] = 0x00100000;
    s->values[2] = 0x00000002;
    s->values[3] = 0x00000002;
    s->values[4] = 0;
    s->values[5] = 0;
}

/* ------------------------------------------------------------------------- */
/* Realize                                                                   */
/* ------------------------------------------------------------------------- */
static void clkctl_realize(DeviceState *dev, Error **errp)
{
    TuringQdsp6ClkctlState *s = QDSP6SS_CLKCTL(dev);

    memory_region_init_io(&s->mmio, OBJECT(dev), &clkctl_ops, s,
                          TYPE_QDSP6SS_CLKCTL, QDSP6SS_CLKCTL_MMIO_SIZE);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->mmio);
}

static const VMStateDescription vmstate_qdsp6_cc_regs = {
    .name = "qdsp6ss-cc-regs",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_UINT32_ARRAY(values, TuringQdsp6ClkctlState, 6),
        VMSTATE_END_OF_LIST()
    }
};

/* ------------------------------------------------------------------------- */
/* Type info                                                                 */
/* ------------------------------------------------------------------------- */
static void clkctl_class_init(ObjectClass *oc, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);
    ResettableClass *rc = RESETTABLE_CLASS(oc);
    dc->realize = clkctl_realize;
    dc->vmsd = &vmstate_qdsp6_cc_regs;
    rc->phases.hold = clkctl_reset_hold;
    dc->desc = "Turing QDSP6SS Clock Controller (CLKCTL)";
}

static const TypeInfo clkctl_info = {
    .name = TYPE_QDSP6SS_CLKCTL,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(TuringQdsp6ClkctlState),
    .class_init = clkctl_class_init,
};

static void clkctl_register_types(void)
{
    type_register_static(&clkctl_info);
}

type_init(clkctl_register_types)
