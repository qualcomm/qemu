#include "qemu/osdep.h"
#include "hw/arm/qcom-virt.h"
#include "hw/qcom/logger.h"

#define LOGGER_LOG(dev, fmt, ...) QDEV_LOG_INFO(dev, fmt __VA_OPT__(,) __VA_ARGS__)
#define LOGGER_LOG_ERROR(dev, fmt, ...) QDEV_LOG_ERROR(dev, fmt __VA_OPT__(,) __VA_ARGS__)

static uint64_t qcom_logger_read(void *opaque, hwaddr addr, unsigned size)
{
    LOGGER_LOG(opaque, "read detected @addr 0x%lx\n", addr);
    return 0;
}

static void qcom_logger_write(void *opaque, hwaddr addr,
                              uint64_t value, unsigned int size)
{
    LOGGER_LOG(opaque, "write detected @addr 0x%lx\n", addr);
}

static void qcom_logger_init(Object* obj)
{
}

static const MemoryRegionOps qcom_logger_ops = {
    .read = qcom_logger_read,
    .write = qcom_logger_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .impl = {
        .min_access_size = 1,
        .max_access_size = 8,
    },
};

static void qcom_logger_realize(DeviceState* dev, Error **errp)
{
    QcomLoggerState *s = QCOM_LOGGER(dev);
    SysBusDevice* sbd = SYS_BUS_DEVICE(dev);

    // Put the whole MMIO range all together.
    // We need to do this because some components sometimes interact
    // with other components "dirtily" (like KGSL with GMU).
    memory_region_init_io(&s->iomem, OBJECT(dev), &qcom_logger_ops, s, TYPE_QCOM_LOGGER, QCOM_VIRT_HW_TOP_ADDR - QCOM_VIRT_HW_BASE_ADDR);
    sysbus_init_mmio(sbd, &s->iomem);
}

static void qcom_logger_class_init(ObjectClass* oc, void* data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->realize = qcom_logger_realize;
}

static const TypeInfo qcom_logger_info = {
    .name = TYPE_QCOM_LOGGER,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(QcomLoggerState),
    .instance_init = qcom_logger_init,
    .class_init = qcom_logger_class_init,
};

static void qcom_logger_register_types(void)
{
    type_register_static(&qcom_logger_info);
}

type_init(qcom_logger_register_types);
