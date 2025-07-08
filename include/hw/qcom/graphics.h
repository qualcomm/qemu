/* Qualcomm Android graphics device
 *
 * Author: Romain Malmain <rmalmain@qti.qualcomm.com>
 *
 * It has been developed for the Adreno840, on canoe.
 * Versions supported (check the dts file):
 *  - msm_gpu: "qcom,adreno-gpu-gen8-2-1" (chipid = 0x44050a01)
 *  - kgsl_msm_iommu: "qcom,kgsl-smmu-v2"
 *  - gmu: "qcom,gen8-gmu"
 *
 * As of now, it is the only version supported.
 * Using this device for another driver will have uninteded consequences, and will most likely not work.
 *
 */

#ifndef QEMU_QCOM_GRAPHICS_H
#define QEMU_QCOM_GRAPHICS_H

#include "qemu/osdep.h"
#include "hw/sysbus-of.h"
#include "qom/object.h"

#include "hw/qcom/graphics/kgsl-iommu.h"
#include "hw/qcom/graphics/gmu.h"

#define QCOM_GRAPHICS_SIZE              0x0100000

#define TYPE_QCOM_GRAPHICS "qcom-graphics"
OBJECT_DECLARE_SIMPLE_TYPE(QcomGraphicsState, QCOM_GRAPHICS)

#define GPU_HW_ACTIVE	0x00
#define GPU_HW_IFPC	0x03
#define GPU_HW_MINBW	0x06
#define GPU_HW_SLUMBER	0x0f


struct QcomGraphicsState {
    OfSysBusDevice parent;

    MemoryRegion container;

    MemoryRegion iomem;

    uint32_t regs[0x100000 / 4];

    QcomKgslIOMMUState* iommu;
    QcomGMUState* gmu;
};

hwaddr qcom_graphics_decode_addr(hwaddr addr);

#endif
