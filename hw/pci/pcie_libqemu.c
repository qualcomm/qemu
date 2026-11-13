#include "qemu/osdep.h"

#include "hw/pci/pci.h"
#include "hw/pci/pcie_libqemu.h"
#include "hw/pci/pci_device.h"
#include "hw/pci/pcie.h"
#include "hw/pci/msi.h"
#include "hw/pci/msix.h"
#include "hw/core/resettable.h"

#include "hw/core/qdev-properties.h"
#include "hw/pci/pcie_regs.h"
#include "hw/pci/pcie_sriov.h"
#include "standard-headers/linux/pci_regs.h"
#include "glib.h"
#include "system/memory.h"

#define INVALID_VENDOR_ID 0xffff
#define INVALID_DEVICE_ID 0xffff
#define INVALID_REVISION 0xff
#define INVALID_CLASS_ID 0xffff

#define DEFAULT_PCIE_CAP_OFFSET 0x80
#define DEFAULT_MSI_CAP_OFFSET 0xc0
#define DEFAULT_PM_CAP_OFFSET 0xe0
#define DEFAULT_SRIOV_CAP_OFFSET 0x100
#define DEFAULT_PASID_CAP_OFFSET 0x140

#define DEFAULT_PM_CAPABILITIES 3

static bool check_bar(LibQemuPCIeState* s, LibQemuPCIeBar* bar, uint8_t idx, Error** errp)
{
    if (!bar->size) {
        return true;
    }

    if (!bar->mr) {
        error_setg(errp, "bar%u-mr must be non-null", idx);
        return false;
    }

    if (idx < s->bars_nb - 1 && bar->is_64b && s->bars[idx + 1].size != 0) {
        error_setg(errp, "bar%u-mr must be non-null", idx);
        return false;
    }

    if (idx == s->bars_nb - 1 && bar->is_64b) {
        error_setg(errp, "bar%u-mr is the last BAR, which cannot be 64b", idx);
        return false;
    }

    return true;
}

static void libqemu_pcie_config_write(PCIDevice* dev, uint32_t addr, uint32_t val, int len)
{
    pci_default_write_config(dev, addr, val, len);
    pcie_cap_flr_write_config(dev, addr, val, len);
}

static bool libqemu_pcie_init_common(PCIDevice* dev, const LibQemuPCIeState* s, Error** errp)
{
    pci_config_set_class(dev->config, s->class_id);
    pci_config_set_revision(dev->config, s->revision);

    if (pcie_cap_init(dev, s->pcie_cap_offset, s->pcie_type, 0, errp) < 0) {
        return false;
    }

    pcie_cap_flr_init(dev);

    return true;
}

static uint8_t encode_pci_attrs(const LibQemuPCIeBar* bar)
{
    uint8_t attr = PCI_BASE_ADDRESS_SPACE_MEMORY;

    if (bar->is_64b) {
        attr |= PCI_BASE_ADDRESS_MEM_TYPE_64;
    }

    if (bar->is_prefetchable) {
        attr |= PCI_BASE_ADDRESS_MEM_PREFETCH;
    }

    return attr;
}

static void libqemu_pcie_reset_hold(Object* obj, ResetType type)
{
    PCIDevice* dev = PCI_DEVICE(obj);

    if (dev->exp.pasid_cap) {
        pci_set_word(dev->config + dev->exp.pasid_cap + PCI_PASID_CTRL, 0);
    }
}

static void libqemu_pcie_realize(PCIDevice* dev, Error** errp)
{
    LibQemuPCIeState* s = LIBQEMU_PCIE(dev);

    const uint8_t nmsi = s->msi_nb_vectors;
    if (nmsi > 32) {
        error_setg(errp, "msi-nb-vectors must be <= 32");
        return;
    }

    if (s->msix_nb_vectors > PCI_MSIX_FLAGS_QSIZE + 1) {
        error_setg(errp, "msix-nb-vectors must be <= %d", PCI_MSIX_FLAGS_QSIZE);
        return;
    }

    if (s->pasid_width > PCI_EXT_CAP_PASID_MAX_WIDTH) {
        error_setg(errp, "pasid-width must be <= %d", PCI_EXT_CAP_PASID_MAX_WIDTH);
        return;
    }

    for (uint8_t i = 0; i < LIBQEMU_BARS; ++i) {
        if (!check_bar(s, &s->bars[i], i, errp)) {
            return;
        }
    }

    if (s->pasid_width && !s->pasid_cap_offset) {
        if (s->vf_nb && s->sriov_cap_offset == PCI_CONFIG_SPACE_SIZE) {
            s->pasid_cap_offset = DEFAULT_PASID_CAP_OFFSET;
        } else {
            s->pasid_cap_offset = PCI_CONFIG_SPACE_SIZE;
        }
    }

    uint8_t* config = dev->config;
    pci_config_set_vendor_id(config, s->vendor_id);
    pci_config_set_device_id(config, s->device_id);
    pci_set_word(config + PCI_SUBSYSTEM_VENDOR_ID, s->subsystem_vendor_id);
    pci_set_word(config + PCI_SUBSYSTEM_ID, s->subsystem_id);

    if (!libqemu_pcie_init_common(dev, s, errp)) {
        return;
    }

    if (s->pm_enabled) {
        int pm = pci_pm_init(dev, s->pm_cap_offset, errp);

        if(pm < 0) {
            return;
        }

        pci_set_word(config + pm + PCI_PM_PMC, s->pm_caps);
        pci_set_word(config + pm + PCI_PM_CTRL, s->pm_control_reset);
        pci_set_word(dev->wmask + pm + PCI_PM_CTRL, PCI_PM_CTRL_STATE_MASK);
    }

    if (s->pasid_width && (!s->vf_nb || s->pasid_cap_offset < s->sriov_cap_offset)) {
        pcie_pasid_init(dev, s->pasid_cap_offset, s->pasid_width, s->pasid_exec, s->pasid_priv);
    }

    if (s->msi_nb_vectors) {
        if (msi_init(dev, s->msi_cap_offset, s->msi_nb_vectors, s->msi_64b, true, errp) < 0) {
            return;
        }
    }

    for (unsigned i = 0; i < LIBQEMU_BARS; ++i) {
        LibQemuPCIeBar* bar = &s->bars[i];

        g_autofree char* name = NULL;

        if (!bar->size) {
            continue;
        }

        name = g_strdup_printf("pcie-libqemu-bar%u", i);
        memory_region_init(&bar->container, OBJECT(s), name, bar->size);
        memory_region_init_alias(&bar->alias, OBJECT(s), "bar-alias", bar->mr, bar->mr_offset, bar->size);
        memory_region_add_subregion_overlap(&bar->container, 0, &bar->alias, -1);

        pci_register_bar(dev, i, encode_pci_attrs(bar), &bar->container);
    }

    if (s->msix_nb_vectors) {
        if (s->msix_pba_bar >= LIBQEMU_BARS || s->msix_table_bar >= LIBQEMU_BARS) {
            error_setg(errp, "MSI-X error: bar does not exist");
            return;
        }

        if (!s->bars[s->msix_pba_bar].size || !s->bars[s->msix_table_bar].size) {
            error_setg(errp, "MSI-X error: bar is not configured");
            return;
        }

        if (msix_init(dev, s->msix_nb_vectors,
                    &s->bars[s->msix_table_bar].container, s->msix_table_bar, s->msix_table_offset,
                    &s->bars[s->msix_pba_bar].container, s->msix_pba_bar, s->msix_pba_offset,
                    0, errp) < 0) {
            return;
        }

        for (unsigned v = 0; v < s->msix_nb_vectors; ++v) {
            msix_vector_use(dev, v);
        }
    }

    if (s->vf_nb) {
        if (!pcie_sriov_pf_init(dev, s->sriov_cap_offset, TYPE_LIBQEMU_PCIE_VF,
                    s->vf_device_id, s->vf_nb, s->vf_nb,
                    s->vf_offset, s->vf_stride, errp)) {
            return;
        }

        for (size_t vf_bar = 0; vf_bar < PCI_SRIOV_NUM_BARS; ++vf_bar) {
            if (s->vf_bars[vf_bar].size) {
                pcie_sriov_pf_init_vf_bar(dev, vf_bar, encode_pci_attrs(&s->bars[vf_bar]), s->vf_bars[vf_bar].size);
            }
        }

        if (s->pasid_width && s->pasid_cap_offset > s->sriov_cap_offset) {
            pcie_pasid_init(dev, s->pasid_cap_offset, s->pasid_width, s->pasid_exec, s->pasid_priv);
        }
    }
}

static void libqemu_pcie_vf_realize(PCIDevice* dev, Error** errp)
{
    LibQemuPCIeVfState* vf = LIBQEMU_PCIE_VF(dev);
    LibQemuPCIeState* s = LIBQEMU_PCIE(pcie_sriov_get_pf(dev));
    uint16_t vf_idx = pcie_sriov_vf_number(dev);

    if (!libqemu_pcie_init_common(dev, s, errp)) {
        return;
    }

    for (size_t bar_idx = 0; bar_idx < PCI_SRIOV_NUM_BARS; ++bar_idx) {
        LibQemuPCIeVfBar* bar = &s->vf_bars[bar_idx];
        g_autofree char* name = NULL;

        if (!bar->size) {
            continue;
        }

        name = g_strdup_printf("pcie-libqemu-vf%u-bar%lu", vf_idx, bar_idx);

        const uint64_t offset = bar->mr_offset + vf_idx * bar->stride;
        memory_region_init_alias(&vf->bar_mrs[bar_idx], OBJECT(vf), name, bar->mr, offset, bar->size);

        pci_register_bar(dev, bar_idx, encode_pci_attrs(&s->bars[bar_idx]), &vf->bar_mrs[bar_idx]);
    }
}

#define BAR_PROP(_n) \
    DEFINE_PROP_LINK("bar" #_n "-mr", LibQemuPCIeState, bars[_n].mr, TYPE_MEMORY_REGION, MemoryRegion*), \
    DEFINE_PROP_UINT64("bar" #_n "-mr-offset", LibQemuPCIeState, bars[_n].mr_offset, 0), \
    DEFINE_PROP_UINT64("bar" #_n "-size", LibQemuPCIeState, bars[_n].size, 0), \
    DEFINE_PROP_BOOL("bar" #_n "-prefetchable", LibQemuPCIeState, bars[_n].is_prefetchable, false), \
    DEFINE_PROP_BOOL("bar" #_n "-64b", LibQemuPCIeState, bars[_n].is_64b, false)

#define VF_BAR_PROP(_n) \
    DEFINE_PROP_LINK("vf-bar" #_n "-mr", LibQemuPCIeState, vf_bars[_n].mr, TYPE_MEMORY_REGION, MemoryRegion*), \
    DEFINE_PROP_UINT64("vf-bar" #_n "-mr-offset", LibQemuPCIeState, vf_bars[_n].mr_offset, 0), \
    DEFINE_PROP_UINT64("vf-bar" #_n "-size", LibQemuPCIeState, vf_bars[_n].size, 0), \
    DEFINE_PROP_UINT64("vf-bar" #_n "-stride", LibQemuPCIeState, vf_bars[_n].stride, 0)

static const Property libqemu_pcie_properties[] = {
    // PCI config
    DEFINE_PROP_UINT16("vendor-id", LibQemuPCIeState, vendor_id, INVALID_VENDOR_ID),
    DEFINE_PROP_UINT16("device-id", LibQemuPCIeState, device_id, INVALID_DEVICE_ID),
    DEFINE_PROP_UINT16("subsystem-vendor-id", LibQemuPCIeState, subsystem_vendor_id, INVALID_VENDOR_ID),
    DEFINE_PROP_UINT16("subsystem-id", LibQemuPCIeState, subsystem_id, INVALID_DEVICE_ID),
    DEFINE_PROP_UINT16("class-id", LibQemuPCIeState, class_id, INVALID_CLASS_ID),
    DEFINE_PROP_UINT8("revision", LibQemuPCIeState, revision, INVALID_REVISION),

    // PCIe config
    DEFINE_PROP_UINT8("pcie-type", LibQemuPCIeState, pcie_type, PCI_EXP_TYPE_ENDPOINT),
    DEFINE_PROP_UINT8("pcie-cap-offset", LibQemuPCIeState, pcie_cap_offset, DEFAULT_PCIE_CAP_OFFSET),

    // BAR config
    DEFINE_PROP_SIZE("bars-nb", LibQemuPCIeState, bars_nb, PCI_ROM_SLOT),
    BAR_PROP(0),
    BAR_PROP(1),
    BAR_PROP(2),
    BAR_PROP(3),
    BAR_PROP(4),
    BAR_PROP(5),

    // SRIOV
    DEFINE_PROP_UINT16("sriov-cap-offset", LibQemuPCIeState, sriov_cap_offset, DEFAULT_SRIOV_CAP_OFFSET),
    DEFINE_PROP_UINT16("vf-nb", LibQemuPCIeState, vf_nb, 0),
    DEFINE_PROP_UINT16("vf-device-id", LibQemuPCIeState, vf_device_id, INVALID_DEVICE_ID),
    DEFINE_PROP_UINT16("vf-offset", LibQemuPCIeState, vf_offset, 1),
    DEFINE_PROP_UINT16("vf-stride", LibQemuPCIeState, vf_stride, 1),
    VF_BAR_PROP(0),
    VF_BAR_PROP(1),
    VF_BAR_PROP(2),
    VF_BAR_PROP(3),
    VF_BAR_PROP(4),
    VF_BAR_PROP(5),

    // MSI
    DEFINE_PROP_UINT8("msi-nb-vectors", LibQemuPCIeState, msi_nb_vectors, 0),
    DEFINE_PROP_UINT8("msi-cap-offset", LibQemuPCIeState, msi_cap_offset, DEFAULT_MSI_CAP_OFFSET),
    DEFINE_PROP_BOOL("msi-64b", LibQemuPCIeState, msi_64b, false),

    // MSI-X
    DEFINE_PROP_UINT16("msix-nb-vectors", LibQemuPCIeState, msix_nb_vectors, 0),
    DEFINE_PROP_UINT8("msix-table-bar", LibQemuPCIeState, msix_table_bar, 0),
    DEFINE_PROP_UINT32("msix-table-offset", LibQemuPCIeState, msix_table_offset, 0),
    DEFINE_PROP_UINT8("msix-pba-bar", LibQemuPCIeState, msix_pba_bar, 0),
    DEFINE_PROP_UINT32("msix-pba-offset", LibQemuPCIeState, msix_pba_offset, 0),

    // PM
    DEFINE_PROP_BOOL("pm-enabled", LibQemuPCIeState, pm_enabled, false),
    DEFINE_PROP_UINT8("pm-cap-offset", LibQemuPCIeState, pm_cap_offset, DEFAULT_PM_CAP_OFFSET),
    DEFINE_PROP_UINT16("pm-capabilities", LibQemuPCIeState, pm_caps, DEFAULT_PM_CAPABILITIES),
    DEFINE_PROP_UINT16("pm-control-reset", LibQemuPCIeState, pm_control_reset, 0),

    // PASID
    DEFINE_PROP_UINT8("pasid-width", LibQemuPCIeState, pasid_width, 0),
    DEFINE_PROP_UINT16("pasid-cap-offset", LibQemuPCIeState, pasid_cap_offset, 0),
    DEFINE_PROP_BOOL("pasid-exec", LibQemuPCIeState, pasid_exec, false),
    DEFINE_PROP_BOOL("pasid-priv", LibQemuPCIeState, pasid_priv, false),
};

static void libqemu_pcie_class_init(ObjectClass* class, const void* data)
{
    DeviceClass* dc = DEVICE_CLASS(class);
    PCIDeviceClass* pdc = PCI_DEVICE_CLASS(dc);
    ResettableClass* rc = RESETTABLE_CLASS(class);

    rc->phases.hold = libqemu_pcie_reset_hold;

    pdc->realize = libqemu_pcie_realize;
    pdc->exit = pcie_cap_exit;
    pdc->config_write = libqemu_pcie_config_write;
    pdc->vendor_id = INVALID_VENDOR_ID;
    pdc->device_id = INVALID_VENDOR_ID;
    pdc->revision = INVALID_REVISION;
    pdc->class_id = INVALID_CLASS_ID;
    pdc->subsystem_vendor_id = INVALID_VENDOR_ID;
    pdc->subsystem_id = INVALID_DEVICE_ID;

    device_class_set_props(dc, libqemu_pcie_properties);
}

static void libqemu_pcie_vf_class_init(ObjectClass* class, const void* data)
{
    DeviceClass* dc = DEVICE_CLASS(class);
    PCIDeviceClass* pdc = PCI_DEVICE_CLASS(dc);

    pdc->realize = libqemu_pcie_vf_realize;
    pdc->exit = pcie_cap_exit;
    pdc->config_write = libqemu_pcie_config_write;
    pdc->vendor_id = INVALID_VENDOR_ID;
    pdc->device_id = INVALID_VENDOR_ID;
}

static const TypeInfo libqemu_pcie_info = {
    .name = TYPE_LIBQEMU_PCIE,
    .parent = TYPE_PCI_DEVICE,
    .instance_size = sizeof(LibQemuPCIeState),
    .class_init = libqemu_pcie_class_init,
    .interfaces = (const InterfaceInfo[]) {
        { INTERFACE_PCIE_DEVICE },
        { }
    },
};

static const TypeInfo libqemu_pcie_vf_info = {
    .name = TYPE_LIBQEMU_PCIE_VF,
    .parent = TYPE_PCI_DEVICE,
    .instance_size = sizeof(LibQemuPCIeVfState),
    .class_init = libqemu_pcie_vf_class_init,
    .interfaces = (const InterfaceInfo[]) {
        { INTERFACE_PCIE_DEVICE },
        { }
    },
};

static void libqemu_pcie_register_types(void)
{
    type_register_static(&libqemu_pcie_info);
    type_register_static(&libqemu_pcie_vf_info);
}

type_init(libqemu_pcie_register_types);