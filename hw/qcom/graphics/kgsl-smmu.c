/*
 * Qualcomm Android KGSL SMMU device
 *
 * Author: Romain Malmain <rmalmain@qti.qualcomm.com>
 *
 */

#include "qemu/osdep.h"
#include "hw/sysbus-of.h"
#include "hw/qcom/smmu.h"
#include "hw/qdev-properties.h"
#include "qapi/error.h"
#include "exec/memory.h"
#include "qemu/error-report.h"

#include "hw/qcom/graphics/kgsl-smmu.h"

QcomKgslSMMUState* qcom_kgsl_smmu_create(void* fdt, void* in_fdt, const char* node_path, const char* name, uint64_t mem_size) {
	DeviceState* dev = qdev_new(TYPE_QCOM_KGSL_SMMU);
	QcomKgslSMMUState* sdev = QCOM_KGSL_SMMU(dev);

	qdev_prop_set_ptr(dev, OF_SYSBUS_PARAM_IN_FDT, in_fdt);
	qdev_prop_set_ptr(dev, OF_SYSBUS_PARAM_FDT, fdt);
	qdev_prop_set_string(dev, OF_SYSBUS_PARAM_NODE_PATH, node_path);

	sdev->mem_size = mem_size;
	sdev->name = name;

	sysbus_realize(SYS_BUS_DEVICE(dev), &error_fatal);

	return sdev;
}

QcomKgslSMMUState* qcom_kgsl_smmu_create_by_label(void* fdt, void* in_fdt, const char* label, uint64_t mem_size) {
	DeviceState* dev = qdev_new(TYPE_QCOM_KGSL_SMMU);
	QcomKgslSMMUState* sdev = QCOM_KGSL_SMMU(dev);

	qdev_prop_set_ptr(dev, OF_SYSBUS_PARAM_IN_FDT, in_fdt);
	qdev_prop_set_ptr(dev, OF_SYSBUS_PARAM_FDT, fdt);
	qdev_prop_set_string(dev, OF_SYSBUS_PARAM_NODE_LABEL, label);

	sdev->mem_size = mem_size;
	sdev->name = label;

	sysbus_realize(SYS_BUS_DEVICE(dev), &error_fatal);

	return sdev;
}

static uint64_t qcom_kgsl_smmu_read(void *opaque, hwaddr addr, unsigned size)
{
    return 0;
}

static void qcom_kgsl_smmu_write(void *opaque, hwaddr addr,
                              uint64_t _value, unsigned int size)
{
}

static const MemoryRegionOps qcom_kgsl_smmu_ops = {
    .read = qcom_kgsl_smmu_read,
    .write = qcom_kgsl_smmu_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .impl = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
};

static void qcom_kgsl_smmu_realize(OfSysBusDevice* ofdev, Error **errp)
{
    QcomKgslSMMUState *s = QCOM_KGSL_SMMU(ofdev);
    SysBusDevice* sbd = SYS_BUS_DEVICE(ofdev);

	assert(s->mem_size);
    memory_region_init_io(&s->iomem, OBJECT(ofdev), &qcom_kgsl_smmu_ops, s, TYPE_QCOM_KGSL_SMMU, s->mem_size);
    sysbus_init_mmio(sbd, &s->iomem);

    struct fdt_iter iter = qemu_fdt_compat_iter_create(ofdev->fdt, "qcom,smmu-kgsl-cb", ofdev->node_path);
    const char* subnode_path;

    size_t nb_cbs = 0;
    while((subnode_path = qemu_fdt_compat_iter_next(ofdev->fdt, &iter))) {
        GArray* props = qemu_fdt_collect_phandle_props(ofdev->fdt, subnode_path, errp);
        for (size_t i = 0; i < props->len; ++i) {
            struct fdt_phandle_prop_data* prop_data = ((struct fdt_phandle_prop_data*) props->data) + i;

            if (prop_data->kind == FDT_PROP_IOMMU) {
                OfSysBusDevice* smmu = of_sysbus_find_by_phandle(prop_data->phandle);

                assert(nb_cbs < KGSL_SMMU_MAX_VD);
                s->cbs[nb_cbs] = (struct qcom_kgsl_cb) {
                    .smmu = QCOM_SMMU(smmu),
                    .vmid = ((uint32_t*) prop_data->params->data)[1],
                    .name = subnode_path,
                };
            }
        }
    }

    s->nb_cbs = nb_cbs;
}

static void qcom_kgsl_smmu_instance_init(Object *obj)
{
}

static void qcom_kgsl_smmu_class_init(ObjectClass *klass, void *data)
{
	OfSysBusDeviceClass* kofdev = OF_SYS_BUS_DEVICE_CLASS(klass);

    kofdev->realize = qcom_kgsl_smmu_realize;
}

static const TypeInfo qcom_kgsl_smmu_type_info = {
    .name          = TYPE_QCOM_KGSL_SMMU,
    .parent        = TYPE_OF_SYS_BUS_DEVICE,
    .instance_size = sizeof(QcomKgslSMMUState),
    .instance_init = qcom_kgsl_smmu_instance_init,
    .class_size    = sizeof(QcomKgslSMMUClass),
    .class_init    = qcom_kgsl_smmu_class_init,
};

static const TypeInfo qcom_kgsl_smmu_iommu_memory_region_info = {
    .parent = TYPE_IOMMU_MEMORY_REGION,
    .name = TYPE_QCOM_KGSL_SMMU_IOMMU_MEMORY_REGION,
};

static void qcom_kgsl_smmu_register_types(void)
{
    type_register_static(&qcom_kgsl_smmu_type_info);
    type_register_static(&qcom_kgsl_smmu_iommu_memory_region_info);
}

type_init(qcom_kgsl_smmu_register_types)

