#include "qemu/osdep.h"
#include "hw/sysbus-of.h"
#include "hw/qcom/qmp.h"
#include "hw/qdev-properties.h"
#include "qapi/error.h"
#include "exec/memory.h"

#define QMP_MAGIC 0x4d41494c
#define QMP_VERSION 1

QcomQMPState* qcom_qmp_create(void* fdt, void* in_fdt, const char* node_path, const char* name, uint64_t mem_size) {
	DeviceState* dev = qdev_new(TYPE_QCOM_QMP);
	QcomQMPState* sdev = QCOM_QMP(dev);

	qdev_prop_set_ptr(dev, OF_SYSBUS_PARAM_IN_FDT, in_fdt);
	qdev_prop_set_ptr(dev, OF_SYSBUS_PARAM_FDT, fdt);
	qdev_prop_set_string(dev, OF_SYSBUS_PARAM_NODE_PATH, node_path);

	sdev->mem_size = mem_size;
	sdev->name = name;

	sysbus_realize(SYS_BUS_DEVICE(dev), &error_fatal);

	return sdev;
}

QcomQMPState* qcom_qmp_create_by_label(void* fdt, void* in_fdt, const char* label, uint64_t mem_size) {
	DeviceState* dev = qdev_new(TYPE_QCOM_QMP);
	QcomQMPState* sdev = QCOM_QMP(dev);

	qdev_prop_set_ptr(dev, OF_SYSBUS_PARAM_IN_FDT, in_fdt);
	qdev_prop_set_ptr(dev, OF_SYSBUS_PARAM_FDT, fdt);
	qdev_prop_set_string(dev, OF_SYSBUS_PARAM_NODE_LABEL, label);

	sdev->mem_size = mem_size;
	sdev->name = label;

	sysbus_realize(SYS_BUS_DEVICE(dev), &error_fatal);

	return sdev;
}

static uint64_t qcom_qmp_read(void *opaque, hwaddr addr, unsigned size)
{
    QcomQMPState *s = QCOM_QMP(opaque);

    printf("[%s] Read at address 0x%lx\n", s->name, addr);

    switch (addr) {
        case 0x00:
            return QMP_MAGIC;
        case 0x04:
            return QMP_VERSION;
        default:
            printf("\tUnhandled read.\n");
            return 0;
    }
}

static void qcom_qmp_write(void *opaque, hwaddr addr,
                              uint64_t value, unsigned int size)
{
    QcomQMPState *s = QCOM_QMP(opaque);

    printf("[%s] Write at address 0x%lx of value 0x%lx\n", s->name, addr, value);
}

static const MemoryRegionOps qcom_qmp_ops = {
    .read = qcom_qmp_read,
    .write = qcom_qmp_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .impl = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
};

static void qcom_qmp_realize(OfSysBusDevice* ofdev, Error **errp)
{
    QcomQMPState *s = QCOM_QMP(ofdev);
    SysBusDevice* sbd = SYS_BUS_DEVICE(ofdev);

	assert(s->mem_size);
    memory_region_init_io(&s->iomem, OBJECT(ofdev), &qcom_qmp_ops, s, TYPE_QCOM_QMP, s->mem_size);
    sysbus_init_mmio(sbd, &s->iomem);
}

static void qcom_qmp_instance_init(Object *obj)
{
}

static void qcom_qmp_class_init(ObjectClass *klass, void *data)
{
	OfSysBusDeviceClass* kofdev = OF_SYS_BUS_DEVICE_CLASS(klass);

    kofdev->realize = qcom_qmp_realize;
}

static const TypeInfo qcom_qmp_type_info = {
    .name          = TYPE_QCOM_QMP,
    .parent        = TYPE_OF_SYS_BUS_DEVICE,
    .instance_size = sizeof(QcomQMPState),
    .instance_init = qcom_qmp_instance_init,
    .class_size    = sizeof(QcomQMPClass),
    .class_init    = qcom_qmp_class_init,
};

static void qcom_qmp_register_types(void)
{
    type_register_static(&qcom_qmp_type_info);
}

type_init(qcom_qmp_register_types)

