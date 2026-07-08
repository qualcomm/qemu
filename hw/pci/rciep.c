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
 * doorbell (edge, value ignored); otherwise the access is forwarded to the
 * comm-block target at commblock-base. Primary doorbells (Addr[4:2] = index
 * 0-7) drive external interrupt lines. VF and secondary-page doorbell fan-out
 * is a follow-up.
 *
 * Properties:
 *   vendor-id / device-id / class-id / revision  PCI identity
 *   num-vfs         SR-IOV VF count (default 8; max 8 per the BAR0 decode)
 *   num-msi-vectors MSI vectors per PF (default 4; must be a power of two)
 *   bar0-size       PF BAR0 bytes (must be 8 MiB; VF BAR0 is 512 KiB)
 *   commblock-base  System-bus base the BAR0 aperture forwards to (0 = off)
 *   private-base    RESERVED - not consumed yet (future CPU PRIVATE window)
 *   pf-shadow-base  RESERVED - not consumed yet (future config mirror)
 */

#include "qemu/osdep.h"
#include "hw/pci/pci_device.h"
#include "hw/pci/pcie.h"
#include "hw/pci/pcie_sriov.h"
#include "hw/pci/msi.h"
#include "hw/core/qdev-properties.h"
#include "qapi/error.h"
#include "qemu/module.h"
#include "qom/object.h"
#include "system/address-spaces.h"

#define TYPE_RCIEP     "rciep"
#define TYPE_RCIEP_VF  "rciep-vf"
OBJECT_DECLARE_SIMPLE_TYPE(RCiEP, RCIEP)
OBJECT_DECLARE_SIMPLE_TYPE(RCiEPVF, RCIEP_VF)

/* Capability offsets per the hardware register spec. */
#define RCIEP_PCIE_CAP_OFFSET   0x80
#define RCIEP_MSI_CAP_OFFSET    0xC0

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

struct RCiEP {
    PCIDevice parent_obj;

    /* Properties */
    uint16_t vendor_id;
    uint16_t device_id;
    uint16_t class_id;
    uint8_t revision;
    uint16_t num_vfs;
    uint8_t num_msi_vectors;
    uint64_t bar0_size;
    uint64_t pf_shadow_base;    /* RESERVED - not consumed yet */
    uint64_t private_base;      /* RESERVED - not consumed yet */
    uint64_t commblock_base;    /* BAR0 forward target (external comm-block) */

    MemoryRegion bar0;
};

/*
 * BAR0 is a doorbell / comm-block aperture: forward the access verbatim to the
 * comm-block component on the system bus, which owns the doorbell decode +
 * comm-block SRAM. The full BAR0 offset is preserved so the target sees the
 * PF/VF/set/idx address bits.
 *
 * commblock_base must point at a dedicated comm-block target region, NOT back
 * into the PCI ECAM/MMIO aperture or DRAM - forwarding into the PCI aperture
 * could re-enter this device (recursion) or hit the wrong target. It is a
 * platform-wiring contract; 0 disables forwarding (deferred).
 */
static uint64_t rciep_mmio_read(void *opaque, hwaddr addr, unsigned size)
{
    RCiEP *d = opaque;
    uint64_t val = 0;

    if (d->commblock_base) {
        address_space_read(&address_space_memory, d->commblock_base + addr,
                           MEMTXATTRS_UNSPECIFIED, &val, size);
    }
    return val;
}

static void rciep_mmio_write(void *opaque, hwaddr addr, uint64_t val,
                             unsigned size)
{
    RCiEP *d = opaque;

    if (d->commblock_base) {
        address_space_write(&address_space_memory, d->commblock_base + addr,
                            MEMTXATTRS_UNSPECIFIED, &val, size);
    }
}

static const MemoryRegionOps rciep_mmio_ops = {
    .read       = rciep_mmio_read,
    .write      = rciep_mmio_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid      = { .min_access_size = 4, .max_access_size = 8 },
};

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
    if (d->num_msi_vectors == 0 ||
        (d->num_msi_vectors & (d->num_msi_vectors - 1))) {
        error_setg(errp, "num-msi-vectors=%u must be a non-zero power of two",
                   d->num_msi_vectors);
        return;
    }
    if (d->num_vfs > 0 && d->bar0_size != RCIEP_PF_BAR0_SIZE) {
        error_setg(errp, "bar0-size=0x%" PRIx64 " must be 0x%x with SR-IOV "
                   "(VF decode assumes an 8 MiB PF aperture)",
                   d->bar0_size, RCIEP_PF_BAR0_SIZE);
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
    if (msi_init(pci_dev, RCIEP_MSI_CAP_OFFSET,
                 d->num_msi_vectors, false, true, errp) < 0) {
        goto err_pcie_cap;
    }

    /*
     * BAR0: 32-bit non-prefetchable MMIO. Non-prefetchable because it is a
     * doorbell / comm-block aperture with read/write side effects -
     * prefetchable would permit read prefetch and write coalescing/reordering,
     * corrupting doorbell semantics. 32-bit per the hardware register spec.
     */
    memory_region_init_io(&d->bar0, OBJECT(d), &rciep_mmio_ops, d,
                          "rciep-bar0", d->bar0_size);
    pci_register_bar(pci_dev, 0,
                     PCI_BASE_ADDRESS_SPACE_MEMORY |
                     PCI_BASE_ADDRESS_MEM_TYPE_32,
                     &d->bar0);

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
    msi_uninit(pci_dev);
    pcie_cap_exit(pci_dev);
}

static const Property rciep_props[] = {
    DEFINE_PROP_UINT16("vendor-id",       RCiEP, vendor_id,      0x17CB),
    DEFINE_PROP_UINT16("device-id",       RCiEP, device_id,
                       RCIEP_PF_DEVICE_ID),
    DEFINE_PROP_UINT16("class-id",        RCiEP, class_id,       0x1200),
    DEFINE_PROP_UINT8("revision",         RCiEP, revision,       0x00),
    DEFINE_PROP_UINT16("num-vfs",         RCiEP, num_vfs,        RCIEP_MAX_VFS),
    DEFINE_PROP_UINT8("num-msi-vectors",  RCiEP, num_msi_vectors, 4),
    DEFINE_PROP_UINT64("bar0-size",       RCiEP, bar0_size,
                       RCIEP_PF_BAR0_SIZE),
    DEFINE_PROP_UINT64("pf-shadow-base",  RCiEP, pf_shadow_base, 0),
    DEFINE_PROP_UINT64("private-base",    RCiEP, private_base,   0),
    DEFINE_PROP_UINT64("commblock-base",  RCiEP, commblock_base, 0),
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
static hwaddr rciep_vf_commblock_addr(RCiEPVF *vf, hwaddr addr)
{
    uint16_t vf_num = pcie_sriov_vf_number(PCI_DEVICE(vf));

    return ((hwaddr)(vf_num % RCIEP_MAX_VFS) << RCIEP_VF_INDEX_SHIFT) |
           (addr & (RCIEP_VF_BAR0_SIZE - 1));
}

static uint64_t rciep_vf_mmio_read(void *opaque, hwaddr addr, unsigned size)
{
    RCiEPVF *vf = opaque;
    RCiEP *d = RCIEP(pcie_sriov_get_pf(PCI_DEVICE(vf)));

    return rciep_mmio_read(d, rciep_vf_commblock_addr(vf, addr), size);
}

static void rciep_vf_mmio_write(void *opaque, hwaddr addr, uint64_t val,
                                unsigned size)
{
    RCiEPVF *vf = opaque;
    RCiEP *d = RCIEP(pcie_sriov_get_pf(PCI_DEVICE(vf)));

    rciep_mmio_write(d, rciep_vf_commblock_addr(vf, addr), val, size);
}

static const MemoryRegionOps rciep_vf_mmio_ops = {
    .read       = rciep_vf_mmio_read,
    .write      = rciep_vf_mmio_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid      = { .min_access_size = 4, .max_access_size = 8 },
};

static void rciep_vf_realize(PCIDevice *pci_dev, Error **errp)
{
    RCiEPVF *vf = RCIEP_VF(pci_dev);

    memory_region_init_io(&vf->bar0, OBJECT(vf), &rciep_vf_mmio_ops, vf,
                          "rciep-vf-bar0", RCIEP_VF_BAR0_SIZE);
    /* Non-prefetchable: doorbell/comm-block aperture (see PF BAR0). */
    pci_register_bar(pci_dev, 0,
                     PCI_BASE_ADDRESS_SPACE_MEMORY |
                     PCI_BASE_ADDRESS_MEM_TYPE_32,
                     &vf->bar0);
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
