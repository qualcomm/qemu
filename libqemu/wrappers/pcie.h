#pragma once

#include "qemu/osdep.h"
#include <stdbool.h>

bool libqemu_pcie_msi_notify(PCIDevice* dev, unsigned vector);
bool libqemu_pcie_msix_notify(PCIDevice* dev, unsigned vector);