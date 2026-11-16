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