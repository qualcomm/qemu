#include "qemu/osdep.h"
#include "libqemu/wrappers/pcie.h"

#include "hw/pci/msi.h"
#include "hw/pci/msix.h"

#include "hw/pci/pcie_libqemu.h"

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