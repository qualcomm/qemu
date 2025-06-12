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

#include "hw/sysbus.h"
#include "qom/object.h"

#define QCOM_GRAPHICS_BASE              0x3d00000
#define QCOM_GRAPHICS_SIZE              0x0100000

#define TYPE_QCOM_GRAPHICS "qcom_graphics"
OBJECT_DECLARE_SIMPLE_TYPE(QcomGraphicsState, QCOM_GRAPHICS)

struct QcomGraphicsState {
    SysBusDevice parent;

    MemoryRegion iomem;

    // qemu_irq irqs[2];
};

DeviceState* qcom_graphics_create(hwaddr base_addr);

#endif