/*
 * Qualcomm Android KGSL IOMMU device
 *
 * Author: Romain Malmain <rmalmain@qti.qualcomm.com>
 *
 */

#ifndef QEMU_QCOM_KGSL_IOMMU_H
#define QEMU_QCOM_KGSL_IOMMU_H

#include "hw/sysbus-of.h"
#include "qom/object.h"
#include "exec/memory.h"
#include "hw/qcom/smmu.h"

#define TYPE_QCOM_KGSL_IOMMU "qcom-kgsl-iommu"
OBJECT_DECLARE_TYPE(QcomKgslIOMMUState, QcomKgslIOMMUClass, QCOM_KGSL_IOMMU)

#define TYPE_QCOM_KGSL_IOMMU_MEMORY_REGION "qcom-kgsl-iommu-memory-region"

#define KGSL_IOMMU_MAX_VD 8

struct qcom_kgsl_cb {
    const char* name;
    QcomSMMUState* smmu;
    uint32_t vmid;
};

struct QcomKgslIOMMUState {
    OfSysBusDevice parent;

    MemoryRegion iomem;

    struct qcom_kgsl_cb cbs[KGSL_IOMMU_MAX_VD];
    size_t nb_cbs;
};

struct QcomKgslIOMMUClass {
    OfSysBusDeviceClass parent;
};

hwaddr qcom_kgsl_iommu_iova2paddr(struct QcomKgslIOMMUState* s, uint32_t vmid, uint64_t iova, uint64_t size);

struct qcom_kgsl_cb* qcom_kgsl_iommu_cb_by_vmid(QcomKgslIOMMUState* s, uint32_t vmid);

#endif
