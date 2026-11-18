/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef _LIBQEMU_WRAPPERS_PCI_H
#define _LIBQEMU_WRAPPERS_PCI_H

#include <stdbool.h>

bool libqemu_pci_msi_notify(PCIDevice* pci_dev, unsigned int vector);
bool libqemu_pci_msix_notify(PCIDevice* pci_dev, unsigned int vector);

#endif