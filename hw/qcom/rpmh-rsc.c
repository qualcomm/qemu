#include "qemu/osdep.h"
#include "hw/sysbus-of.h"
#include "qapi/error.h"
#include "hw/sysbus.h"
#include "hw/qdev-properties.h"
#include "hw/qcom/rpmh-rsc.h"

// for cam rsc
static rpmh_reg_table rpmh_rsc_cam = {
    [RPMH_RSC_ID]                           = 0x00040300,
    [RPMH_RSC_PARAM_SOLVER_CONFIG]          = 0x00010100,
    [RPMH_RSC_PARAM_RSC_CONFIG]             = 0x03100214,
    [RPMH_RSC_PARAM_RSC_PARENTCHILD_CONFIG] = 0x60004104,
};

// for apps rsc
static rpmh_reg_table rpmh_rsc_apps = {
     [RPMH_RSC_ID]                           = 0x00040300,
     [RPMH_RSC_PARAM_SOLVER_CONFIG]          = 0x04010100,
     [RPMH_RSC_PARAM_RSC_CONFIG]             = 0x04800414,
     [RPMH_RSC_PARAM_RSC_PARENTCHILD_CONFIG] = 0x800C8104,
};

QcomRpmhRscState* rpmh_rsc_create(void* fdt, void* in_fdt, const char* node_path, const char* name, uint64_t mem_size) {
	DeviceState* dev = qdev_new(TYPE_QCOM_RPMH_RSC);
	QcomRpmhRscState* cdev = QCOM_RPMH_RSC(dev);

	qdev_prop_set_ptr(dev, OF_SYSBUS_PARAM_IN_FDT, in_fdt);
	qdev_prop_set_ptr(dev, OF_SYSBUS_PARAM_FDT, fdt);
	qdev_prop_set_string(dev, OF_SYSBUS_PARAM_NODE_PATH, node_path);

	cdev->mem_size = mem_size;
	cdev->name = name;

	sysbus_realize(SYS_BUS_DEVICE(dev), &error_fatal);

	return cdev;
}

QcomRpmhRscState* rpmh_rsc_create_by_label(void* fdt, void* in_fdt, const char* label, uint64_t mem_size) {
	DeviceState* dev = qdev_new(TYPE_QCOM_RPMH_RSC);
	QcomRpmhRscState* cdev = QCOM_RPMH_RSC(dev);

	qdev_prop_set_ptr(dev, OF_SYSBUS_PARAM_IN_FDT, in_fdt);
	qdev_prop_set_ptr(dev, OF_SYSBUS_PARAM_FDT, fdt);
	qdev_prop_set_string(dev, OF_SYSBUS_PARAM_NODE_LABEL, label);

	cdev->mem_size = mem_size;
	cdev->name = label;

	sysbus_realize(SYS_BUS_DEVICE(dev), &error_fatal);

	return cdev;
}

static void qcom_rpmh_rsc_init(Object* obj)
{
}

static uint64_t qcom_rpmh_rsc_read(void *opaque, hwaddr addr, unsigned size)
{
    QcomRpmhRscState *s = QCOM_RPMH_RSC(opaque);

    // for now, we keep the same behavior for all drvs
    addr %= 0x1000;

    assert(addr % 4 == 0);
    size_t idx = addr / 4;

    if (idx < RPMH_RSC_MAX) {
        uint64_t param = s->regtable[idx];
        printf("[%s] Paramter @idx %ld successfully handled: 0x%lx\n", s->name, idx, param);
        return param;
    } else {
        printf("[%s] Unhandled parameter @idx %ld.\n", s->name, idx);
    }

    return 0;
}

static void qcom_rpmh_rsc_write(void *opaque, hwaddr addr,
                              uint64_t value, unsigned int size)
{
    QcomRpmhRscState *s = QCOM_RPMH_RSC(opaque);

    printf("[%s] Unhandled write @offset 0x%lx of value 0x%lx.\n", s->name, addr, value);
}

static const MemoryRegionOps qcom_rpmh_rsc_ops = {
    .read = qcom_rpmh_rsc_read,
    .write = qcom_rpmh_rsc_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .impl = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
};

static void qcom_rpmh_rsc_realize(OfSysBusDevice* ofdev, Error **errp)
{
    QcomRpmhRscState *s = QCOM_RPMH_RSC(ofdev);
    SysBusDevice* sbd = SYS_BUS_DEVICE(ofdev);

    printf("[%s] Adding device at address 0x%lx\n", s->name, *ofdev->base_addr);

    if (!strcmp(s->name, "cam_rsc")) {
        s->regtable = rpmh_rsc_cam;
    } else if (!strcmp(s->name, "apps_rsc")) {
        s->regtable = rpmh_rsc_apps;
    } else {
        error_setg(errp, "%s: unknown RPMh device: %s",
                   __func__, s->name);
        return;
    }


	assert(s->mem_size);
    memory_region_init_io(&s->iomem, OBJECT(ofdev), &qcom_rpmh_rsc_ops, s, TYPE_QCOM_RPMH_RSC, s->mem_size);
    sysbus_init_mmio(sbd, &s->iomem);
}

static void qcom_rpmh_rsc_class_init(ObjectClass* oc, void* data)
{
	OfSysBusDeviceClass* kofdev = OF_SYS_BUS_DEVICE_CLASS(oc);

    kofdev->realize = qcom_rpmh_rsc_realize;
}

static const TypeInfo qcom_rpmh_rsc_info = {
    .name = TYPE_QCOM_RPMH_RSC,
    .parent = TYPE_OF_SYS_BUS_DEVICE,
    .instance_size = sizeof(QcomRpmhRscState),
    .instance_init = qcom_rpmh_rsc_init,
    .class_init = qcom_rpmh_rsc_class_init,
};

static void qcom_rpmh_rsc_register_types(void)
{
    type_register_static(&qcom_rpmh_rsc_info);
}

type_init(qcom_rpmh_rsc_register_types);
