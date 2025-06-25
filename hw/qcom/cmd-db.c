// cmd db memory
// for now, this is handled as a device, even though it's not strictly a device...

#include "qemu/osdep.h"
#include "exec/memory.h"
#include "hw/sysbus-of.h"
#include "qapi/error.h"
#include "hw/sysbus.h"
#include "hw/qdev-properties.h"
#include "hw/qcom/cmd-db.h"
#include "hw/qcom/rpmh-clk.h"
#include "hw/qcom/icc-rpmh.h"

/* === Linux kernel copy paste start ===*/

#define NUM_PRIORITY		2
#define MAX_SLV_ID		8
#define SLAVE_ID_MASK		0x7
#define SLAVE_ID_SHIFT		16
#define SLAVE_ID(addr)		FIELD_GET(GENMASK(19, 16), addr)
#define VRM_ADDR(addr)		FIELD_GET(GENMASK(19, 4), addr)

/**
 * struct entry_header: header for each entry in cmddb
 *
 * @id: resource's identifier
 * @priority: unused
 * @addr: the address of the resource
 * @len: length of the data
 * @offset: offset from :@data_offset, start of the data
 */
struct entry_header {
	uint8_t id[8];
	__le32 priority[NUM_PRIORITY];
	__le32 addr;
	__le16 len;
	__le16 offset;
};

/**
 * struct rsc_hdr: resource header information
 *
 * @slv_id: id for the resource
 * @header_offset: entry's header at offset from the end of the cmd_db_header
 * @data_offset: entry's data at offset from the end of the cmd_db_header
 * @cnt: number of entries for HW type
 * @version: MSB is major, LSB is minor
 * @reserved: reserved for future use.
 */
struct rsc_hdr {
	__le16 slv_id;
	__le16 header_offset;
	__le16 data_offset;
	__le16 cnt;
	__le16 version;
	__le16 reserved[3];
};

/**
 * struct cmd_db_header: The DB header information
 *
 * @version: The cmd db version
 * @magic: constant expected in the database
 * @header: array of resources
 * @checksum: checksum for the header. Unused.
 * @reserved: reserved memory
 * @data: driver specific data
 */
struct cmd_db_header {
	__le32 version;
	uint8_t magic[4];
	struct rsc_hdr header[MAX_SLV_ID];
	__le32 checksum;
	__le32 reserved;
	uint8_t data[];
};

/**
 * DOC: Description of the Command DB database.
 *
 * At the start of the command DB memory is the cmd_db_header structure.
 * The cmd_db_header holds the version, checksum, magic key as well as an
 * array for header for each slave (depicted by the rsc_header). Each h/w
 * based accelerator is a 'slave' (shared resource) and has slave id indicating
 * the type of accelerator. The rsc_header is the header for such individual
 * slaves of a given type. The entries for each of these slaves begin at the
 * rsc_hdr.header_offset. In addition each slave could have auxiliary data
 * that may be needed by the driver. The data for the slave starts at the
 * entry_header.offset to the location pointed to by the rsc_hdr.data_offset.
 *
 * Drivers have a stringified key to a slave/resource. They can query the slave
 * information and get the slave id and the auxiliary data and the length of the
 * data. Using this information, they can format the request to be sent to the
 * h/w accelerator and request a resource state.
 */

static const uint8_t CMD_DB_MAGIC[] = { 0xdb, 0x30, 0x03, 0x0c };

/* === Linux kernel copy paste ends ===*/

QcomCmdDbState* cmd_db_create(void* fdt, void* in_fdt, const char* node_path, const char* name, uint64_t mem_size) {
	DeviceState* dev = qdev_new(TYPE_QCOM_CMD_DB);
	QcomCmdDbState* cdev = QCOM_CMD_DB(dev);

	qdev_prop_set_ptr(dev, OF_SYSBUS_PARAM_IN_FDT, in_fdt);
	qdev_prop_set_ptr(dev, OF_SYSBUS_PARAM_FDT, fdt);
	qdev_prop_set_string(dev, OF_SYSBUS_PARAM_NODE_PATH, node_path);

	cdev->mem_size = mem_size;
	cdev->name = name;

	sysbus_realize(SYS_BUS_DEVICE(dev), &error_fatal);

	return cdev;
}

QcomCmdDbState* cmd_db_create_by_label(void* fdt, void* in_fdt, const char* label, uint64_t mem_size) {
	DeviceState* dev = qdev_new(TYPE_QCOM_CMD_DB);
	QcomCmdDbState* cdev = QCOM_CMD_DB(dev);

	qdev_prop_set_ptr(dev, OF_SYSBUS_PARAM_IN_FDT, in_fdt);
	qdev_prop_set_ptr(dev, OF_SYSBUS_PARAM_FDT, fdt);
	qdev_prop_set_string(dev, OF_SYSBUS_PARAM_NODE_LABEL, label);

	cdev->mem_size = mem_size;
	cdev->name = label;

	sysbus_realize(SYS_BUS_DEVICE(dev), &error_fatal);

	return cdev;
}

static void qcom_cmd_db_init(Object* obj)
{
}

// static uint64_t qcom_cmd_db_read(void *opaque, hwaddr addr, unsigned size)
// {
//     QcomCmdDbState *s = QCOM_CMD_DB(opaque);
// 
//     if (addr + size >= sizeof(header)) {
//         printf("[%s] Unhandled read @offset %ld.\n", s->name, addr);
//         return 0;
//     }
// 
//     uint32_t val = *(uint32_t*)((char*) &header + addr);
//     printf("[%s] Header paramter @idx 0x%lx successfully handled: 0x%x\n", s->name, addr, val);
// 
//     return val;
// }
// 
// static void qcom_cmd_db_write(void *opaque, hwaddr addr,
//                               uint64_t value, unsigned int size)
// {
//     QcomCmdDbState *s = QCOM_CMD_DB(opaque);
// 
//     printf("[%s] Unhandled write @offset %ld of value 0x%lx.\n", s->name, addr, value);
// }
// 
// static const MemoryRegionOps qcom_cmd_db_ops = {
//     .read = qcom_cmd_db_read,
//     .write = qcom_cmd_db_write,
//     .endianness = DEVICE_NATIVE_ENDIAN,
//     .impl = {
//         .min_access_size = 4,
//         .max_access_size = 4,
//     },
// };

// a "flat" entry to add
struct cmd_db_entry {
    uint8_t id[8];
    uint32_t addr;
    void* data;
    uint16_t slv_id;
    uint16_t len;
};

struct cmd_db_collection {
    uint16_t slv_id;
    uint32_t nb_entries;
    size_t total_data_len;
    struct cmd_db_entry** entries;
};

#define buf_iter_write(iter, ty, elt_ptr, nb)                   \
    {                                                           \
        ty * __val = elt_ptr;                                   \
        _buf_iter_write(iter, (void*) __val, sizeof(ty), nb);   \
    }

#define buf_iter_alloc(iter, ty, nb)        \
    (ty*) _buf_iter_alloc(iter, sizeof(ty), nb) \

struct buf_iter {
    void* data;

    size_t total_len;
    size_t current_offset;
};

static struct buf_iter buf_iter_new(void* start, size_t total_len)
{
    struct buf_iter iter = {
        .data = start,
        .total_len = total_len,
        .current_offset = 0,
    };

    return iter;
}

static inline size_t buf_iter_remaining_len(struct buf_iter* iter)
{
    assert(iter->current_offset <= iter->total_len);
    return iter->total_len - iter->current_offset;
}

static void* _buf_iter_alloc(struct buf_iter* iter, size_t data_len, size_t nb_data)
{
    size_t allocated_len = data_len * nb_data;

    assert(allocated_len < buf_iter_remaining_len(iter));

    void *allocated_buf = iter->data + iter->current_offset;
    iter->current_offset += allocated_len;

    return allocated_buf;
}

static void _buf_iter_write(struct buf_iter* iter, void* data, size_t data_len, size_t nb_data)
{
    void* allocated_data = _buf_iter_alloc(iter, data_len, nb_data);
    memcpy(allocated_data, data, data_len * nb_data);
}

// static size_t buf_iter_ptr_to_offset(struct buf_iter* iter, void* data)
// {
//     assert(data >= iter->data && data < iter->data + iter->current_offset);
// 
//     return (size_t) (data - iter->data);
// }

static inline size_t buf_iter_current_offset(struct buf_iter* iter)
{
    return iter->current_offset;
}

#define NB_RSCS (CMD_DB_HW_MAX - CMD_DB_HW_MIN + 1)

static void qcom_cmd_db_init_memory(QcomCmdDbState* cmds, char* rom_content, size_t max_size, struct cmd_db_entry* entries, size_t nb_entries, Error **errp)
{
    struct buf_iter iter = buf_iter_new(rom_content, max_size);

    struct cmd_db_collection collections[NB_RSCS] = { 0 };

    for (size_t i = 0; i < NB_RSCS; ++i) {
        size_t rsc_id = i + CMD_DB_HW_MIN;
        struct cmd_db_collection* c = &collections[i];

        c->slv_id = rsc_id;

        // count the number of entries for the current RSC ID
        for (size_t j = 0; j < nb_entries; ++j) {
            struct cmd_db_entry* entry = &entries[j];
            if (entry->slv_id == c->slv_id) {
                c->nb_entries++;
                c->total_data_len += entry->len;
            }
        }

        // now, we can allocate and add the entries
        c->entries = g_new0(struct cmd_db_entry*, nb_entries);
        size_t c_idx = 0;
        for (size_t j = 0; j < nb_entries; ++j) {
            struct cmd_db_entry* entry = &entries[j];
            if (entry->slv_id == c->slv_id) {
                c->entries[c_idx] = entry;
                c_idx++;
            }
        }
    }

    // build the header
    struct cmd_db_header* hdr = buf_iter_alloc(&iter, struct cmd_db_header, 1);
    size_t hdr_offset = buf_iter_current_offset(&iter);

    // set the magic
    memcpy(hdr->magic, CMD_DB_MAGIC, sizeof(CMD_DB_MAGIC));
    // set the default slv id to ALL
    for (size_t i = 0; i < MAX_SLV_ID; ++i) {
        hdr->header[i].slv_id = cpu_to_le16(CMD_DB_HW_ALL);
    }

    // set the rsc headers
    for (size_t i = 0; i < NB_RSCS; ++i) {
        struct cmd_db_collection* c = &collections[i];
        size_t slv_id = c->slv_id;
        size_t nb_entry_hdrs = c->nb_entries;

        struct rsc_hdr* rsc_hdr = &hdr->header[slv_id];
        size_t entry_hdr_offset = buf_iter_current_offset(&iter) - hdr_offset;
        size_t data_offset = entry_hdr_offset + nb_entry_hdrs * sizeof(struct entry_header);

        rsc_hdr->slv_id = cpu_to_le16(slv_id);
        rsc_hdr->header_offset = cpu_to_le16(entry_hdr_offset);
        rsc_hdr->data_offset = cpu_to_le16(data_offset);
        rsc_hdr->cnt = cpu_to_le16(nb_entry_hdrs);

        size_t entry_data_offset = 0;
        // for each rsc header, set the entry headers
        for (size_t j = 0; j < nb_entry_hdrs; ++j) {
            struct cmd_db_entry* entry = c->entries[j];
            struct entry_header* entry_hdr = buf_iter_alloc(&iter, struct entry_header, 1);

            // printf("[*]\tAdding %s with slv_id %ld entry_hdr_offset %ld data_offset %ld entry_data_offset %ld\n", entry->id, slv_id, entry_hdr_offset, data_offset, entry_data_offset);

            memcpy(entry_hdr->id, entry->id, sizeof(entry_hdr->id));
            // priority is unused
            entry_hdr->addr = cpu_to_le32(entry->addr);
            entry_hdr->len = cpu_to_le16(entry->len);
            entry_hdr->offset = cpu_to_le16(entry_data_offset);

            entry_data_offset += entry->len;
        }

        // now, set the data itself
        for (size_t j = 0; j < nb_entry_hdrs; ++j) {
            struct cmd_db_entry* entry = c->entries[j];
            buf_iter_write(&iter, char, entry->data, entry->len);
        }

        g_free(c->entries);
    }
}

static void qcom_cmd_db_realize(OfSysBusDevice* ofdev, Error **errp)
{
    QcomCmdDbState *cmds = QCOM_CMD_DB(ofdev);
    SysBusDevice* sbd = SYS_BUS_DEVICE(ofdev);

    printf("[%s] Adding Command DB device at address 0x%lx\n", cmds->name, *ofdev->base_addr);

	assert(cmds->mem_size);

    cmds->rom_content = g_new0(char, cmds->mem_size);
    assert(cmds->rom_content);

    size_t nb_entries = clk_rpmh_canoe.num_clks + canoe_gem_noc.num_bcms + canoe_mc_virt.num_bcms;
    size_t entry_idx = 0;
    struct cmd_db_entry* entries = g_new0(struct cmd_db_entry, nb_entries);

    // initalize RPMh clocks DB
    for (size_t i = 0; i < clk_rpmh_canoe.num_clks; ++i) {
        struct clk_rpmh* clk = clk_rpmh_canoe.clks[i];
        if (clk != NULL) {
            struct cmd_db_entry* entry = &entries[entry_idx];
            entry->slv_id = CMD_DB_HW_MIN;
            memcpy(entry->id, clk->res_name, strlen(clk->res_name));

            entry->addr = 4 * (entry_idx + 1); // this is a filler to pass checks, to fill later.
            
            struct bcm_db* bcm = g_new0(struct bcm_db, 1);

            // TODO: fill bcm

            entry->data = bcm;
            entry->len = sizeof(struct bcm_db);

            entry_idx++;
        }
    }

    // initialize ICC RPMh gem noc
    for (size_t i = 0; i < canoe_gem_noc.num_bcms; ++i) {
        struct qcom_icc_bcm* bcm_entry = canoe_gem_noc.bcms[i];
        if (bcm_entry != NULL) {
            struct cmd_db_entry* entry = &entries[entry_idx];
            entry->slv_id = CMD_DB_HW_BCM;
            memcpy(entry->id, bcm_entry->name, strlen(bcm_entry->name));

            entry->addr = 4 * (entry_idx + 1); // this is a filler to pass checks, to fill later.
            
            struct bcm_db* bcm = g_new0(struct bcm_db, 1);

            // TODO: fill bcm

            entry->data = bcm;
            entry->len = sizeof(struct bcm_db);

            entry_idx++;
        }
    }

    // initialize ICC RPMh MC virt
    for (size_t i = 0; i < canoe_mc_virt.num_bcms; ++i) {
        struct qcom_icc_bcm* bcm_entry = canoe_mc_virt.bcms[i];
        if (bcm_entry != NULL) {
            struct cmd_db_entry* entry = &entries[entry_idx];
            entry->slv_id = CMD_DB_HW_BCM;
            memcpy(entry->id, bcm_entry->name, strlen(bcm_entry->name));

            entry->addr = 4 * (entry_idx + 1); // this is a filler to pass checks, to fill later.
            
            struct bcm_db* bcm = g_new0(struct bcm_db, 1);

            // TODO: fill bcm

            entry->data = bcm;
            entry->len = sizeof(struct bcm_db);

            entry_idx++;
        }
    }

    qcom_cmd_db_init_memory(cmds, cmds->rom_content, cmds->mem_size, entries, entry_idx, errp);

    for (size_t i = 0; i < clk_rpmh_canoe.num_clks; ++i) {
        g_free(entries[i].data);
    }
    g_free(entries);

    memory_region_init_ram_ptr(&cmds->rom, OBJECT(ofdev), TYPE_QCOM_CMD_DB, cmds->mem_size, cmds->rom_content);
    memory_region_set_readonly(&cmds->rom, true);

    // TODO: do not use sysbus stuff, it does not make sense.
    sysbus_init_mmio(sbd, &cmds->rom);
}

static void qcom_cmd_db_class_init(ObjectClass* oc, void* data)
{
	OfSysBusDeviceClass* kofdev = OF_SYS_BUS_DEVICE_CLASS(oc);

    kofdev->realize = qcom_cmd_db_realize;
}

static const TypeInfo qcom_cmd_db_info = {
    .name = TYPE_QCOM_CMD_DB,
    .parent = TYPE_OF_SYS_BUS_DEVICE,
    .instance_size = sizeof(QcomCmdDbState),
    .instance_init = qcom_cmd_db_init,
    .class_init = qcom_cmd_db_class_init,
};

static void qcom_cmd_db_register_types(void)
{
    type_register_static(&qcom_cmd_db_info);
}

type_init(qcom_cmd_db_register_types);
