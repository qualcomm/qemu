#include "libqemu/wrappers/pcie.h"

#include "hw/pci/msi.h"
#include "hw/pci/msix.h"

bool libqemu_pcie_msi_notify(PCIDevice* dev, unsigned vector)
{
    if (!msi_present(dev) || vector >= msi_nr_vectors_allocated(dev)) {
        return false;
    }

    if (msi_enabled(dev)) {
        msi_notify(dev, vector);
    }

    return true;
}

bool libqemu_pcie_msix_notify(PCIDevice* dev, unsigned vector)
{
    if (!msix_present(dev) || vector >= msix_nr_vectors_allocated(dev)) {
        return false;
    }

    if (msix_enabled(dev)) {
        msix_notify(dev, vector);
    }

    return true;
}