/*
 * Qualcomm Android KGSL IOMMU device
 *
 * Author: Romain Malmain <rmalmain@qti.qualcomm.com>
 *
 */

#include "qemu/osdep.h"
#include "hw/sysbus-of.h"
#include "hw/qdev-properties.h"
#include "qapi/error.h"
#include "exec/memory.h"
#include "qemu/error-report.h"

#include "hw/qcom/graphics/kgsl-iommu.h"

#define IOMMU_LOG(dev, fmt, ...) QDEV_LOG_INFO(dev, fmt __VA_OPT__(,) __VA_ARGS__)
#define IOMMU_LOG_ERROR(dev, fmt, ...) QDEV_LOG_ERROR(dev, fmt __VA_OPT__(,) __VA_ARGS__)

struct qcom_kgsl_cb* qcom_kgsl_iommu_cb_by_vmid(QcomKgslIOMMUState* s, uint32_t vmid)
{
    for (size_t i = 0; i < s->nb_cbs; ++i) {
        struct qcom_kgsl_cb* entry = &s->cbs[i];
        if (entry->vmid == vmid) {
            return entry;
        }
    }

    return NULL;
}

static uint64_t qcom_kgsl_iommu_read(void *opaque, hwaddr addr, unsigned size)
{
    return 0;
}

static void qcom_kgsl_iommu_write(void *opaque, hwaddr addr,
                              uint64_t _value, unsigned int size)
{
}

static const MemoryRegionOps qcom_kgsl_iommu_ops = {
    .read = qcom_kgsl_iommu_read,
    .write = qcom_kgsl_iommu_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .impl = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
};

static void qcom_kgsl_iommu_realize(OfSysBusDevice* ofdev, Error **errp)
{
    QcomKgslIOMMUState *s = QCOM_KGSL_IOMMU(ofdev);
    SysBusDevice* sbd = SYS_BUS_DEVICE(ofdev);

	assert(ofdev->mem_size);
    memory_region_init_io(&s->iomem, OBJECT(ofdev), &qcom_kgsl_iommu_ops, s, TYPE_QCOM_KGSL_IOMMU, ofdev->mem_size);
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

                assert(nb_cbs < KGSL_IOMMU_MAX_VD);
                s->cbs[nb_cbs] = (struct qcom_kgsl_cb) {
                    .smmu = QCOM_SMMU(smmu),
                    .vmid = ((uint32_t*) prop_data->params->data)[1],
                    .name = subnode_path,
                };
                nb_cbs++;
            }
        }
    }

    s->nb_cbs = nb_cbs;
}

static void qcom_kgsl_iommu_instance_init(Object *obj)
{
}

static void qcom_kgsl_iommu_class_init(ObjectClass *klass, void *data)
{
	OfSysBusDeviceClass* kofdev = OF_SYS_BUS_DEVICE_CLASS(klass);

    kofdev->realize = qcom_kgsl_iommu_realize;
}

static const TypeInfo qcom_kgsl_iommu_type_info = {
    .name          = TYPE_QCOM_KGSL_IOMMU,
    .parent        = TYPE_OF_SYS_BUS_DEVICE,
    .instance_size = sizeof(QcomKgslIOMMUState),
    .instance_init = qcom_kgsl_iommu_instance_init,
    .class_size    = sizeof(QcomKgslIOMMUClass),
    .class_init    = qcom_kgsl_iommu_class_init,
};

static void qcom_kgsl_iommu_register_types(void)
{
    type_register_static(&qcom_kgsl_iommu_type_info);
}

type_init(qcom_kgsl_iommu_register_types)

