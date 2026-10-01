/*
 * QEMU device: HWKM PRNG emulator
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Hardware Key Manager Pseudo Random Number (hwkm_prng)
 */

#include "qemu/osdep.h"
#include "hw/core/irq.h"
#include "hw/core/registerfields.h"
#include "hw/core/resettable.h"
#include "hw/core/sysbus.h"
#include "hw/misc/qcom-hwkm-prng.h"
#include "migration/vmstate.h"
#include "qapi/error.h"
#include "qemu/bitops.h"
#include "qemu/guest-random.h"
#include "qemu/log.h"
#include "qemu/module.h"
#include "qemu/queue.h"
#include "qemu/typedefs.h"
#include "system/memory.h"

/* Register offsets */
#define REG_PRNG_DATA_OUT 0x0
#define REG_PRNG_STATUS 0x4

/* Status bits */
#define STATUS_DATA_AVAIL BIT(0)
#define STATUS_RING_OSC0_HEALTHY BIT(1)
#define STATUS_RING_OSC1_HEALTHY BIT(2)
#define STATUS_RING_OSC2_HEALTHY BIT(3)
#define STATUS_RING_OSC3_HEALTHY BIT(4)
#define STATUS_CURRENT_OPERATION_SHIFT 8

static uint64_t hwkm_prng_read(void *opaque, hwaddr offset, unsigned size)
{
    HwkmPrngState *s = opaque;
    switch (offset) {
    case REG_PRNG_DATA_OUT:
        if (s->data_avail) {
            uint32_t val = s->fifo_data;
            uint32_t rnd;
            /* Prep a new random number after every read */
            if (qemu_guest_getrandom(&rnd, sizeof(uint32_t), NULL) == 0) {
                s->fifo_data = rnd;
            }
            return val;
        }
        return 0;
    case REG_PRNG_STATUS: {
        uint32_t status = 0;
        if (s->data_avail) {
            status |= STATUS_DATA_AVAIL;
        }
        status |= STATUS_RING_OSC0_HEALTHY | STATUS_RING_OSC1_HEALTHY |
                  STATUS_RING_OSC2_HEALTHY | STATUS_RING_OSC3_HEALTHY;
        status |= (0x0 << STATUS_CURRENT_OPERATION_SHIFT); /* IDLE */
        return status;
    }
    default:
        return 0;
    }
}

static void hwkm_prng_write(void *opaque, hwaddr offset, uint64_t val,
                            unsigned size)
{
    qemu_log_mask(LOG_GUEST_ERROR,
                  "%s: Invalid write to offset 0x%" PRIx64
                  " with value 0x%" PRIx64 "\n",
                  __func__, offset, val);
}

static const MemoryRegionOps hwkm_prng_ops = {
    .read = hwkm_prng_read,
    .write = hwkm_prng_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

static void hwkm_prng_reset_hold(Object *obj, ResetType type)
{
    HwkmPrngState *s = HWKM_PRNG(obj);
    uint32_t rnd;

    if (qemu_guest_getrandom(&rnd, sizeof(uint32_t), NULL) == 0) {
        s->fifo_data = rnd;
        s->data_avail = true;
    }
}

static void hwkm_prng_realize(DeviceState *dev, Error **errp)
{
    HwkmPrngState *s = HWKM_PRNG(dev);
    memory_region_init_io(&s->iomem, OBJECT(s), &hwkm_prng_ops, s,
                          TYPE_HWKM_PRNG, HWKM_PRNG_SIZE);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->iomem);
}

static const VMStateDescription vmstate_hwkm_prng = {
    .name = "qdsp6ss-hwkm-prng",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_UINT32(fifo_data, HwkmPrngState),
        VMSTATE_BOOL(data_avail, HwkmPrngState),
        VMSTATE_END_OF_LIST()
    }
};

static void hwkm_prng_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    ResettableClass *rc = RESETTABLE_CLASS(klass);
    dc->realize = hwkm_prng_realize;
    dc->vmsd = &vmstate_hwkm_prng;
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
