/* 
 * Qualcomm dummy logger.
 *
 * Copyright (c) 2025 Qualcomm Technologies, Inc. All Rights Reserved.
 *
 * Author: Romain Malmain <rmalmain@qti.qualcomm.com>
 */

#ifndef QEMU_QCOM_LOGGER_H
#define QEMU_QCOM_LOGGER_H

#include "hw/sysbus.h"
#include "qom/object.h"

#define QCOM_LOGGER_BASE              0x0
#define QCOM_LOGGER_SIZE              0x0100000

#define TYPE_QCOM_LOGGER "qcom-logger"
OBJECT_DECLARE_SIMPLE_TYPE(QcomLoggerState, QCOM_LOGGER)

struct QcomLoggerState {
    SysBusDevice parent;

    MemoryRegion iomem;

    // qemu_irq irqs[2];
};

#endif
