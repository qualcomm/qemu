// cmd db memory
// for now, this is handled as a device, even though it's not strictly a device...

#include "qemu/osdep.h"
#include "hw/sysbus-of.h"
#include "qapi/error.h"
#include "hw/sysbus.h"
#include "hw/qdev-properties.h"
#include "hw/qcom/cmd-db.h"

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

static struct cmd_db_header header;

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
    memcpy(header.magic, CMD_DB_MAGIC, sizeof(CMD_DB_MAGIC));
}

static uint64_t qcom_cmd_db_read(void *opaque, hwaddr addr, unsigned size)
{
    QcomCmdDbState *s = QCOM_CMD_DB(opaque);

    if (addr + size >= sizeof(header)) {
        printf("[%s] Unhandled read @offset %ld.\n", s->name, addr);
        return 0;
    }

    uint32_t val = *(uint32_t*)((char*) &header + addr);
    printf("[%s] Header paramter @idx %addr successfully handled: 0x%lx\n", s->name, addr, val);

    return val;
}

static void qcom_cmd_db_write(void *opaque, hwaddr addr,
                              uint64_t value, unsigned int size)
{
    QcomCmdDbState *s = QCOM_CMD_DB(opaque);

    printf("[%s] Unhandled write @offset %ld of value 0x%lx.\n", s->name, addr, value);
}

static const MemoryRegionOps qcom_cmd_db_ops = {
    .read = qcom_cmd_db_read,
    .write = qcom_cmd_db_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .impl = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
};

static void qcom_cmd_db_realize(OfSysBusDevice* ofdev, Error **errp)
{
    QcomCmdDbState *s = QCOM_CMD_DB(ofdev);
    SysBusDevice* sbd = SYS_BUS_DEVICE(ofdev);

    printf("[%s] Adding Command DB device at address 0x%lx\n", s->name, *ofdev->base_addr);

	assert(s->mem_size);
    memory_region_init_io(&s->iomem, OBJECT(ofdev), &qcom_cmd_db_ops, s, TYPE_QCOM_CMD_DB, s->mem_size);
    sysbus_init_mmio(sbd, &s->iomem);
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
