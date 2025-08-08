/*
 * QMP device
 *
 * Copyright (c) 2025 Qualcomm Technologies, Inc. All Rights Reserved.
 *
 * Author: Romain Malmain <rmalmain@qti.qualcomm.com>
 */

#ifndef QEMU_QCOM_QMP_H
#define QEMU_QCOM_QMP_H

#include "hw/sysbus-of.h"
#include "qom/object.h"
#include "exec/memory.h"

#define QMP_DESC_MCORE_START        0x40
#define QMP_DESC_MCORE_MAX_SIZE     0x64

#define TYPE_QCOM_QMP "qcom-qmp"
OBJECT_DECLARE_TYPE(QcomQMPState, QcomQMPClass, QCOM_QMP)

#define TYPE_QCOM_QMP_IOMMU_MEMORY_REGION "qcom-qmp-iommu-memory-region"

struct QcomQMPState {
    OfSysBusDevice parent;

    MemoryRegion iomem;

    char msg_buf[QMP_DESC_MCORE_MAX_SIZE + 1];
    size_t msg_size;
};

struct QcomQMPClass {
    OfSysBusDeviceClass parent;
};

#endif
