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

    const char* name;
    uint64_t mem_size;

    MemoryRegion iomem;

    qemu_irq irq[6];
};

QcomCrmState* crm_v2_create(void* out_fdt, void* in_fdt, const char* node_path, const char* name, uint64_t mem_size);

QcomCrmState* crm_v2_create_by_label(void* out_fdt, void* in_fdt, const char* label, uint64_t mem_size);

#endif
