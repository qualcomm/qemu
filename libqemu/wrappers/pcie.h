#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct PCIDevice PCIDevice;

typedef struct LibQemuPCIeMsiConfig {
    bool enabled;
    uint64_t addr;
    uint32_t data;
} LibQemuPCIeMsiConfig;

typedef void (*LibQemuPCIeBoolCb)(PCIDevice* dev, void* opaque, bool val);
typedef void (*LibQemuPCIeMsiCb)(PCIDevice* dev, void* opaque, const LibQemuPCIeMsiConfig* msi_config);
typedef void (*LibQemuPCIeEventCb)(PCIDevice* dev, void* opaque);

typedef struct LibQemuPCIeCallbacks {
    LibQemuPCIeBoolCb on_bme_updated;
    LibQemuPCIeBoolCb on_vf_enabled_updated;

    LibQemuPCIeMsiCb on_msi_config_changed;

    LibQemuPCIeEventCb on_flr;
    LibQemuPCIeEventCb on_reset;
} LibQemuPCIeCallbacks;

typedef struct LibQemuPCIeCbData {
    bool bme;
    bool vf_enabled;
    LibQemuPCIeMsiConfig msi_config;
} LibQemuPCIeCbData;

bool libqemu_pcie_msi_notify(PCIDevice* dev, unsigned vector);
bool libqemu_pcie_msix_notify(PCIDevice* dev, unsigned vector);

bool libqemu_pcie_register_callbacks(PCIDevice* dev, void* opaque, const LibQemuPCIeCallbacks* cbs);
bool libqemu_pcie_clear_callbacks(PCIDevice* dev, void* opaque);
bool libqemu_pcie_callbacks_data(PCIDevice* dev, LibQemuPCIeCbData* cb_data);
