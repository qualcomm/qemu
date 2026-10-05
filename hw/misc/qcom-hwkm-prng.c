/*
 * QEMU device: HWKM PRNG emulator
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Hardware Key Manager Pseudo Random Number (hwkm_prng)
 */

#include "qemu/osdep.h"
#include "hw/core/resettable.h"
#include "hw/core/sysbus.h"
#include "hw/misc/qcom-hwkm-prng.h"
#include "qemu/guest-random.h"
#include "qemu/module.h"

static uint64_t hwkm_prng_read(void *opaque, hwaddr offset, unsigned size)
{
    HwkmPrngState *s = opaque;

    switch (offset) {
    case 0:
        if (s->flags[0]) {
            uint32_t val = s->values[0];
            uint32_t rnd;

            if (qemu_guest_getrandom(&rnd, sizeof(uint32_t), NULL) == 0) {
                s->values[0] = rnd;
            }
            return val;
        }
        return 0;
    case 4:
        return s->flags[0] ? 0x1f : 0x1e;
    default:
        return 0;
    }
}

static const MemoryRegionOps hwkm_prng_ops = {
    .read = hwkm_prng_read,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

static void hwkm_prng_reset_hold(Object *obj, ResetType type)
{
    HwkmPrngState *s = HWKM_PRNG(obj);
    uint32_t rnd;

    if (qemu_guest_getrandom(&rnd, sizeof(uint32_t), NULL) == 0) {
        s->values[0] = rnd;
        s->flags[0] = true;
    }
}

static void hwkm_prng_realize(DeviceState *dev, Error **errp)
{
    HwkmPrngState *s = HWKM_PRNG(dev);
    memory_region_init_io(&s->iomem, OBJECT(s), &hwkm_prng_ops, s,
                          TYPE_HWKM_PRNG, HWKM_PRNG_SIZE);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->iomem);
}

static void hwkm_prng_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    ResettableClass *rc = RESETTABLE_CLASS(klass);
    dc->realize = hwkm_prng_realize;
    rc->phases.hold = hwkm_prng_reset_hold;
    dc->desc = "QCOM HWKM PRNG";
}

static const TypeInfo hwkm_prng_info = {
    .name = TYPE_HWKM_PRNG,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(HwkmPrngState),
    .class_init = hwkm_prng_class_init,
};

static void hwkm_prng_register_types(void)
{
    type_register_static(&hwkm_prng_info);
}

type_init(hwkm_prng_register_types);
