#pragma once

#include "qemu/osdep.h"
#include "qom/object.h"
#include "system/memory.h"
#include "pci.h"
#include "pcie.h"
#include "pcie_sriov.h"
#include "pci_device.h"

#include <stdint.h>

#define TYPE_LIBQEMU_PCIE       "libqemu-pcie"
OBJECT_DECLARE_SIMPLE_TYPE(LibQemuPCIeState, LIBQEMU_PCIE)

#define TYPE_LIBQEMU_PCIE_VF       "libqemu-pcie-vf"
OBJECT_DECLARE_SIMPLE_TYPE(LibQemuPCIeVfState, LIBQEMU_PCIE_VF)

#define LIBQEMU_BARS PCI_ROM_SLOT

typedef struct LibQemuPCIeBar {
    /* public */
    MemoryRegion* mr;
    uint64_t mr_offset;
    uint64_t size;
    bool is_prefetchable;
    bool is_64b;

    /* private */
    MemoryRegion alias; // alias of @mr, with the right offset / size
    MemoryRegion container; // contains the mr alias, necessary for msi-x
} LibQemuPCIeBar;

typedef struct LibQemuPCIeVfBar {
    MemoryRegion* mr;
    uint64_t mr_offset;
    uint64_t size;
    uint64_t stride;
} LibQemuPCIeVfBar;

typedef struct LibQemuPCIeState {
    PCIDevice parent_obj;

    // PCI base config
    uint16_t vendor_id;
    uint16_t device_id;
    uint16_t subsystem_vendor_id;
    uint16_t subsystem_id;
    uint16_t class_id;
    uint8_t revision;

    // PCIe config
    uint8_t pcie_type;
    uint8_t pcie_cap_offset;

    // BAR
    size_t bars_nb;
    LibQemuPCIeBar bars[LIBQEMU_BARS];

    // VF
    uint16_t sriov_cap_offset;
    uint16_t vf_nb;
    LibQemuPCIeVfBar vf_bars[PCI_SRIOV_NUM_BARS];
    uint16_t vf_device_id;
    uint16_t vf_offset;
    uint16_t vf_stride;
    bool sriov_init;

    // MSI
    uint8_t msi_nb_vectors;
    uint8_t msi_cap_offset;
    bool msi_64b;
    bool msi_init;

    // MSI-X
    uint16_t msix_nb_vectors;
    uint8_t msix_table_bar;
    uint32_t msix_table_offset;
    uint8_t msix_pba_bar;
    uint32_t msix_pba_offset;
    bool msix_init;

    // PM
    bool pm_enabled;
    uint8_t pm_cap_offset;
    uint16_t pm_caps;
    uint16_t pm_control_reset;

    // PASID
    uint8_t pasid_width;
    uint16_t pasid_cap_offset;
    bool pasid_exec;
    bool pasid_priv;
} LibQemuPCIeState;

typedef struct LibQemuPCIeVfState {
    PCIDevice parent_obj;

    MemoryRegion bar_mrs[PCI_SRIOV_NUM_BARS];
} LibQemuPCIeVfState;