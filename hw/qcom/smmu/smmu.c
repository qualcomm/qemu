#include "qemu/osdep.h"
#include "hw/sysbus-of.h"
#include "hw/qcom/smmu.h"
#include "hw/qdev-properties.h"
#include "qapi/error.h"
#include "exec/memory.h"
#include "qemu/error-report.h"

#define SMMU_LOG(dev, fmt, ...) QDEV_LOG_INFO(dev, fmt __VA_OPT__(,) __VA_ARGS__)
#define SMMU_LOG_WARN(dev, fmt, ...) QDEV_LOG_WARN(dev, fmt __VA_OPT__(,) __VA_ARGS__)
#define SMMU_LOG_ERROR(dev, fmt, ...) QDEV_LOG_ERROR(dev, fmt __VA_OPT__(,) __VA_ARGS__)

/* Configuration registers for the franksmmu device */
#define QCOM_FRANKSMMU_PADDR_LO	    0x39000
#define QCOM_FRANKSMMU_PADDR_HI	    0x39004
#define QCOM_FRANKSMMU_IOVA_LO		0x39008
#define QCOM_FRANKSMMU_IOVA_HI		0x3900c
#define QCOM_FRANKSMMU_PGSIZE		0x39010
#define QCOM_FRANKSMMU_PGCOUNT		0x39014
#define QCOM_FRANKSMMU_PERM		    0x39018
#define QCOM_FRANKSMMU_VMID		    0x3901c
#define QCOM_FRANKSMMU_COMMIT		0x39020

#define BITS32_MASK                 0xffffffff

const DMAMap* qcom_smmu_iova2paddr(struct QcomSMMUState* s, uint32_t vmid, uint64_t iova, uint64_t size)
{
    assert (size > 0);

    SMMU_LOG(s, "Translation requested:\n");
    SMMU_LOG(s, "\tVMID: %d\n", vmid);
    SMMU_LOG(s, "\tiova: 0x%lx\n", iova);
    SMMU_LOG(s, "\tsize: 0x%lx\n", size);

    DMAMap needle = {
        .iova = iova,
        .size = size - 1,
    };

    assert(s->franksmmu_state.domains[vmid]);
    assert(s->franksmmu_state.domains[vmid]->maps);
    assert(vmid < s->franksmmu_state.nb_domains);

    const DMAMap* res = iova_tree_find(s->franksmmu_state.domains[vmid]->maps, &needle);

    return res;
}

static uint64_t qcom_smmu_read(void *opaque, hwaddr addr, unsigned size)
{
    SMMU_LOG(opaque, "Read at address 0x%lx\n", addr);

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
            SMMU_LOG_WARN(opaque, "Unhandled read @addr 0x%lx\n", addr);
            return 0;
    }
}

static void print_dma_map(QcomSMMUState* s, DMAMap* map, uint32_t vmid, bool remove)
{
    const char* status = remove ? "Delete" : "Add";
    SMMU_LOG(s, "%s entry with VMID %d\n", status, vmid);
    SMMU_LOG(s, "\t- iova: 0x%lx\n", map->iova);
    SMMU_LOG(s, "\t- size: 0x%lx\n", map->size);
    if (!remove) {
        SMMU_LOG(s, "\t- paddr: 0x%lx\n", map->translated_addr);
        SMMU_LOG(s, "\t- perm: 0x%x\n", map->perm);
    }
}

static void qcom_smmu_write(void *opaque, hwaddr addr,
                              uint64_t _value, unsigned int size)
{
    QcomSMMUState *s = QCOM_SMMU(opaque);
    uint32_t value = _value;

    SMMU_LOG(s, "Write at address 0x%lx of value 0x%x\n", addr, value);

    switch (addr) {
        case QCOM_FRANKSMMU_PADDR_LO: {
            s->franksmmu_state.cached_map.translated_addr &= ~BITS32_MASK;
            s->franksmmu_state.cached_map.translated_addr |= value;
            break;
        }
        case QCOM_FRANKSMMU_PADDR_HI: {
            s->franksmmu_state.cached_map.translated_addr &= BITS32_MASK;
            s->franksmmu_state.cached_map.translated_addr |= (_value << 32);
            break;
        }
        case QCOM_FRANKSMMU_IOVA_LO: {
            s->franksmmu_state.cached_map.iova &= ~BITS32_MASK;
            s->franksmmu_state.cached_map.iova |= value;
            break;
        }
        case QCOM_FRANKSMMU_IOVA_HI: {
            s->franksmmu_state.cached_map.iova &= BITS32_MASK;
            s->franksmmu_state.cached_map.iova |= (_value << 32);
            break;
        }
        case QCOM_FRANKSMMU_PGSIZE: {
            s->franksmmu_state.cached_map.size = value;
            break;
        }
        case QCOM_FRANKSMMU_PGCOUNT: {
            s->franksmmu_state.cached_pgcount = value;
            break;
        }
        case QCOM_FRANKSMMU_PERM: {
            s->franksmmu_state.cached_map.perm = value;
            break;
        }
        case QCOM_FRANKSMMU_VMID: {
            s->franksmmu_state.cached_vmid = value;
            break;
        }
        case QCOM_FRANKSMMU_COMMIT: {
            if (value & BIT(0)) {
                struct DMAMap* map = &s->franksmmu_state.cached_map;
                uint32_t vmid = s->franksmmu_state.cached_vmid;

                map->size *= s->franksmmu_state.cached_pgcount;
                map->size--;

                if (s->franksmmu_state.cached_vmid > 1024) {
                    error_report("a high value of VMID has been provided. It's either a bug or the current domain implementation should be changed for a hashmap.");
                    exit(1);
                }

                if (vmid >= s->franksmmu_state.nb_domains) {
                    uint64_t nb_new_bytes = (vmid + 1 - s->franksmmu_state.nb_domains) * sizeof(struct smmu_franksmmu_domain*);
                    uint64_t old_nb_domains = s->franksmmu_state.nb_domains;

                    s->franksmmu_state.domains = g_realloc_n(s->franksmmu_state.domains, vmid + 1, sizeof(struct smmu_franksmmu_domain*));
                    s->franksmmu_state.nb_domains = vmid + 1;

                    memset(s->franksmmu_state.domains + old_nb_domains, 0, nb_new_bytes);
                }
                assert(vmid < s->franksmmu_state.nb_domains);

                if (!s->franksmmu_state.domains[vmid]) {
                    s->franksmmu_state.domains[vmid] = g_new(struct franksmmu_domain, 1);
                    s->franksmmu_state.domains[vmid]->maps = iova_tree_new();
                }

                print_dma_map(s, map, vmid, false);

                struct franksmmu_domain* domain = s->franksmmu_state.domains[vmid];
                iova_tree_insert(domain->maps, map);
            } else if (value & BIT(1)) {
                struct DMAMap* map = &s->franksmmu_state.cached_map;
                uint32_t vmid = s->franksmmu_state.cached_vmid;

                map->size *= s->franksmmu_state.cached_pgcount;
                map->size--;

                print_dma_map(s, map, vmid, true);

                assert(s->franksmmu_state.domains[vmid]);

                iova_tree_remove(s->franksmmu_state.domains[vmid]->maps, *map);
            }
            break;
        }
        default:
            SMMU_LOG_WARN(s, "Unknown write @addr 0x%lx of value 0x%x\n", addr, value);
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

	assert(ofdev->mem_size);
    memory_region_init_io(&s->iomem, OBJECT(ofdev), &qcom_smmu_ops, s, TYPE_QCOM_SMMU, ofdev->mem_size);
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

