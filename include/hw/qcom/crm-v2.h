/* 
 * Qualcomm Android CRM (v2) device
 *
 * Author: Romain Malmain <rmalmain@qti.qualcomm.com>
 *
 * Only provides minimal support, mostly to pass probe checks.
 */

#ifndef QEMU_QCOM_CRM_H
#define QEMU_QCOM_CRM_H

#include "qemu/osdep.h"
#include "qom/object.h"
#include "hw/sysbus-of.h"

#define TYPE_QCOM_CRM "qcom-crm-v2"
OBJECT_DECLARE_SIMPLE_TYPE(QcomCrmState, QCOM_CRM)

struct QcomCrmState {
    OfSysBusDevice parent;

    MemoryRegion iomem;

    qemu_irq irq[6];
};

#endif
