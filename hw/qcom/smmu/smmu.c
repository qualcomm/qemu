#include "qemu/osdep.h"
#include "hw/sysbus-of.h"
#include "hw/qcom/smmu.h"
#include "hw/qdev-properties.h"
#include "qapi/error.h"
#include "exec/memory.h"
#include "qemu/error-report.h"

/* Configuration registers for the dummy device */
#define QCOM_SMMU_DUMMY_PADDR_LO	0x39000
#define QCOM_SMMU_DUMMY_PADDR_HI	0x39004
#define QCOM_SMMU_DUMMY_IOVA_LO		0x39008
#define QCOM_SMMU_DUMMY_IOVA_HI		0x3900c
#define QCOM_SMMU_DUMMY_PGSIZE		0x39010
#define QCOM_SMMU_DUMMY_VMID		0x39014
#define QCOM_SMMU_DUMMY_COMMIT		0x39018

#define BITS32_MASK                 0xffffffff

QcomSMMUState* qcom_smmu_create(void* fdt, void* in_fdt, const char* node_path, const char* name, uint64_t mem_size) {
	DeviceState* dev = qdev_new(TYPE_QCOM_SMMU);
	QcomSMMUState* sdev = QCOM_SMMU(dev);

	qdev_prop_set_ptr(dev, OF_SYSBUS_PARAM_IN_FDT, in_fdt);
	qdev_prop_set_ptr(dev, OF_SYSBUS_PARAM_FDT, fdt);
	qdev_prop_set_string(dev, OF_SYSBUS_PARAM_NODE_PATH, node_path);

	sdev->mem_size = mem_size;
	sdev->name = name;

	sysbus_realize(SYS_BUS_DEVICE(dev), &error_fatal);

	return sdev;
}

QcomSMMUState* qcom_smmu_create_by_label(void* fdt, void* in_fdt, const char* label, uint64_t mem_size) {
	DeviceState* dev = qdev_new(TYPE_QCOM_SMMU);
	QcomSMMUState* sdev = QCOM_SMMU(dev);

	qdev_prop_set_ptr(dev, OF_SYSBUS_PARAM_IN_FDT, in_fdt);
	qdev_prop_set_ptr(dev, OF_SYSBUS_PARAM_FDT, fdt);
	qdev_prop_set_string(dev, OF_SYSBUS_PARAM_NODE_LABEL, label);

	sdev->mem_size = mem_size;
	sdev->name = label;

	sysbus_realize(SYS_BUS_DEVICE(dev), &error_fatal);

	return sdev;
}

hwaddr qcom_smmu_iova2paddr(struct QcomSMMUState* s, uint32_t vmid, uint64_t iova, uint64_t size)
{
    DMAMap needle = {
        .iova = iova,
        .size = size,
    };

    assert(s->dummy_state.domains[vmid]);

    const DMAMap* res = iova_tree_find(s->dummy_state.domains[vmid]->maps, &needle);
    assert(res);

    return res->translated_addr;
}

static uint64_t qcom_smmu_read(void *opaque, hwaddr addr, unsigned size)
{
    QcomSMMUState *s = QCOM_SMMU(opaque);

    printf("[%s] Read at address 0x%lx\n", s->name, addr);

    switch (addr) {
        case 0x00:
            return 0x00200001;
        case 0x10:
            return 0x04000004;
        case 0x20:
            return 0xFC017E19;
        case 0x24:
            return 0x40000019;
        case 0x28:
            return 0x00005511;
        case 0x48:
            return 0;
        case 0x74:
            return 0;
        case 0x800:
            return 0;
        default:
            printf("\tUnhandled read.\n");
            return 0;
    }
}

static void qcom_smmu_write(void *opaque, hwaddr addr,
                              uint64_t _value, unsigned int size)
{
    QcomSMMUState *s = QCOM_SMMU(opaque);
    uint32_t value = _value;

    printf("[%s] Write at address 0x%lx of value 0x%x\n", s->name, addr, value);

    switch (addr) {
        case QCOM_SMMU_DUMMY_PADDR_LO: {
            s->dummy_state.cached_map.translated_addr &= ~BITS32_MASK;
            s->dummy_state.cached_map.translated_addr |= value;
            break;
        }
        case QCOM_SMMU_DUMMY_PADDR_HI: {
            s->dummy_state.cached_map.translated_addr &= BITS32_MASK;
            s->dummy_state.cached_map.translated_addr |= (_value << 32);
            break;
        }
        case QCOM_SMMU_DUMMY_IOVA_LO: {
            s->dummy_state.cached_map.iova &= ~BITS32_MASK;
            s->dummy_state.cached_map.iova |= value;
            break;
        }
        case QCOM_SMMU_DUMMY_IOVA_HI: {
            s->dummy_state.cached_map.iova &= BITS32_MASK;
            s->dummy_state.cached_map.iova |= (_value << 32);
            break;
        }
        case QCOM_SMMU_DUMMY_PGSIZE: {
            s->dummy_state.cached_map.size = value;
            break;
        }
        case QCOM_SMMU_DUMMY_VMID: {
            s->dummy_state.cached_vmid = value;
            break;
        }
        case QCOM_SMMU_DUMMY_COMMIT: {
            if (value) {
                struct DMAMap* map = &s->dummy_state.cached_map;
                uint32_t vmid = s->dummy_state.cached_vmid;

                if (s->dummy_state.cached_vmid > 1024) {
                    error_report("a high value of VMID has been provided. It's either a bug or the current domain implementation should be changed for a hashmap.");
                    exit(1);
                }

                if (vmid >= s->dummy_state.nb_domains) {
                    uint64_t nb_new_bytes = (vmid + 1 - s->dummy_state.nb_domains) * sizeof(struct smmu_dummy_domain*);
                    uint64_t old_nb_domains = s->dummy_state.nb_domains;

                    s->dummy_state.domains = g_realloc_n(s->dummy_state.domains, vmid + 1, sizeof(struct smmu_dummy_domain*));
                    s->dummy_state.nb_domains = vmid + 1;

                    memset(s->dummy_state.domains + old_nb_domains, 0, nb_new_bytes);
                }
                assert(vmid < s->dummy_state.nb_domains);

                if (!s->dummy_state.domains[vmid]) {
                    s->dummy_state.domains[vmid] = g_new(struct smmu_dummy_domain, 1);
                }

                struct smmu_dummy_domain* domain = s->dummy_state.domains[vmid];

                domain->maps = iova_tree_new();
                iova_tree_insert(domain->maps, map);
            }
            break;
        }
        default:
            printf("\tUnknown write address.\n");
            break;
    }
}

static const MemoryRegionOps qcom_smmu_ops = {
    .read = qcom_smmu_read,
    .write = qcom_smmu_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .impl = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
};

static void qcom_smmu_realize(OfSysBusDevice* ofdev, Error **errp)
{
    QcomSMMUState *s = QCOM_SMMU(ofdev);
    SysBusDevice* sbd = SYS_BUS_DEVICE(ofdev);

	assert(s->mem_size);
    memory_region_init_io(&s->iomem, OBJECT(ofdev), &qcom_smmu_ops, s, TYPE_QCOM_SMMU, s->mem_size);
    sysbus_init_mmio(sbd, &s->iomem);
}

static void qcom_smmu_instance_init(Object *obj)
{
}

static void qcom_smmu_class_init(ObjectClass *klass, void *data)
{
	OfSysBusDeviceClass* kofdev = OF_SYS_BUS_DEVICE_CLASS(klass);

    kofdev->realize = qcom_smmu_realize;
}


static void qcom_smmu_iommu_memory_region_class_init(ObjectClass *klass,
                                                  void *data)
{
    // IOMMUMemoryRegionClass *imrc = IOMMU_MEMORY_REGION_CLASS(klass);
    // imrc->translate = smmu_translate;
    // imrc->notify_flag_changed = smmu_notify_flag_changed;
}

static const TypeInfo qcom_smmu_type_info = {
    .name          = TYPE_QCOM_SMMU,
    .parent        = TYPE_OF_SYS_BUS_DEVICE,
    .instance_size = sizeof(QcomSMMUState),
    .instance_init = qcom_smmu_instance_init,
    .class_size    = sizeof(QcomSMMUClass),
    .class_init    = qcom_smmu_class_init,
};

static const TypeInfo qcom_smmu_iommu_memory_region_info = {
    .parent = TYPE_IOMMU_MEMORY_REGION,
    .name = TYPE_QCOM_SMMU_IOMMU_MEMORY_REGION,
    .class_init = qcom_smmu_iommu_memory_region_class_init,
};

static void qcom_smmu_register_types(void)
{
    type_register_static(&qcom_smmu_type_info);
    type_register_static(&qcom_smmu_iommu_memory_region_info);
}

type_init(qcom_smmu_register_types)

