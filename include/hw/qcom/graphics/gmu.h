/* Qualcomm Android gmu GMU
 *
 * Author: Romain Malmain <rmalmain@qti.qualcomm.com>
 *
 * It has been developed for the Adreno840, on canoe.
 * Versions supported (check the dts file): "qcom,gen8-gmu"
 *
 * As of now, it is the only version supported.
 * Using this device for another driver will have uninteded consequences, and will most likely not work.
 *
 */

#ifndef QEMU_QCOM_GMU_H
#define QEMU_QCOM_GMU_H

#include "qemu/osdep.h"
#include "hw/sysbus-of.h"
#include "qom/object.h"

#include "hw/qcom/graphics/kgsl-iommu.h"

#define QCOM_GMU_BASE              0x3d00000
#define QCOM_GMU_SIZE              0x0100000

#define TYPE_QCOM_GMU "qcom-gmu"
OBJECT_DECLARE_SIMPLE_TYPE(QcomGmuState, QCOM_GMU)

struct QcomGmuState {
    OfSysBusDevice parent;

    MemoryRegion iomem;
    hwaddr gpu_offset;

    MemoryRegion iomem_ao_blk;
    hwaddr gpu_offset_ao_blk;

    uint32_t regs[0x100000 / 4];
    // uint32_t regs[0x68000 / 4];
    // uint32_t regs_ao_blk[0x10000 / 4];

    QcomSMMUState* smmu;
    uint32_t vmid;
};

#endif
