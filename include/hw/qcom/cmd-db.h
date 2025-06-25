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

enum cmd_db_hw_type {
	CMD_DB_HW_INVALID = 0,
	CMD_DB_HW_MIN     = 3,
	CMD_DB_HW_ARC     = CMD_DB_HW_MIN,
	CMD_DB_HW_VRM     = 4,
	CMD_DB_HW_BCM     = 5,
	CMD_DB_HW_MAX     = CMD_DB_HW_BCM,
	CMD_DB_HW_ALL     = 0xff,
};

struct QcomCmdDbState {
    OfSysBusDevice parent;

    const char* name;
    uint64_t mem_size;

    MemoryRegion rom;
    char* rom_content;
};

QcomCmdDbState* cmd_db_create(void* out_fdt, void* in_fdt, const char* node_path, const char* name, uint64_t mem_size);

QcomCmdDbState* cmd_db_create_by_label(void* out_fdt, void* in_fdt, const char* label, uint64_t mem_size);

#endif
