/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "hw/pci/pci_device.h"
#include "hw/pci/msi.h"

#include "../pci.h"

bool libqemu_pci_msi_notify(PCIDevice* pci_dev, unsigned int vector)
{
    (void) pci_dev;
    (void) vector;

    return false;
}

bool libqemu_pci_msix_notify(PCIDevice* pci_dev, unsigned int vector)
{
    (void) pci_dev;
    (void) vector;

    return false;
}