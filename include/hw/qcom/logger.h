/* Qualcomm memory logger
 *
 * Author: Romain Malmain <rmalmain@qti.qualcomm.com>
 *
 * It simply logs memory accesses, to catch devices writing to
 * non-supported memory.
 *
 */

#ifndef QEMU_QCOM_LOGGER_H
#define QEMU_QCOM_LOGGER_H

#include "hw/sysbus.h"
#include "qom/object.h"

#define QCOM_LOGGER_BASE              0x0
#define QCOM_LOGGER_SIZE              0x0100000

#define TYPE_QCOM_LOGGER "qcom_logger"
OBJECT_DECLARE_SIMPLE_TYPE(QcomLoggerState, QCOM_LOGGER)

struct QcomLoggerState {
    SysBusDevice parent;

    MemoryRegion iomem;

    // qemu_irq irqs[2];
};

#endif