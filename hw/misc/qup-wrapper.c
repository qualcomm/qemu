/*
 * QUP wrapper (GENI SE QUP) device
 *
 * Models just enough of the Qualcomm QUP wrapper's own register block
 * (distinct from the individual Serial Engine devices it wraps) for the
 * Linux qcom_geni_se driver to probe successfully.  Currently this is
 * limited to the HW_VER register, which the qcom_geni_serial driver
 * reads via geni_se_get_qup_hw_version() during console setup.
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "hw/core/sysbus.h"
#include "hw/core/qdev-properties.h"
#include "hw/misc/qup-wrapper.h"
#include "qemu/log.h"
#include "migration/vmstate.h"

#define QUP_WRAPPER_REG_SIZE 0x6000

/* Register offsets */
#define REG_QUPV3_HW_VER 0x4

typedef struct QupWrapperState {
    SysBusDevice parent_obj;

    MemoryRegion iomem;

    /* Properties */
    uint32_t hw_version;
} QupWrapperState;

static uint64_t qup_wrapper_read(void *opaque, hwaddr offset, unsigned size)
{
    QupWrapperState *s = QUP_WRAPPER(opaque);
    uint64_t value = 0;

    switch (offset) {
    case REG_QUPV3_HW_VER:
        value = s->hw_version;
        break;
    default:
        qemu_log_mask(LOG_UNIMP, "qup-wrapper: read from unknown offset 0x%"
                      HWADDR_PRIx "\n", offset);
        break;
    }

    return value;
}

static void qup_wrapper_write(void *opaque, hwaddr offset, uint64_t value,
                              unsigned size)
{
    qemu_log_mask(LOG_GUEST_ERROR, "qup-wrapper: write to offset 0x%"
                  HWADDR_PRIx " value 0x%" PRIx64 "\n", offset, value);
}

static const MemoryRegionOps qup_wrapper_ops = {
    .read = qup_wrapper_read,
    .write = qup_wrapper_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
};

static void qup_wrapper_realize(DeviceState *dev, Error **errp)
{
    QupWrapperState *s = QUP_WRAPPER(dev);

    memory_region_init_io(&s->iomem, OBJECT(s), &qup_wrapper_ops, s,
                          TYPE_QUP_WRAPPER, QUP_WRAPPER_REG_SIZE);
    sysbus_init_mmio(SYS_BUS_DEVICE(s), &s->iomem);
}

static const Property qup_wrapper_properties[] = {
    DEFINE_PROP_UINT32("hw-version", QupWrapperState, hw_version,
                       0x30100000),
};

static const VMStateDescription vmstate_qup_wrapper = {
    .name = "qup-wrapper",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_UINT32(hw_version, QupWrapperState),
        VMSTATE_END_OF_LIST()
    }
};

static void qup_wrapper_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = qup_wrapper_realize;
    dc->vmsd = &vmstate_qup_wrapper;
    device_class_set_props(dc, qup_wrapper_properties);
    dc->desc = "Qualcomm QUP wrapper";
}

static const TypeInfo qup_wrapper_info = {
    .name          = TYPE_QUP_WRAPPER,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(QupWrapperState),
    .class_init    = qup_wrapper_class_init,
};

static void qup_wrapper_register_types(void)
{
    type_register_static(&qup_wrapper_info);
}

type_init(qup_wrapper_register_types)
