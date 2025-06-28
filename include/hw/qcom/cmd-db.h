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

#define TYPE_QCOM_CMD_DB "qcom-cmd-db"
OBJECT_DECLARE_TYPE(QcomCmdDbState, QcomCmdDbClass, QCOM_CMD_DB)

enum cmd_db_hw_type {
	CMD_DB_HW_INVALID = 0,
	CMD_DB_HW_MIN     = 3,
	CMD_DB_HW_ARC     = CMD_DB_HW_MIN,
	CMD_DB_HW_VRM     = 4,
	CMD_DB_HW_BCM     = 5,
	CMD_DB_HW_MAX     = CMD_DB_HW_BCM,
	CMD_DB_HW_ALL     = 0xff,
};

/**
 * struct bcm_db - Auxiliary data pertaining to each Bus Clock Manager(BCM)
 * @unit: divisor used to convert Hz value to an RPMh msg
 * @width: multiplier used to convert Hz value to an RPMh msg
 * @vcd: virtual clock domain that this bcm belongs to
 * @reserved: reserved to pad the struct
 */
struct bcm_db {
	__le32 unit;
	__le16 width;
	uint8_t vcd;
	uint8_t reserved;
};

// a "flat" entry to add
struct cmd_db_entry {
    char id[8];
    uint32_t addr;
    void* data;
    uint16_t slv_id;
    uint16_t len;
};

struct QcomCmdDbState {
    OfSysBusDevice parent;

    const char* name;
    uint64_t mem_size;

    MemoryRegion rom;
    char* rom_content;

    // will be serialized into ROM at the
    // when the board gets created.
    GArray* entries;
};

struct QcomCmdDbClass {
    OfSysBusDeviceClass parent;

    void (*commit)(QcomCmdDbState* cmds, Error** errp);
    void (*add)(QcomCmdDbState* cmds, struct cmd_db_entry* entry);
    void (*merge)(QcomCmdDbState* cmds, GArray* entries);
};

QcomCmdDbState* cmd_db_create(void* out_fdt, void* in_fdt, const char* node_path, const char* name, uint64_t mem_size);

QcomCmdDbState* cmd_db_create_by_label(void* out_fdt, void* in_fdt, const char* label, uint64_t mem_size);

struct cmd_db_entry* qcom_cmd_db_array_get(GArray* array, size_t idx);
void qcom_cmd_db_array_add_entry(GArray* array, const char* id, enum cmd_db_hw_type slv_id, uint32_t addr, void* data, uint16_t data_len);

#endif
