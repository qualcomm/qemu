// cmd db memory

#include "qemu/osdep.h"
#include "exec/memory.h"
#include "hw/sysbus-of.h"
#include "qapi/error.h"
#include "hw/sysbus.h"
#include "hw/qdev-properties.h"
#include "hw/qcom/cmd-db.h"
#include "hw/qcom/rpmh-clk.h"
#include "hw/qcom/icc-rpmh.h"

#define CMD_DB_LOG(dev, fmt, ...) QDEV_LOG_INFO(dev, fmt __VA_OPT__(,) __VA_ARGS__)

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

/* === Linux kernel copy paste start ===*/


/**
 * memcpy_and_pad - Copy one buffer to another with padding
 * @dest: Where to copy to
 * @dest_len: The destination buffer size
 * @src: Where to copy from
 * @count: The number of bytes to copy
 * @pad: Character to use for padding if space is left in destination.
 */
static void memcpy_and_pad(void *dest, size_t dest_len, const void *src, size_t count,
		    int pad)
{
	if (dest_len > count) {
		memcpy(dest, src, count);
		memset(dest + count, pad,  dest_len - count);
	} else {
		memcpy(dest, src, dest_len);
	}
}

/**
 * strtomem_pad - Copy NUL-terminated string to non-NUL-terminated buffer
 *
 * @dest: Pointer of destination character array (marked as __nonstring)
 * @src: Pointer to NUL-terminated string
 * @pad: Padding character to fill any remaining bytes of @dest after copy
 *
 * This is a replacement for strncpy() uses where the destination is not
 * a NUL-terminated string, but with bounds checking on the source size, and
 * an explicit padding character. If padding is not required, use strtomem().
 *
 * Note that the size of @dest is not an argument, as the length of @dest
 * must be discoverable by the compiler.
 */
#define strtomem_pad(dest, src, pad)	do {				\
	const size_t _dest_len = __builtin_object_size(dest, 1);	\
	const size_t _src_len = __builtin_object_size(src, 1);		\
									\
	memcpy_and_pad(dest, _dest_len, src,				\
		       strnlen(src, MIN(_src_len, _dest_len)), pad);	\
} while (0)

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

struct cmd_db_entry* qcom_cmd_db_array_get(GArray* array, size_t idx)
{
    return &((struct cmd_db_entry*) array->data)[idx];
}

static struct cmd_db_entry get_entry(const char* id, enum cmd_db_hw_type slv_id, uint32_t addr, void* data, uint16_t data_len)
{
    struct cmd_db_entry entry = { 0 };

    entry.slv_id = slv_id;

    memcpy(entry.id, id, MIN(8, strlen(id)));

    entry.addr = addr; // this is a filler to pass checks, to fill later.
    entry.addr |= ((slv_id & SLAVE_ID_MASK) << SLAVE_ID_SHIFT);
    
    char* data_cpy = g_new(char, data_len);
    memcpy(data_cpy, data, data_len);

    // TODO: fill bcm
    entry.data = data_cpy;
    entry.len = data_len;

    return entry;
}

void qcom_cmd_db_array_add_entry(GArray* array, const char* id, enum cmd_db_hw_type slv_id, uint32_t addr, void* data, uint16_t data_len)
{
    struct cmd_db_entry entry = get_entry(id, slv_id, addr, data, data_len);
    g_array_append_vals(array, &entry, 1);
}

static void qcom_cmd_db_array_add(GArray* array, struct cmd_db_entry* entry)
{
    g_array_append_vals(array, entry, 1);
}

static void clean_entry(gpointer elt)
{
    struct cmd_db_entry* entry = elt;

    if (entry && entry->data) {
        g_free(entry->data);
    }
}

static void qcom_cmd_db_init(Object* obj)
{
    QcomCmdDbState *s = QCOM_CMD_DB(obj);

    s->entries = g_array_new(false, true, sizeof(struct cmd_db_entry));
}

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

static inline size_t buf_iter_current_offset(struct buf_iter* iter)
{
    return iter->current_offset;
}

#define NB_RSCS (CMD_DB_HW_MAX - CMD_DB_HW_MIN + 1)

static void qcom_cmd_db_init_memory(QcomCmdDbState* cmds, char* rom_content, size_t max_size, Error **errp)
{
    struct buf_iter iter = buf_iter_new(rom_content, max_size);
    GArray* entries = cmds->entries;

    CMD_DB_LOG(cmds, "Initializing cmd_db memory with %d entries.\n", cmds->entries->len);

    struct cmd_db_collection collections[NB_RSCS] = { 0 };

    for (size_t i = 0; i < NB_RSCS; ++i) {
        size_t rsc_id = i + CMD_DB_HW_MIN;
        struct cmd_db_collection* c = &collections[i];

        c->slv_id = rsc_id;

        // count the number of entries for the current RSC ID
        for (size_t j = 0; j < entries->len; ++j) {
            struct cmd_db_entry* entry = qcom_cmd_db_array_get(entries, j);
            if (entry->slv_id == c->slv_id) {
                c->nb_entries++;
                c->total_data_len += entry->len;
            }
        }

        // now, we can allocate and add the entries
        c->entries = g_new0(struct cmd_db_entry*, entries->len);
        size_t c_idx = 0;
        for (size_t j = 0; j < entries->len; ++j) {
            struct cmd_db_entry* entry = qcom_cmd_db_array_get(entries, j);
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

// ==== test functions, mostly taken from the linux kernel ====

struct cmd_db_header* cmd_db_header;

static bool cmd_db_magic_matches(const struct cmd_db_header *header)
{
	const uint8_t *magic = header->magic;

	return memcmp(magic, CMD_DB_MAGIC, ARRAY_SIZE(CMD_DB_MAGIC)) == 0;
}

/**
 * cmd_db_ready - Indicates if command DB is available
 *
 * Return: 0 on success, errno otherwise
 */
static int cmd_db_ready(void)
{
	if (cmd_db_header == NULL)
		return -1;
	else if (!cmd_db_magic_matches(cmd_db_header))
		return -2;

	return 0;
}

static inline const void *rsc_to_entry_header(const struct rsc_hdr *hdr)
{
	uint16_t offset = le16_to_cpu(hdr->header_offset);

	return cmd_db_header->data + offset;
}

static int cmd_db_get_header(const char *id, const struct entry_header **eh,
			     const struct rsc_hdr **rh)
{
	const struct rsc_hdr *rsc_hdr;
	const struct entry_header *ent;
	int ret, i, j;
	uint8_t query[sizeof(ent->id)];

    ret = cmd_db_ready();
	if (ret)
		return ret;

	strtomem_pad(query, id, 0);

	for (i = 0; i < MAX_SLV_ID; i++) {
		rsc_hdr = &cmd_db_header->header[i];
		if (!rsc_hdr->slv_id)
			break;

		ent = rsc_to_entry_header(rsc_hdr);
		for (j = 0; j < le16_to_cpu(rsc_hdr->cnt); j++, ent++) {
			if (memcmp(ent->id, query, sizeof(ent->id)) == 0) {
				if (eh)
					*eh = ent;
				if (rh)
					*rh = rsc_hdr;
				return 0;
			}
		}
	}

	return -ENODEV;
}

/**
 * cmd_db_read_addr() - Query command db for resource id address.
 *
 * @id: resource id to query for address
 *
 * Return: resource address on success, 0 on error
 *
 * This is used to retrieve resource address based on resource
 * id.
 */
static uint32_t cmd_db_read_addr(const char *id)
{
	int ret;
	const struct entry_header *ent;

	ret = cmd_db_get_header(id, &ent, NULL);

	return ret < 0 ? 0 : le32_to_cpu(ent->addr);
}

/**
 * cmd_db_read_slave_id - Get the slave ID for a given resource address
 *
 * @id: Resource id to query the DB for version
 *
 * Return: cmd_db_hw_type enum on success, CMD_DB_HW_INVALID on error
 */
static enum cmd_db_hw_type cmd_db_read_slave_id(const char *id)
{
	int ret;
	const struct entry_header *ent;
	uint32_t addr;

	ret = cmd_db_get_header(id, &ent, NULL);
	if (ret < 0)
		return CMD_DB_HW_INVALID;

	addr = le32_to_cpu(ent->addr);
	return (addr >> SLAVE_ID_SHIFT) & SLAVE_ID_MASK;
}

// ==== end of test functions ====

static void qcom_cmd_db_add(QcomCmdDbState* cmds, struct cmd_db_entry* entry)
{
    qcom_cmd_db_array_add(cmds->entries, entry);
}

static void qcom_cmd_db_merge(QcomCmdDbState* cmds, GArray* entries)
{
    g_array_append_vals(cmds->entries, entries->data, entries->len);
}

static void qcom_cmd_db_add_entry(QcomCmdDbState* cmds, const char id[8], enum cmd_db_hw_type slv_id, uint32_t addr, void* data, uint16_t data_len)
{
    qcom_cmd_db_array_add_entry(cmds->entries, id, slv_id, addr, data, data_len);
}

static void qcom_cmd_db_commit(QcomCmdDbState* cmds, Error** errp)
{
    OfSysBusDevice* ofdev = OF_SYS_BUS_DEVICE(cmds);
    qcom_cmd_db_init_memory(cmds, cmds->rom_content, ofdev->mem_size, errp);

    // test for cmd db
    cmd_db_header = (struct cmd_db_header*) cmds->rom_content;

    // extreme case, when the id is 8 chars
    uint32_t addr = cmd_db_read_addr("vrm.wcal");
    assert(addr != 0);
    
    // make sure the slvid is encoded in the address
    int sid = cmd_db_read_slave_id("L7N_E1");
    assert(sid == 4);
}

static void qcom_cmd_db_realize(OfSysBusDevice* ofdev, Error **errp)
{
    QcomCmdDbState *cmds = QCOM_CMD_DB(ofdev);
    SysBusDevice* sbd = SYS_BUS_DEVICE(ofdev);

	assert(ofdev->mem_size);

    cmds->rom_content = g_new0(char, ofdev->mem_size);
    assert(cmds->rom_content);

    // size_t nb_entries = clk_rpmh_canoe.num_clks + canoe_gem_noc.num_bcms + canoe_mc_virt.num_bcms;
    // size_t entry_idx = 0;
    GArray* entries = cmds->entries;
    g_array_set_clear_func(entries, clean_entry);

    // initalize RPMh clocks DB
    for (size_t i = 0; i < clk_rpmh_canoe.num_clks; ++i) {
        struct clk_rpmh* clk = clk_rpmh_canoe.clks[i];
        if (clk != NULL) {
            struct bcm_db* bcm = g_new0(struct bcm_db, 1);
            qcom_cmd_db_add_entry(cmds, clk->res_name, CMD_DB_HW_MIN, 4, (void*) bcm, sizeof(struct bcm_db));
        }
    }

    // initialize ICC RPMh memdb entries
    for (size_t i = 0; i < canoe_icc_collection.num_icc_mds; ++i) {
        const struct qcom_icc_md* md = &canoe_icc_collection.icc_mds[i];

        for (size_t j = 0; j < md->desc->num_bcms; ++j) {
            struct qcom_icc_bcm* bcm_entry = md->desc->bcms[j];

            if (bcm_entry != NULL) {
                struct bcm_db* bcm = g_new0(struct bcm_db, 1);
                qcom_cmd_db_add_entry(cmds, bcm_entry->name, CMD_DB_HW_BCM, 4, (void*) bcm, sizeof(struct bcm_db));
            }
        }
    }

    memory_region_init_ram_ptr(&cmds->rom, OBJECT(ofdev), TYPE_QCOM_CMD_DB, ofdev->mem_size, cmds->rom_content);
    memory_region_set_readonly(&cmds->rom, true);

    // TODO: do not use sysbus stuff, it does not make sense.
    sysbus_init_mmio(sbd, &cmds->rom);
}

static void qcom_cmd_db_class_init(ObjectClass* oc, void* data)
{
    QcomCmdDbClass* kcmd = QCOM_CMD_DB_CLASS(oc);
	OfSysBusDeviceClass* kofdev = OF_SYS_BUS_DEVICE_CLASS(oc);

    kofdev->realize = qcom_cmd_db_realize;

    kcmd->commit = qcom_cmd_db_commit;
    kcmd->add = qcom_cmd_db_add;
    kcmd->merge = qcom_cmd_db_merge;
}

static const TypeInfo qcom_cmd_db_info = {
    .name = TYPE_QCOM_CMD_DB,
    .parent = TYPE_OF_SYS_BUS_DEVICE,
    .instance_size = sizeof(QcomCmdDbState),
    .instance_init = qcom_cmd_db_init,
    .class_size = sizeof(QcomCmdDbClass),
    .class_init = qcom_cmd_db_class_init,
};

static void qcom_cmd_db_register_types(void)
{
    type_register_static(&qcom_cmd_db_info);
}

type_init(qcom_cmd_db_register_types);
