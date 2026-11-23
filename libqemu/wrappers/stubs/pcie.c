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

#include "libqemu/wrappers/pcie.h"

bool libqemu_pcie_msi_notify(PCIDevice* dev, unsigned vector)
{
    (void)dev;
    (void)vector;
    return false;

}

bool libqemu_pcie_msix_notify(PCIDevice* dev, unsigned vector)
{
    (void)dev;
    (void)vector;
    return false;
}

bool libqemu_pcie_register_callbacks(PCIDevice* dev, void* opaque, const LibQemuPCIeCallbacks* cbs)
{
    (void) dev;
    (void) opaque;
    (void) cbs;
    return false;
}

bool libqemu_pcie_clear_callbacks(PCIDevice* dev, void* opaque)
{
    (void) dev;
    (void) opaque;
    return false;
}

bool libqemu_pcie_callbacks_data(PCIDevice* dev, LibQemuPCIeCbData* cb_data)
{
    (void)dev;
    (void)cb_data;
    return false;
}