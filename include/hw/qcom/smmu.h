/* 
 * Qualcomm Android RPMh RSC device
 *
 * Author: Romain Malmain <rmalmain@qti.qualcomm.com>
 *
 * This does not support the official SMMU, but the franksmmu alternative.
 * The translation is not done by fetching in-memory tables, but instead with
 * a custom version using MMIO accesses to register page registrations.
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

struct franksmmu_domain {
    IOVATree* maps;
};

struct QcomFrankSMMUState {
    DMAMap cached_map;
    uint32_t cached_vmid;
    uint32_t cached_pgcount;

    struct franksmmu_domain** domains;
    size_t nb_domains;
};

struct QcomSMMUState {
    OfSysBusDevice parent;

    MemoryRegion iomem;

    struct QcomFrankSMMUState franksmmu_state;
};

struct QcomSMMUClass {
    OfSysBusDeviceClass parent;
};

const DMAMap* qcom_smmu_iova2paddr(struct QcomSMMUState* s, uint32_t vmid, uint64_t iova, uint64_t size);

#endif
