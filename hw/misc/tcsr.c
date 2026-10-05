/*
 * TCSR (Top Control and Status Register) device
 *
 * Copyright (c) 2025 Qualcomm Technologies, Inc. All Rights Reserved.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "hw/core/sysbus.h"
#include "hw/core/qdev-properties.h"
#include "hw/misc/tcsr.h"
#include "qapi/error.h"
#include "qemu/log.h"
#include "migration/vmstate.h"
#include "hw/core/resettable.h"

#define TCSR_REG_SIZE 0x100000

/* Register offsets */
#define REG_SOC_EMULATION_TYPE    0x8004
#define REG_SOC_HW_VERSION       0x8000
#define REG_SW_WONCE_SOC_HW_VERSION 0x8008
#define REG_TZ_WONCE_BASE        0x14000
#define REG_TZ_WONCE_SIZE        0x40 /* 16 registers * 4 bytes */

/* SOC_HW_VERSION fields */
#define SOC_HW_VERSION_MINOR_VERSION_SHIFT 0
#define SOC_HW_VERSION_MINOR_VERSION_MASK  0xFF
#define SOC_HW_VERSION_MAJOR_VERSION_SHIFT 8
#define SOC_HW_VERSION_MAJOR_VERSION_MASK  0xFF
#define SOC_HW_VERSION_DEVICE_NUMBER_SHIFT 16
#define SOC_HW_VERSION_DEVICE_NUMBER_MASK  0xFFF
#define SOC_HW_VERSION_FAMILY_NUMBER_SHIFT 28
#define SOC_HW_VERSION_FAMILY_NUMBER_MASK  0xF

typedef struct TCSRState {
    SysBusDevice parent_obj;

    MemoryRegion iomem;

    /* Registers */
    uint32_t soc_emulation_type;
    uint32_t soc_hw_version;
    uint32_t sw_wonce_soc_hw_version;
    uint32_t sw_wonce_soc_hw_version_mask;
    uint32_t tz_wonce[16];
    uint32_t tz_wonce_mask[16];

    /* Properties */
    uint8_t family_number;
    uint16_t device_number;
    uint8_t major_version;
    uint8_t minor_version;
    uint8_t emulation_type;
    uint32_t tz_wonce_init_count;
    uint32_t *tz_wonce_init;  /* Initial values for TZ_WONCE registers */
} TCSRState;

static uint64_t tcsr_read(void *opaque, hwaddr offset, unsigned size)
{
    TCSRState *s = TCSR(opaque);
    uint64_t value = 0;

    if (size != 4) {
        qemu_log_mask(LOG_GUEST_ERROR, "tcsr: read with size %d at offset 0x%"
                      HWADDR_PRIx "\n", size, offset);
        return 0;
    }

    switch (offset) {
    case REG_SOC_EMULATION_TYPE:
        value = s->soc_emulation_type;
        break;
    case REG_SOC_HW_VERSION:
        value = s->soc_hw_version;
        break;
    case REG_SW_WONCE_SOC_HW_VERSION:
        value = s->sw_wonce_soc_hw_version;
        break;
    default:
        if (offset >= REG_TZ_WONCE_BASE &&
            offset < REG_TZ_WONCE_BASE + REG_TZ_WONCE_SIZE) {
            int idx = (offset - REG_TZ_WONCE_BASE) / 4;
            value = s->tz_wonce[idx];
        } else {
            qemu_log_mask(LOG_UNIMP, "tcsr: read from unknown offset 0x%"
                          HWADDR_PRIx "\n", offset);
        }
        break;
    }

    return value;
}

static void tcsr_write(void *opaque, hwaddr offset, uint64_t value,
                       unsigned size)
{
    TCSRState *s = TCSR(opaque);

    if (size != 4) {
        qemu_log_mask(LOG_GUEST_ERROR, "tcsr: write with size %d at offset 0x%"
                      HWADDR_PRIx "\n", size, offset);
        return;
    }

    switch (offset) {
    case REG_SOC_EMULATION_TYPE:
        /* Read-only register */
        qemu_log_mask(LOG_GUEST_ERROR, "tcsr: write to read-only register "
                      "SOC_EMULATION_TYPE\n");
        break;
    case REG_SOC_HW_VERSION:
        /* Read-only register */
        qemu_log_mask(LOG_GUEST_ERROR, "tcsr: write to read-only register "
                      "SOC_HW_VERSION\n");
        break;
    case REG_SW_WONCE_SOC_HW_VERSION:
        /* Write-once register - apply mask */
        s->sw_wonce_soc_hw_version = (s->sw_wonce_soc_hw_version &
                                      ~s->sw_wonce_soc_hw_version_mask) |
                                     (value & s->sw_wonce_soc_hw_version_mask);
        /* After write, clear mask to prevent further writes */
        s->sw_wonce_soc_hw_version_mask = 0;
        break;
    default:
        if (offset >= REG_TZ_WONCE_BASE &&
            offset < REG_TZ_WONCE_BASE + REG_TZ_WONCE_SIZE) {
            int idx = (offset - REG_TZ_WONCE_BASE) / 4;
            /* Write-once register - apply mask */
            s->tz_wonce[idx] = (s->tz_wonce[idx] & ~s->tz_wonce_mask[idx]) |
                               (value & s->tz_wonce_mask[idx]);
            /* After write, clear mask to prevent further writes */
            s->tz_wonce_mask[idx] = 0;
        } else {
            qemu_log_mask(LOG_UNIMP, "tcsr: write to unknown offset 0x%"
                          HWADDR_PRIx " value 0x%" PRIx64 "\n", offset, value);
        }
        break;
    }
}

static const MemoryRegionOps tcsr_ops = {
    .read = tcsr_read,
    .write = tcsr_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
};

static void tcsr_reset_hold(Object *obj, ResetType type)
{
    TCSRState *s = TCSR(obj);
    int i;

    /* Initialize SOC_HW_VERSION register */
    s->soc_hw_version =
        (s->family_number << SOC_HW_VERSION_FAMILY_NUMBER_SHIFT) |
        (s->device_number << SOC_HW_VERSION_DEVICE_NUMBER_SHIFT) |
        (s->major_version << SOC_HW_VERSION_MAJOR_VERSION_SHIFT) |
        (s->minor_version << SOC_HW_VERSION_MINOR_VERSION_SHIFT);

    /* Initialize SW_WONCE_SOC_HW_VERSION with same values */
    s->sw_wonce_soc_hw_version = s->soc_hw_version;
    s->sw_wonce_soc_hw_version_mask = 0xFFFFFFFF; /* All bits writable */

    /* Initialize SOC_EMULATION_TYPE */
    s->soc_emulation_type = s->emulation_type;

    /* Initialize TZ_WONCE registers */
    for (i = 0; i < 16; i++) {
        if (s->tz_wonce_init && i < s->tz_wonce_init_count) {
            s->tz_wonce[i] = s->tz_wonce_init[i];
        } else {
            s->tz_wonce[i] = 0;
        }
        s->tz_wonce_mask[i] = 0xFFFFFFFF; /* All bits writable */
    }
}

static void tcsr_realize(DeviceState *dev, Error **errp)
{
    TCSRState *s = TCSR(dev);

    memory_region_init_io(&s->iomem, OBJECT(s), &tcsr_ops, s,
                          TYPE_TCSR, TCSR_REG_SIZE);
    sysbus_init_mmio(SYS_BUS_DEVICE(s), &s->iomem);
}

static const Property tcsr_properties[] = {
    DEFINE_PROP_UINT8("family-number", TCSRState, family_number, 0xE),
    DEFINE_PROP_UINT16("device-number", TCSRState, device_number, 0x875),
    DEFINE_PROP_UINT8("major-version", TCSRState, major_version, 0x1),
    DEFINE_PROP_UINT8("minor-version", TCSRState, minor_version, 0x0),
    DEFINE_PROP_UINT8("emulation-type", TCSRState, emulation_type, 0x0),
    DEFINE_PROP_ARRAY("tz-wonce-init", TCSRState, tz_wonce_init_count,
                      tz_wonce_init, qdev_prop_uint32, uint32_t),
};

static const VMStateDescription vmstate_tcsr = {
    .name = "tcsr",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_UINT32(soc_emulation_type, TCSRState),
        VMSTATE_UINT32(soc_hw_version, TCSRState),
        VMSTATE_UINT32(sw_wonce_soc_hw_version, TCSRState),
        VMSTATE_UINT32(sw_wonce_soc_hw_version_mask, TCSRState),
        VMSTATE_UINT32_ARRAY(tz_wonce, TCSRState, 16),
        VMSTATE_UINT32_ARRAY(tz_wonce_mask, TCSRState, 16),
        VMSTATE_END_OF_LIST()
    }
};

static void tcsr_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    ResettableClass *rc = RESETTABLE_CLASS(klass);

    dc->realize = tcsr_realize;
    dc->vmsd = &vmstate_tcsr;
    rc->phases.hold = tcsr_reset_hold;
    device_class_set_props(dc, tcsr_properties);
    dc->desc = "TCSR (Top Control and Status Register)";
}

static const TypeInfo tcsr_info = {
    .name          = TYPE_TCSR,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(TCSRState),
    .class_init    = tcsr_class_init,
};

static void tcsr_register_types(void)
{
    type_register_static(&tcsr_info);
}

type_init(tcsr_register_types)
