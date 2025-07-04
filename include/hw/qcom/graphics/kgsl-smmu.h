/*
 * Qualcomm Android KGSL SMMU device
 *
 * Author: Romain Malmain <rmalmain@qti.qualcomm.com>
 *
 */

#ifndef QEMU_QCOM_KGSL_SMMU_H
#define QEMU_QCOM_KGSL_SMMU_H

#include "hw/sysbus-of.h"
#include "qom/object.h"
#include "exec/memory.h"

#include "hw/qcom/smmu.h"

#define TYPE_QCOM_KGSL_SMMU "qcom-kgsl-smmu"
OBJECT_DECLARE_TYPE(QcomKgslSMMUState, QcomKgslSMMUClass, QCOM_KGSL_SMMU)

#define TYPE_QCOM_KGSL_SMMU_IOMMU_MEMORY_REGION "qcom-smmu-iommu-memory-region"

#define KGSL_SMMU_MAX_VD 8

struct qcom_kgsl_cb {
    const char* name;
    QcomSMMUState* smmu;
    uint32_t vmid;
};

struct QcomKgslSMMUState {
    OfSysBusDevice parent;

    MemoryRegion iomem;

    const char* name;
    size_t mem_size;

    struct qcom_kgsl_cb cbs[KGSL_SMMU_MAX_VD];
    size_t nb_cbs;
};

struct QcomKgslSMMUClass {
    OfSysBusDeviceClass parent;
};

hwaddr qcom_kgsl_smmu_iova2paddr(struct QcomKgslSMMUState* s, uint32_t vmid, uint64_t iova, uint64_t size);

QcomKgslSMMUState* qcom_kgsl_smmu_create(void* fdt, void* in_fdt, const char* node_path, const char* name, uint64_t mem_size);
QcomKgslSMMUState* qcom_kgsl_smmu_create_by_label(void* fdt, void* in_fdt, const char* label, uint64_t mem_size);

#endif
