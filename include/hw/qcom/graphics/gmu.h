/* 
 * GMU device
 *
 * Copyright (c) 2025 Qualcomm Technologies, Inc. All Rights Reserved.
 *
 * Author: Romain Malmain <rmalmain@qti.qualcomm.com>
 */

#ifndef QEMU_QCOM_GMU_H
#define QEMU_QCOM_GMU_H

#include "qemu/osdep.h"
#include "hw/sysbus-of.h"
#include "qom/object.h"

#include "hw/qcom/smmu.h"

#include "hw/qcom/graphics/hfi.h"

#define TYPE_QCOM_GMU "qcom-gmu"
OBJECT_DECLARE_SIMPLE_TYPE(QcomGMUState, QCOM_GMU)

typedef hwaddr gpuaddr;

struct QcomGMUState {
    OfSysBusDevice parent;

    MemoryRegion iomem;
    hwaddr gpu_offset;

    MemoryRegion iomem_ao_blk;
    hwaddr gpu_offset_ao_blk;

    uint32_t* regs;

    QcomSMMUState* smmu;
    uint32_t vmid;

    struct qcom_hfi_state hfi;
};

bool qcom_gmu_gpumem_read(QcomGMUState* s, uint32_t vmid, gpuaddr addr, char* buf, gpuaddr size);
bool qcom_gmu_gpumem_write(QcomGMUState* s, uint32_t vmid, gpuaddr addr, char* buf, gpuaddr size);

#endif
