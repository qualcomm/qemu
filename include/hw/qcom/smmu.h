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
#include "qemu/iova-tree.h"

#define TYPE_QCOM_SMMU "qcom-smmu"
OBJECT_DECLARE_TYPE(QcomSMMUState, QcomSMMUClass, QCOM_SMMU)

#define TYPE_QCOM_SMMU_IOMMU_MEMORY_REGION "qcom-smmu-iommu-memory-region"

struct smmu_dummy_domain {
    IOVATree* maps;
};

struct QcomSMMUDummyState {
    DMAMap cached_map;
    uint32_t cached_vmid;
    uint32_t cached_pgcount;

    struct smmu_dummy_domain** domains;
    size_t nb_domains;
};

struct QcomSMMUState {
    OfSysBusDevice parent;

    MemoryRegion iomem;

    struct QcomSMMUDummyState dummy_state;
};

struct QcomSMMUClass {
    OfSysBusDeviceClass parent;
};

const DMAMap* qcom_smmu_iova2paddr(struct QcomSMMUState* s, uint32_t vmid, uint64_t iova, uint64_t size);

#endif
