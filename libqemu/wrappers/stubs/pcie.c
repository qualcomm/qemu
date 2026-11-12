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