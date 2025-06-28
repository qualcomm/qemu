/* 
 * Qualcomm Android RPMh RSC device
 *
 * Author: Romain Malmain <rmalmain@qti.qualcomm.com>
 *
 * Only provides minimal support, mostly to pass probe checks.
 */

#ifndef QEMU_QCOM_SMMU_H
#define QEMU_QCOM_SMMU_H

#include "hw/sysbus-of.h"
#include "qom/object.h"
#include "exec/memory.h"

#define TYPE_QCOM_SMMU "qcom-smmu"
OBJECT_DECLARE_TYPE(QcomSMMUState, QcomSMMUClass, QCOM_SMMU)

#define TYPE_QCOM_SMMU_IOMMU_MEMORY_REGION "qcom-smmu-iommu-memory-region"

struct QcomSMMUState {
    OfSysBusDevice parent;

    MemoryRegion iomem;

    const char* name;
    size_t mem_size;
};

struct QcomSMMUClass {
    OfSysBusDeviceClass parent;
};

QcomSMMUState* qcom_smmu_create(void* fdt, void* in_fdt, const char* node_path, const char* name, uint64_t mem_size);
QcomSMMUState* qcom_smmu_create_by_label(void* fdt, void* in_fdt, const char* label, uint64_t mem_size);

#endif
