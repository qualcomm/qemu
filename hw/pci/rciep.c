/*
 * RCiEP: Root Complex Integrated Endpoint
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * PCIe endpoint on pcie.0, type PCI_EXP_TYPE_RC_END (0x9), with num-vfs
 * SR-IOV VFs sharing the PF BAR layout.
 *
 * BAR0 is a doorbell / comm-block aperture. A write with Addr[12]=1 rings a
 * doorbell (edge, value ignored). Primary doorbells (Addr[4:2] = index
 * 0-7) drive external interrupt lines. VF and secondary-page doorbell fan-out
 * is a follow-up.
 *
 * Properties:
 *   vendor-id / device-id / class-id / revision  PCI identity
 *   num-vfs         SR-IOV VF count (default 0; max 8 per the BAR0 decode)
 *   num-msi-vectors MSI vectors per PF (default 4; must be a power of two)
 *   private-base    RESERVED - not consumed yet (future CPU PRIVATE window)
 *   pf-shadow-base  RESERVED - not consumed yet (future config mirror)
 */

#include "qemu/osdep.h"
#include "hw/pci/pci_device.h"
#include "hw/pci/pcie.h"
#include "hw/pci/pcie_sriov.h"
#include "hw/pci/msi.h"
#include "hw/pci/msix.h"
#include "hw/core/qdev-properties.h"
#include "qapi/error.h"
#include "qemu/host-utils.h"
#include "qemu/module.h"
#include "qom/object.h"
#include "standard-headers/linux/pci_regs.h"
#include "system/memory.h"

#define TYPE_RCIEP     "rciep"
#define TYPE_RCIEP_VF  "rciep-vf"
OBJECT_DECLARE_SIMPLE_TYPE(RCiEP, RCIEP)
OBJECT_DECLARE_SIMPLE_TYPE(RCiEPVF, RCIEP_VF)

/* Capability offsets per the hardware register spec. */
#define RCIEP_PCIE_CAP_OFFSET   0x80
#define RCIEP_MSI_CAP_OFFSET    0xC0
#define RCIEP_MSIX_CAP_OFFSET   0xE0

/*
 * BAR0 doorbell/comm-block geometry (hardware register spec). The PF aperture
 * is 8 MiB: Addr[22]=PF/VF, Addr[21:19]=VF index (so at most 8 VFs),
 * Addr[18:0]=the per-function window. A VF therefore sees a 512 KiB (2^19)
 * slice. These are fixed by the address decode; bar0-size must equal
 * RCIEP_PF_BAR0_SIZE.
 */
#define RCIEP_PF_BAR0_SIZE   0x800000  /* 8 MiB; Addr[22] in range */
#define RCIEP_VF_BAR0_SIZE   0x80000   /* 512 KiB; window Addr[18:0] */
#define RCIEP_MAX_VFS        8         /* Addr[21:19] VF index = 3 bits */
#define RCIEP_VF_INDEX_SHIFT 19        /* VF slice = vf_index << 19 */

/*
 * The SR-IOV VF Device ID. QEMU derives the cap's VF DID from the PF as
 * (PF device_id + 1); keep the VF QOM type's device_id the same single source.
 */
#define RCIEP_PF_DEVICE_ID      0x0D0A
#define RCIEP_VF_DEVICE_ID      (RCIEP_PF_DEVICE_ID + 1)

#define RCIEP_NUM_BARS          PCI_ROM_SLOT

typedef struct RCiEPBar {
    MemoryRegion region;
    MemoryRegion* target;
    uint64_t size;
    bool is_64bits;
    bool prefetchable;
} RCiEPBar;

struct RCiEP {
    PCIDevice parent_obj;

    /* Properties */
    uint16_t vendor_id;
    uint16_t device_id;
    uint16_t class_id;
    uint8_t revision;
    uint16_t num_vfs;
    uint64_t pf_shadow_base;    /* RESERVED - not consumed yet */
    uint64_t private_base;      /* RESERVED - not consumed yet */

    uint8_t num_msi_vectors;

    uint16_t num_msix_vectors;
    uint8_t msix_table_bar;
    uint32_t msix_table_offset;
    uint8_t msix_pba_bar;
    uint32_t msix_pba_offset;

    RCiEPBar bars[RCIEP_NUM_BARS];
};

static bool rciep_validate_msix(RCiEP* d, Error **errp)
{
    if (d->num_msix_vectors > PCI_MSIX_FLAGS_QSIZE + 1) {
        error_setg(errp, "num-msix-vectors=%u must be smaller or equal to %u", d->num_msix_vectors, PCI_MSIX_FLAGS_QSIZE + 1);
        return false;
    }

    if (d->msix_table_bar >= RCIEP_NUM_BARS) {
        error_setg(errp, "msix-table-bar=%u must be smaller than %u", d->msix_table_bar, RCIEP_NUM_BARS);
        return false;
    }

    if (d->msix_pba_bar >= RCIEP_NUM_BARS) {
        error_setg(errp, "msix-pba-bar=%u must be smaller than %u", d->msix_pba_bar, RCIEP_NUM_BARS);
        return false;
    }

    if (d->bars[d->msix_table_bar].size == 0) {
        error_setg(errp, "MSI-X table uses BAR %d, but it's not configured", d->msix_table_bar);
        return false;
    }

    if (d->bars[d->msix_pba_bar].size == 0) {
        error_setg(errp, "MSI-X PBA uses BAR %d, but it's not configured", d->msix_pba_bar);
        return false;
    }

    return true;
}

static bool rciep_validate_bars(RCiEP *d, Error **errp)
{
    for (unsigned i = 0; i < RCIEP_NUM_BARS; ++i) {
        RCiEPBar* bar = &d->bars[i];

        if (bar->size == 0) {
            if (bar->target || bar->is_64bits || bar->prefetchable) {
                error_setg(errp, "BAR%u has properties set, but its size if 0", i);
                return false;
            }

            continue;
        }

        if (!bar->target) {
            error_setg(errp, "BAR%u has a size but no target MemoryRegion", i);
            return false;
        }

        if (!is_power_of_2(bar->size)) {
            error_setg(errp, "BAR%u size 0x%" PRIx64 "must be a power of 2", i, bar->size);
            return false;
        }

        if (bar->size > memory_region_size(bar->target)) {
            error_setg(errp, "BAR%u has a size bigger than the MemoryRegion size", i);
            return false;
        }

        if (i == RCIEP_NUM_BARS - 1 && bar->is_64bits) {
            error_setg(errp, "BAR%u cannot be 64 bits", i);
            return false;
        }

        if (bar->is_64bits) {
            // skip bar i+1 if 64bits
            i++;
            assert(i < RCIEP_NUM_BARS);
            RCiEPBar* next_bar = &d->bars[i];

            if (next_bar->size != 0 ||
                next_bar->target ||
                next_bar->is_64bits ||
                next_bar->prefetchable
            ) {
                error_setg(errp, "BAR%u is the upper half of 64-bit BAR%u, and must be unused", i, i - 1);
                return false;
            }
        }
    }

    return true;
}

static bool rciep_validate_vfs(RCiEP *d, Error **errp)
{
    RCiEPBar* bar0 = &d->bars[0];

    if (d->num_vfs == 0) {
        return true;
    }

    if (d->num_vfs > RCIEP_MAX_VFS) {
        error_setg(errp, "num-vfs=%u is above maximum %u", d->num_vfs, RCIEP_MAX_VFS);
        return false;
    }

    if (bar0->size != RCIEP_PF_BAR0_SIZE) {
        error_setg(errp, "bar0-size=0x%" PRIx64 " must be 0x%x with SR-IOV "
                   "(VF decode assumes an 8 MiB PF aperture)", bar0->size, RCIEP_PF_BAR0_SIZE);
        return false;
    }

    if (bar0->target == NULL) {
        error_setg(errp, "SR-IOV requires BAR0 to have a target MemoryRegion");
        return false;
    }


    if (bar0->is_64bits || bar0->prefetchable) {
        error_setg(errp, "SR-IOV requires BAR0 to be 32-bit and non-prefetchable");
        return false;
    }

    return true;
}

static void rciep_msix_uninit(RCiEP *d)
{
    PCIDevice *pci_dev = PCI_DEVICE(d);

    if (!msix_present(pci_dev)) {
        return;
    }

    msix_uninit(pci_dev,
        &d->bars[d->msix_table_bar].region,
        &d->bars[d->msix_pba_bar].region);
}

static void rciep_realize(PCIDevice *pci_dev, Error **errp)
{
    RCiEP *d = RCIEP(pci_dev);
    uint8_t *cfg = pci_dev->config;

    /* Validate parameters up front (silent misconfig otherwise). */
    if (d->num_vfs > RCIEP_MAX_VFS) {
        error_setg(errp, "num-vfs=%u exceeds max %d (BAR0 VF-index field)",
                   d->num_vfs, RCIEP_MAX_VFS);
        return;
    }

    if (d->num_msi_vectors && !is_power_of_2(d->num_msi_vectors)) {
        error_setg(errp, "num-msi-vectors=%u must be a non-zero power of two",
                   d->num_msi_vectors);
        return;
    }

    if (d->num_msi_vectors > 32) {
        error_setg(errp, "num-msi-vectors=%u must be smaller or equal to 32", d->num_msi_vectors);
        return;
    }

    if (d->num_msix_vectors && !rciep_validate_msix(d, errp)) {
        return;
    }

    if (!rciep_validate_bars(d, errp)) {
        return;
    }

    if (!rciep_validate_vfs(d, errp)) {
        return;
    }

    /* PCI identity */
    pci_set_word(cfg + PCI_VENDOR_ID,           d->vendor_id);
    pci_set_word(cfg + PCI_DEVICE_ID,           d->device_id);
    pci_set_byte(cfg + PCI_REVISION_ID,         d->revision);
    pci_config_set_class(cfg, d->class_id);
    pci_set_word(cfg + PCI_SUBSYSTEM_VENDOR_ID, d->vendor_id);
    pci_set_word(cfg + PCI_SUBSYSTEM_ID,        d->device_id);

    /* PCIe Express capability - RC_END (type 0x9) */
    if (pcie_cap_init(pci_dev, RCIEP_PCIE_CAP_OFFSET,
                      PCI_EXP_TYPE_RC_END, 0, errp) < 0) {
        return;
    }

    /*
     * MSI. msi_init(..., msi64=false, ...) advertises a 32-bit message
     * address, which is correct only if the platform MSI doorbell sits below
     * 4 GiB (true for the GICv2m target here). Per-vector masking is enabled.
     * Keep the vector count small so all PFs fit the platform IRQ pool - e.g.
     * a GICv2m with 64 SPIs is exhausted by 32 vectors x 2 PFs.
     */
    if (d->num_msi_vectors) {
        if (msi_init(pci_dev, RCIEP_MSI_CAP_OFFSET,
                    d->num_msi_vectors, false, true, errp) < 0) {
            goto err_pcie_cap;
        }
    }

    for (unsigned i = 0; i < RCIEP_NUM_BARS; ++i) {
        RCiEPBar* bar = &d->bars[i];

        if (bar->size == 0) {
            continue;
        }

        uint32_t flags = PCI_BASE_ADDRESS_SPACE_MEMORY |
            (bar->is_64bits ? PCI_BASE_ADDRESS_MEM_TYPE_64 : PCI_BASE_ADDRESS_MEM_TYPE_32);

        if (bar->prefetchable) {
            flags |= PCI_BASE_ADDRESS_MEM_PREFETCH;
        }

        gchar* bar_name = g_strdup_printf("rciep-bar%u", i);
        memory_region_init_alias(&bar->region, OBJECT(d), bar_name, bar->target, 0, bar->size);
        pci_register_bar(pci_dev, i, flags, &bar->region);
        g_free(bar_name);

        if (bar->is_64bits) {
            // skip bar i+1 if 64bits
            ++i;
        }
    }

    if (d->num_msix_vectors) {
        if (msix_init(pci_dev, d->num_msix_vectors,
            &d->bars[d->msix_table_bar].region,
            d->msix_table_bar, d->msix_table_offset,
            &d->bars[d->msix_pba_bar].region,
            d->msix_pba_bar, d->msix_pba_offset,
            RCIEP_MSIX_CAP_OFFSET, errp) < 0) {
                goto err_bar;
        }

        for (unsigned i = 0; i < d->num_msix_vectors; ++i) {
            msix_vector_use(pci_dev, i);
        }
    }


    /* SR-IOV extended capability */
    if (d->num_vfs > 0) {
        if (!pcie_sriov_pf_init(pci_dev, PCI_CONFIG_SPACE_SIZE, TYPE_RCIEP_VF,
                                RCIEP_VF_DEVICE_ID, d->num_vfs, d->num_vfs,
                                1, 1, errp)) {
            goto err_bar;
        }

        pcie_sriov_pf_init_vf_bar(pci_dev, 0,
                                  PCI_BASE_ADDRESS_MEM_TYPE_32 |
                                  PCI_BASE_ADDRESS_SPACE_MEMORY,
                                  RCIEP_VF_BAR0_SIZE);
    }

    return;

err_bar:
    /* The BAR memory region is torn down with the device. */
    rciep_msix_uninit(d);
    msi_uninit(pci_dev);
err_pcie_cap:
    pcie_cap_exit(pci_dev);
}

static void rciep_exit(PCIDevice *pci_dev)
{
    RCiEP *d = RCIEP(pci_dev);

    if (d->num_vfs > 0) {
        pcie_sriov_pf_exit(pci_dev);
    }

    rciep_msix_uninit(d);
    msi_uninit(pci_dev);
    pcie_cap_exit(pci_dev);
}

// note these prop names must keep the pattern barN-prop
#define DEFINE_RCIEP_BAR_PROP(_bar_id) \
    DEFINE_PROP_LINK("bar" #_bar_id "-target", RCiEP, bars[_bar_id].target, TYPE_MEMORY_REGION, MemoryRegion*), \
    DEFINE_PROP_UINT64("bar" #_bar_id "-size", RCiEP, bars[_bar_id].size, 0), \
    DEFINE_PROP_BOOL("bar" #_bar_id "-is-64bits", RCiEP, bars[_bar_id].is_64bits, false), \
    DEFINE_PROP_BOOL("bar" #_bar_id "-prefetchable", RCiEP, bars[_bar_id].prefetchable, false)

static const Property rciep_props[] = {
    DEFINE_PROP_UINT16("vendor-id",       RCiEP, vendor_id,      0x17CB),
    DEFINE_PROP_UINT16("device-id",       RCiEP, device_id,
                       RCIEP_PF_DEVICE_ID),
    DEFINE_PROP_UINT16("class-id",        RCiEP, class_id,       0x1200),
    DEFINE_PROP_UINT8("revision",         RCiEP, revision,       0x00),
    DEFINE_PROP_UINT16("num-vfs",         RCiEP, num_vfs,        0),
    DEFINE_PROP_UINT64("pf-shadow-base",  RCiEP, pf_shadow_base, 0),
    DEFINE_PROP_UINT64("private-base",    RCiEP, private_base,   0),

    DEFINE_PROP_UINT8("num-msi-vectors",  RCiEP, num_msi_vectors, 4),

    DEFINE_PROP_UINT16("num-msix-vectors", RCiEP, num_msix_vectors, 0),
    DEFINE_PROP_UINT8("msix-table-bar", RCiEP, msix_table_bar, 0),
    DEFINE_PROP_UINT32("msix-table-offset", RCiEP, msix_table_offset, 0),
    DEFINE_PROP_UINT8("msix-pba-bar", RCiEP, msix_pba_bar, 0),
    DEFINE_PROP_UINT32("msix-pba-offset", RCiEP, msix_pba_offset, 0),

    DEFINE_RCIEP_BAR_PROP(0),
    DEFINE_RCIEP_BAR_PROP(1),
    DEFINE_RCIEP_BAR_PROP(2),
    DEFINE_RCIEP_BAR_PROP(3),
    DEFINE_RCIEP_BAR_PROP(4),
    DEFINE_RCIEP_BAR_PROP(5),
};

static void rciep_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    PCIDeviceClass *pc = PCI_DEVICE_CLASS(klass);

    pc->realize  = rciep_realize;
    pc->exit     = rciep_exit;
    /*
     * Identity is authoritatively set from properties in realize(); these
     * class defaults just seed the QOM class. Keep them in sync with the
     * prop defaults.
     */
    pc->vendor_id = 0x17CB;
    pc->device_id = RCIEP_PF_DEVICE_ID;
    pc->class_id  = 0x1200;
    pc->revision  = 0x00;

    dc->desc = "RCiEP (Root Complex Integrated Endpoint)";
    device_class_set_props(dc, rciep_props);
    set_bit(DEVICE_CATEGORY_MISC, dc->categories);
}

static const TypeInfo rciep_type_info = {
    .name          = TYPE_RCIEP,
    .parent        = TYPE_PCI_DEVICE,
    .instance_size = sizeof(RCiEP),
    .class_init    = rciep_class_init,
    .interfaces    = (InterfaceInfo[]) {
        { INTERFACE_PCIE_DEVICE },
        { }
    },
};

/* ======================== VF device ======================== */

struct RCiEPVF {
    PCIDevice parent_obj;
    MemoryRegion bar0;
};

/*
 * A VF's 512 KiB BAR0 aliases into the PF doorbell/comm-block layout's VF
 * region. Reconstruct the PF-style address (Addr[22]=0 = VF, Addr[21:19] = vf
 * index) so the comm-block target decodes the correct VF doorbell/comm-block.
 * num-vfs is capped at RCIEP_MAX_VFS in realize, so vf_num fits the 3-bit
 * field.
 */
static hwaddr rciep_vf_commblock_addr(RCiEPVF *vf)
{
    uint16_t vf_num = pcie_sriov_vf_number(PCI_DEVICE(vf));

    assert(vf_num < RCIEP_MAX_VFS);

    return (hwaddr) vf_num << RCIEP_VF_INDEX_SHIFT;
}

static void rciep_vf_realize(PCIDevice *pci_dev, Error **errp)
{
    RCiEPVF *vf = RCIEP_VF(pci_dev);
    RCiEP* pf = RCIEP(pcie_sriov_get_pf(pci_dev));
    RCiEPBar* pf_bar0 = &pf->bars[0];

    memory_region_init_alias(&vf->bar0, OBJECT(vf), "rciep-vf-bar0", pf_bar0->target, rciep_vf_commblock_addr(vf), RCIEP_VF_BAR0_SIZE);

    /* Non-prefetchable: doorbell/comm-block aperture (see PF BAR0). */
    pci_register_bar(pci_dev, 0, PCI_BASE_ADDRESS_SPACE_MEMORY | PCI_BASE_ADDRESS_MEM_TYPE_32, &vf->bar0);
}

static void rciep_vf_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    PCIDeviceClass *pc = PCI_DEVICE_CLASS(klass);

    pc->realize   = rciep_vf_realize;
    pc->vendor_id = 0x17CB;
    pc->device_id = RCIEP_VF_DEVICE_ID;   /* single source: PF device_id + 1 */
    pc->class_id  = 0x1200;
    pc->revision  = 0x00;

    dc->desc = "RCiEP Virtual Function";
    set_bit(DEVICE_CATEGORY_MISC, dc->categories);
}

static const TypeInfo rciep_vf_type_info = {
    .name          = TYPE_RCIEP_VF,
    .parent        = TYPE_PCI_DEVICE,
    .instance_size = sizeof(RCiEPVF),
    .class_init    = rciep_vf_class_init,
    .interfaces    = (InterfaceInfo[]) {
        { INTERFACE_PCIE_DEVICE },
        { }
    },
};

/* ======================== Registration ======================== */

static void rciep_register_types(void)
{
    type_register_static(&rciep_type_info);
    type_register_static(&rciep_vf_type_info);
}

type_init(rciep_register_types)
