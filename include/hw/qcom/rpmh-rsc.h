/* 
 * Qualcomm Android RPMh RSC device
 *
 * Author: Romain Malmain <rmalmain@qti.qualcomm.com>
 *
 * Only provides minimal support, mostly to pass probe checks.
 */

#ifndef QEMU_QCOM_RPMH_RSC_H
#define QEMU_QCOM_RPMH_RSC_H

#include "qemu/osdep.h"
#include "qom/object.h"
#include "hw/sysbus-of.h"

#define TYPE_QCOM_RPMH_RSC "qcom_rpmh_rsc"
OBJECT_DECLARE_SIMPLE_TYPE(QcomRpmhRscState, QCOM_RPMH_RSC)

struct QcomRpmhRscState {
    OfSysBusDevice parent;

    const char* name;
    uint64_t mem_size;

    MemoryRegion iomem;
};

QcomRpmhRscState* rpmh_rsc_create(void* out_fdt, void* in_fdt, const char* node_path, const char* name, uint64_t mem_size);

QcomRpmhRscState* rpmh_rsc_create_by_label(void* out_fdt, void* in_fdt, const char* label, uint64_t mem_size);

#endif
