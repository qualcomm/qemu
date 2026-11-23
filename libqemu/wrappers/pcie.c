/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Configurable PCIe device, used as a configurable base device for more
 * complex PCIe devices. It supports up to 6 BARs with configurabl memory
 * regions, and many PCIe capabilities like PM, PASID, MSI, MSI-X,
 * SR-IOV, etc.
 */

#include "qemu/osdep.h"
#include "libqemu/wrappers/pcie.h"

#include "hw/pci/msi.h"
#include "hw/pci/msix.h"

#include "hw/pci/pcie_libqemu.h"

bool libqemu_pcie_msi_notify(PCIDevice* dev, unsigned vector)
{
    if (!dev->enabled ||
        !(pci_get_word(dev->config + PCI_COMMAND) & PCI_COMMAND_MASTER) ||
        !msi_enabled(dev) || vector >= msi_nr_vectors_allocated(dev)) {
        return false;
    }

    msi_notify(dev, vector);

    return true;
}

bool libqemu_pcie_msix_notify(PCIDevice* dev, unsigned vector)
{
    if (!dev->enabled ||
        !(pci_get_word(dev->config + PCI_COMMAND) & PCI_COMMAND_MASTER) ||
        !msix_enabled(dev) || vector >= msix_nr_vectors_allocated(dev)) {
        return false;
    }

    msix_notify(dev, vector);

    return true;
}

bool libqemu_pcie_register_callbacks(PCIDevice* dev, void* opaque, const LibQemuPCIeCallbacks* cbs)
{
    LibQemuPCIeState* s = LIBQEMU_PCIE(dev);

    if (cbs) {
        s->cbs = *cbs;
        s->cbs_opaque = cbs ? opaque : NULL;
    } else {
        memset(&s->cbs, 0, sizeof(s->cbs));
        s->cbs_opaque = NULL;
    }

    return true;
}

bool libqemu_pcie_clear_callbacks(PCIDevice* dev, void* opaque)
{
    LibQemuPCIeState* s = LIBQEMU_PCIE(dev);

    if (s->cbs_opaque != opaque) {
        return false;
    }

    s->cbs = (LibQemuPCIeCallbacks) { 0 };
    s->cbs_opaque = NULL;

    return true;
}

bool libqemu_pcie_callbacks_data(PCIDevice* dev, LibQemuPCIeCbData* cb_data)
{
    if (!cb_data) {
        return false;
    }

    LibQemuPCIeState* s = LIBQEMU_PCIE(dev);
    libqemu_pcie_get_cb_data(s, cb_data);

    return true;
}