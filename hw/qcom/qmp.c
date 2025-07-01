#include "qemu/osdep.h"
#include "hw/sysbus-of.h"
#include "hw/qcom/qmp.h"
#include "hw/qdev-properties.h"
#include "qapi/error.h"
#include "exec/memory.h"

#define QMP_DESC_MAGIC			0x0
#define QMP_DESC_VERSION		0x4
#define QMP_DESC_FEATURES		0x8

/* AOP-side offsets */
#define QMP_DESC_UCORE_LINK_STATE	0xc
#define QMP_DESC_UCORE_LINK_STATE_ACK	0x10
#define QMP_DESC_UCORE_CH_STATE		0x14
#define QMP_DESC_UCORE_CH_STATE_ACK	0x18
#define QMP_DESC_UCORE_MBOX_SIZE	0x1c
#define QMP_DESC_UCORE_MBOX_OFFSET	0x20

/* Linux-side offsets */
#define QMP_DESC_MCORE_LINK_STATE	0x24
#define QMP_DESC_MCORE_LINK_STATE_ACK	0x28
#define QMP_DESC_MCORE_CH_STATE		0x2c
#define QMP_DESC_MCORE_CH_STATE_ACK	0x30
#define QMP_DESC_MCORE_MBOX_SIZE	0x34
#define QMP_DESC_MCORE_MBOX_OFFSET	0x38

#define QMP_STATE_UP			GENMASK(15, 0)
#define QMP_STATE_DOWN			GENMASK(31, 16)

#define QMP_MAGIC 0x4d41494c
#define QMP_VERSION 1

/* 0x64 bytes is enough to store the requests and provides padding to 4 bytes */
#define QMP_MSG_LEN			0x64

#define QMP_NUM_COOLING_RESOURCES	2

#define QMP_DEBUGFS_FILES		4

QcomQMPState* qcom_qmp_create(void* fdt, void* in_fdt, const char* node_path, const char* name, uint64_t mem_size) {
	DeviceState* dev = qdev_new(TYPE_QCOM_QMP);
	QcomQMPState* sdev = QCOM_QMP(dev);

	qdev_prop_set_ptr(dev, OF_SYSBUS_PARAM_IN_FDT, in_fdt);
	qdev_prop_set_ptr(dev, OF_SYSBUS_PARAM_FDT, fdt);
	qdev_prop_set_string(dev, OF_SYSBUS_PARAM_NODE_PATH, node_path);

	sdev->mem_size = mem_size;
	sdev->name = name;

	sysbus_realize(SYS_BUS_DEVICE(dev), &error_fatal);

	return sdev;
}

QcomQMPState* qcom_qmp_create_by_label(void* fdt, void* in_fdt, const char* label, uint64_t mem_size) {
	DeviceState* dev = qdev_new(TYPE_QCOM_QMP);
	QcomQMPState* sdev = QCOM_QMP(dev);

	qdev_prop_set_ptr(dev, OF_SYSBUS_PARAM_IN_FDT, in_fdt);
	qdev_prop_set_ptr(dev, OF_SYSBUS_PARAM_FDT, fdt);
	qdev_prop_set_string(dev, OF_SYSBUS_PARAM_NODE_LABEL, label);

	sdev->mem_size = mem_size;
	sdev->name = label;

	sysbus_realize(SYS_BUS_DEVICE(dev), &error_fatal);

	return sdev;
}

static bool is_msg(QcomQMPState* s, hwaddr addr)
{
    return addr >= QMP_DESC_MCORE_START && addr < QMP_DESC_MCORE_START + QMP_DESC_MCORE_MAX_SIZE;
}

static uint64_t qcom_qmp_read(void *opaque, hwaddr addr, unsigned size)
{
    QcomQMPState *s = QCOM_QMP(opaque);

    printf("[%s] Read at address 0x%lx\n", s->name, addr);

    switch (addr) {
        case QMP_DESC_MAGIC:
            return QMP_MAGIC;
        case QMP_DESC_VERSION:
            return QMP_VERSION;
        case QMP_DESC_MCORE_MBOX_SIZE:
            return QMP_DESC_MCORE_MAX_SIZE;
        case QMP_DESC_MCORE_LINK_STATE_ACK:
            return QMP_STATE_UP;
        case QMP_DESC_MCORE_CH_STATE_ACK:
            return QMP_STATE_UP;
	    case QMP_DESC_UCORE_CH_STATE:
            return QMP_STATE_UP;
        case QMP_DESC_MCORE_MBOX_OFFSET:
            return QMP_DESC_MCORE_START;
        case QMP_DESC_MCORE_START: {
            // commit the message
            if (s->msg_size > 0) {
                s->msg_buf[sizeof(s->msg_buf) - 1] = '\0';
                printf("[%s] QMP message received: %s\n", s->name, s->msg_buf);
                memset(s->msg_buf, 0, sizeof(s->msg_buf));
                s->msg_size = 0;
            }

            return 0;
        }
        default:
            printf("\tUnhandled read.\n");
            return 0;
    }
}

static void write_msg(QcomQMPState* s, hwaddr addr, uint32_t val)
{
    hwaddr offset = addr - QMP_DESC_MCORE_START;

    if (offset == 0) {
        s->msg_size = val;
    } else {
        *(uint32_t*)(s->msg_buf + offset - sizeof(uint32_t)) = val;
    }
}

static void qcom_qmp_write(void *opaque, hwaddr addr,
                              uint64_t value, unsigned int size)
{
    QcomQMPState *s = QCOM_QMP(opaque);

    printf("[%s] Write at address 0x%lx of value 0x%lx\n", s->name, addr, value);

    if (is_msg(s, addr)) {
        write_msg(s, addr, value);
    }
}

static const MemoryRegionOps qcom_qmp_ops = {
    .read = qcom_qmp_read,
    .write = qcom_qmp_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .impl = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
};

static void qcom_qmp_realize(OfSysBusDevice* ofdev, Error **errp)
{
    QcomQMPState *s = QCOM_QMP(ofdev);
    SysBusDevice* sbd = SYS_BUS_DEVICE(ofdev);

	assert(s->mem_size);
    memory_region_init_io(&s->iomem, OBJECT(ofdev), &qcom_qmp_ops, s, TYPE_QCOM_QMP, s->mem_size);
    sysbus_init_mmio(sbd, &s->iomem);
}

static void qcom_qmp_instance_init(Object *obj)
{
}

static void qcom_qmp_class_init(ObjectClass *klass, void *data)
{
	OfSysBusDeviceClass* kofdev = OF_SYS_BUS_DEVICE_CLASS(klass);

    kofdev->realize = qcom_qmp_realize;
}

static const TypeInfo qcom_qmp_type_info = {
    .name          = TYPE_QCOM_QMP,
    .parent        = TYPE_OF_SYS_BUS_DEVICE,
    .instance_size = sizeof(QcomQMPState),
    .instance_init = qcom_qmp_instance_init,
    .class_size    = sizeof(QcomQMPClass),
    .class_init    = qcom_qmp_class_init,
};

static void qcom_qmp_register_types(void)
{
    type_register_static(&qcom_qmp_type_info);
}

type_init(qcom_qmp_register_types)

