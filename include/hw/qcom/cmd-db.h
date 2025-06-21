/* 
 * Qualcomm Android cmd-db device
 *
 * Author: Romain Malmain <rmalmain@qti.qualcomm.com>
 *
 * Only provides minimal support, mostly to pass probe checks.
 */

#ifndef QEMU_QCOM_CMD_DB_H
#define QEMU_QCOM_CMD_DB_H

#include "qemu/osdep.h"
#include "qom/object.h"
#include "hw/sysbus-of.h"

#define TYPE_QCOM_CMD_DB "qcom_cmd_db"
OBJECT_DECLARE_SIMPLE_TYPE(QcomCmdDbState, QCOM_CMD_DB)

struct QcomCmdDbState {
    OfSysBusDevice parent;

    const char* name;
    uint64_t mem_size;

    MemoryRegion iomem;
};

QcomCmdDbState* cmd_db_create(void* out_fdt, void* in_fdt, const char* node_path, const char* name, uint64_t mem_size);

QcomCmdDbState* cmd_db_create_by_label(void* out_fdt, void* in_fdt, const char* label, uint64_t mem_size);

#endif
