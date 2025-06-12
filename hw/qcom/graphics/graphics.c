#include "qemu/osdep.h"
#include "hw/qcom/graphics.h"

static uint64_t qcom_graphics_read(void *opaque, hwaddr addr, unsigned size)
{
    return 0;
}

static void qcom_graphics_write(void *opaque, hwaddr addr,
                              uint64_t value, unsigned int size)
{
}

DeviceState* qcom_graphics_create(hwaddr base_addr)
{
    return NULL;
}

static void qcom_graphics_init(Object* obj)
{
}

static const MemoryRegionOps qcom_graphics_ops = {
    .read = qcom_graphics_read,
    .write = qcom_graphics_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .impl = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
};

static void qcom_graphics_realize(DeviceState* dev, Error **errp)
{
    QcomGraphicsState *s = QCOM_GRAPHICS(dev);
    SysBusDevice* sbd = SYS_BUS_DEVICE(dev);

    // Put the whole MMIO range all together.
    // We need to do this because some components sometimes interact
    // with other components "dirtily" (like KGSL with GMU).
    memory_region_init_io(&s->iomem, OBJECT(dev), &qcom_graphics_ops, s, TYPE_QCOM_GRAPHICS, QCOM_GRAPHICS_SIZE);
    sysbus_init_mmio(sbd, &s->iomem);
}

static void qcom_graphics_class_init(ObjectClass* oc, void* data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->realize = qcom_graphics_realize;
}

static const TypeInfo qcom_graphics_info = {
    .name = TYPE_QCOM_GRAPHICS,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(QcomGraphicsState),
    .instance_init = qcom_graphics_init,
    .class_init = qcom_graphics_class_init,
};

static void qcom_graphics_register_types(void)
{
    type_register_static(&qcom_graphics_info);
}

type_init(qcom_graphics_register_types);