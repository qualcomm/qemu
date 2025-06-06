/*
 * ARM mach-virt emulation for Qualcomm Snapdragon devices.
 * The only supported target at the moment is KaanapaliT r2.
 * This board is mostly an adaptation of virt.c, and tries to keep
 * the same architecture as much as possible.
 * 
 * It should have a similar behavior to a generic virt board.
 * The main differences with a normal virt board are as follow:
 *  - include custom devices to support some Qualcomm drivers
 *  - changes memory mappings to fit with Qualcomm Snapdragon mapping.
 *      Virt devices are mapped higher in memory
 *
 * Copyright (c) 2013 Linaro Limited
 * Copyright (c) 2025 Qualcomm Technologies, Inc. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2 or later, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Emulate a virtual board which works by passing Linux all the information
 * it needs about what devices are present via the device tree.
 * There are some restrictions about what we can do here:
 *  + we can only present devices whose Linux drivers will work based
 *    purely on the device tree with no platform data at all
 *  + we want to present a very stripped-down minimalist platform,
 *    both because this reduces the security attack surface from the guest
 *    and also because it reduces our exposure to being broken when
 *    the kernel updates its device tree bindings and requires further
 *    information in a device binding that we aren't providing.
 * This is essentially the same approach kvmtool uses.
 */

#include "qemu/osdep.h"
#include "qemu/datadir.h"
#include "qemu/units.h"
#include "qemu/option.h"
#include "monitor/qdev.h"
#include "hw/sysbus.h"
#include "hw/arm/boot.h"
#include "hw/arm/primecell.h"
#include "hw/arm/virt.h"
#include "hw/arm/virt-qcom.h"
#include "hw/block/flash.h"
#include "hw/vfio/vfio-calxeda-xgmac.h"
#include "hw/vfio/vfio-amd-xgbe.h"
#include "hw/display/ramfb.h"
#include "net/net.h"
#include "system/device_tree.h"
#include "system/numa.h"
#include "system/runstate.h"
#include "system/tpm.h"
#include "system/tcg.h"
#include "system/kvm.h"
#include "system/hvf.h"
#include "system/qtest.h"
#include "hw/loader.h"
#include "qapi/error.h"
#include "qemu/bitops.h"
#include "qemu/cutils.h"
#include "qemu/error-report.h"
#include "qemu/module.h"
#include "hw/pci-host/gpex.h"
#include "hw/virtio/virtio-pci.h"
#include "hw/core/sysbus-fdt.h"
#include "hw/platform-bus.h"
#include "hw/qdev-properties.h"
#include "hw/arm/fdt.h"
#include "hw/intc/arm_gic.h"
#include "hw/intc/arm_gicv3_common.h"
#include "hw/intc/arm_gicv3_its_common.h"
#include "hw/irq.h"
#include "kvm_arm.h"
#include "hvf_arm.h"
#include "hw/firmware/smbios.h"
#include "qapi/visitor.h"
#include "qapi/qapi-visit-common.h"
#include "qobject/qlist.h"
#include "standard-headers/linux/input.h"
#include "hw/arm/smmuv3.h"
#include "hw/acpi/acpi.h"
#include "target/arm/cpu-qom.h"
#include "target/arm/internals.h"
#include "target/arm/multiprocessing.h"
#include "target/arm/gtimer.h"
#include "hw/mem/pc-dimm.h"
#include "hw/mem/nvdimm.h"
#include "hw/acpi/generic_event_device.h"
#include "hw/uefi/var-service-api.h"
#include "hw/virtio/virtio-md-pci.h"
#include "hw/virtio/virtio-iommu.h"
#include "hw/char/pl011.h"
#include "qemu/guest-random.h"

static void gpu_create(struct QcomVirtDevice* qcom_device, VirtMachineState* vms, MemoryRegion* mem, hwaddr base)
{
    const char compat[] = "qcom,adreno-gpu-gen8-2-0\0qcom,kgsl-3d0";

    DeviceState* dev = qdev_new("qcom_gpu");
    SysBusDevice* s = SYS_BUS_DEVICE(dev);

    static hwaddr base_addresses[4] = {
        0x3d00000,
        0x3d50000,
        0x3d61000,
        0x3d9e000,
    };

    static hwaddr irqs[2] = {
        300,
        80
    };

    for (unsigned i = 0; i < 4; ++i) {
        memory_region_add_subregion(mem, base_addresses[i], sysbus_mmio_get_region(s, i));
    }

    for (unsigned i = 0; i < 2; ++i) {
        sysbus_connect_irq(s, 0, qdev_get_gpio_in(dev, irqs[i]));
    }
}

static void gmu_create(struct QcomVirtDevice* qcom_device, VirtMachineState* vms)
{

}

static void gmu_update_fdt(struct QcomVirtDevice* qcom_device, void* fdt, VirtMachineState* vms)
{

}
 
// TODO: Update the sizes...
static const struct QcomVirtDevice qcom_devices[] = {
    [VIRT_QCOM_GPU] = {
        .memmap = { 0x3d37000, 0 },
        .device_create = gpu_create,
    },

    [VIRT_QCOM_GMU] = {
        .memmap = { 0x3d37000, 0 },
        .device_create = gmu_create,
        .udpate_fdt = gmu_update_fdt,
    },

    [VIRT_QCOM_SMMU] = {
        .memmap = { 0x3da0000, 0 },
        .device_create = gmu_create,
        .udpate_fdt = gmu_update_fdt,
    },
};

static void qcom_create_devices(MachineState* machine)
{
    VirtMachineState* vms = VIRT_MACHINE(machine);
    MemoryRegion* sysmem = get_system_memory();

    gpu_create(qcom_devices[VIRT_QCOM_GPU], vms, sysmem);
}

static void virt_qcom_machine_class_init(ObjectClass *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);
    VirtMachineClass* vmc = VIRT_MACHINE_CLASS(oc);
    mc->desc = "QEMU ARM Virtual Machine for Qualcomm SoC";

    vmc->create_extra_devices = qcom_create_devices;
}

static const TypeInfo virt_qcom_machine_info = {
    .name = MACHINE_TYPE_NAME("virt-qcom"),
    .parent = TYPE_VIRT_MACHINE,
    .class_init = virt_qcom_machine_class_init,
};

static void virt_qcom_machine_register(void)
{
    type_register_static(&virt_qcom_machine_info);
}

type_init(virt_qcom_machine_register);