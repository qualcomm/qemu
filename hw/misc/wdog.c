/*
 * Watchdog (WDOG) device
 *
 * Copyright (c) 2025 Qualcomm Technologies, Inc. All Rights Reserved.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "hw/core/sysbus.h"
#include "hw/core/qdev-properties.h"
#include "hw/misc/wdog.h"
#include "qapi/error.h"
#include "qemu/log.h"
#include "migration/vmstate.h"
#include "hw/core/resettable.h"

#define WDOG_REG_SIZE 0x400  /* 0x100 * 4 bytes */

/* Register offset 0xc has special initial value */
#define REG_MAGIC_OFFSET 0xc
#define REG_MAGIC_VALUE  0xdeadbeef

typedef struct WdogState {
    SysBusDevice parent_obj;

    MemoryRegion iomem;
    uint32_t regs[0x100];
} WdogState;

static uint64_t wdog_read(void *opaque, hwaddr offset, unsigned size)
{
    WdogState *s = WDOG(opaque);

    if (size != 4) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "wdog: read with size %d at offset 0x%" HWADDR_PRIx "\n",
                      size, offset);
        return 0;
    }

    if (offset >= WDOG_REG_SIZE) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "wdog: read outside range at 0x%" HWADDR_PRIx "\n",
                      offset);
        return 0;
    }

    return s->regs[offset / 4];
}

static void wdog_write(void *opaque, hwaddr offset, uint64_t value,
                       unsigned size)
{
    WdogState *s = WDOG(opaque);

    if (size != 4) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "wdog: write with size %d at offset 0x%" HWADDR_PRIx "\n",
                      size, offset);
        return;
    }

    if (offset >= WDOG_REG_SIZE) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "wdog: write outside range at 0x%" HWADDR_PRIx "\n",
                      offset);
        return;
    }

    s->regs[offset / 4] = value;
}

static const MemoryRegionOps wdog_ops = {
    .read = wdog_read,
    .write = wdog_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
};

static void wdog_reset_hold(Object *obj, ResetType type)
{
    WdogState *s = WDOG(obj);
    int i;

    /* Clear all registers */
    for (i = 0; i < 0x100; i++) {
        s->regs[i] = 0;
    }

    /* Initialize special register at offset 0xc */
    s->regs[REG_MAGIC_OFFSET / 4] = REG_MAGIC_VALUE;
}

static void wdog_realize(DeviceState *dev, Error **errp)
{
    WdogState *s = WDOG(dev);

    memory_region_init_io(&s->iomem, OBJECT(s), &wdog_ops, s,
                          TYPE_WDOG, WDOG_REG_SIZE);
    sysbus_init_mmio(SYS_BUS_DEVICE(s), &s->iomem);
}

static void wdog_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    ResettableClass *rc = RESETTABLE_CLASS(klass);

    dc->realize = wdog_realize;
    rc->phases.hold = wdog_reset_hold;
    dc->desc = "Watchdog (WDOG)";
}

static const TypeInfo wdog_info = {
    .name          = TYPE_WDOG,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(WdogState),
    .class_init    = wdog_class_init,
};

static void wdog_register_types(void)
{
    type_register_static(&wdog_info);
}

type_init(wdog_register_types)
