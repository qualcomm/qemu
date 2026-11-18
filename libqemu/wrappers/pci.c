/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "qapi/error.h"
#include "qom/object.h"
#include "hw/pci/pci_device.h"
#include "hw/pci/msi.h"
#include "hw/pci/msix.h"

#include "pci.h"

bool libqemu_pci_msi_notify(PCIDevice* pci_dev, unsigned int vector)
{
    if (!msi_present(pci_dev) || vector >= msi_nr_vectors_allocated(pci_dev)) {
        return false;
    }

    // same behavior as for real device: drop signal if already raised.
    if (!msi_enabled(pci_dev)) {
        return true;
    }

    msi_notify(pci_dev, vector);

    return true;
}

bool libqemu_pci_msix_notify(PCIDevice* pci_dev, unsigned int vector)
{
    if (!msix_present(pci_dev) || vector >= msix_nr_vectors_allocated(pci_dev)) {
        return false;
    }

    if (!msix_enabled(pci_dev)) {
        return true;
    }

    msix_notify(pci_dev, vector);
    return true;
}