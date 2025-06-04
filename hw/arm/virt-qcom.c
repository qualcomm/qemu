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

/* Legacy RAM limit in GB (< version 4.0) */
#define LEGACY_RAMLIMIT_GB 255
#define LEGACY_RAMLIMIT_BYTES (LEGACY_RAMLIMIT_GB * GiB)

/* Addresses and sizes of our components.
 * 0..128MB is space for a flash device so we can run bootrom code such as UEFI.
 * 128MB..256MB is used for miscellaneous device I/O.
 * 256MB..1GB is reserved for possible future PCI support (ie where the
 * PCI memory window will go if we add a PCI host controller).
 * 1GB and up is RAM (which may happily spill over into the
 * high memory region beyond 4GB).
 * This represents a compromise between how much RAM can be given to
 * a 32 bit VM and leaving space for expansion and in particular for PCI.
 * Note that devices should generally be placed at multiples of 0x10000,
 * to accommodate guests using 64K pages.
 */
static const MemMapEntry qcom_base_memmap[] = {
    /* Space up to 0x8000000 is reserved for a boot ROM */
    [VIRT_FLASH] =              { 0x1000000000, 0x08000000 },
    [VIRT_CPUPERIPHS] =         { 0x1008000000, 0x00020000 },
    /* GIC distributor and CPU interfaces sit inside the CPU peripheral space */
    [VIRT_GIC_DIST] =           { 0x1008000000, 0x00010000 },
    [VIRT_GIC_CPU] =            { 0x1008010000, 0x00010000 },
    [VIRT_GIC_V2M] =            { 0x1008020000, 0x00001000 },
    [VIRT_GIC_HYP] =            { 0x1008030000, 0x00010000 },
    [VIRT_GIC_VCPU] =           { 0x1008040000, 0x00010000 },
    /* The space in between here is reserved for GICv3 CPU/vCPU/HYP */
    [VIRT_GIC_ITS] =            { 0x1008080000, 0x00020000 },
    /* This redistributor space allows up to 2*64kB*123 CPUs */
    [VIRT_GIC_REDIST] =         { 0x10080A0000, 0x00F60000 },
    [VIRT_UART0] =              { 0x1009000000, 0x00001000 },
    [VIRT_RTC] =                { 0x1009010000, 0x00001000 },
    [VIRT_FW_CFG] =             { 0x1009020000, 0x00000018 },
    [VIRT_GPIO] =               { 0x1009030000, 0x00001000 },
    [VIRT_UART1] =              { 0x1009040000, 0x00001000 },
    [VIRT_SMMU] =               { 0x1009050000, 0x00020000 },
    [VIRT_PCDIMM_ACPI] =        { 0x1009070000, MEMORY_HOTPLUG_IO_LEN },
    [VIRT_ACPI_GED] =           { 0x1009080000, ACPI_GED_EVT_SEL_LEN },
    [VIRT_NVDIMM_ACPI] =        { 0x1009090000, NVDIMM_ACPI_IO_LEN},
    [VIRT_PVTIME] =             { 0x10090a0000, 0x00010000 },
    [VIRT_SECURE_GPIO] =        { 0x10090b0000, 0x00001000 },
    [VIRT_MMIO] =               { 0x100a000000, 0x00000200 },
    /* ...repeating for a total of NUM_VIRTIO_TRANSPORTS, each of that size */
    [VIRT_PLATFORM_BUS] =       { 0x100c000000, 0x02000000 },
    [VIRT_SECURE_MEM] =         { 0x100e000000, 0x01000000 },
    [VIRT_PCIE_MMIO] =          { 0x1010000000, 0x2eff0000 },
    [VIRT_PCIE_PIO] =           { 0x103eff0000, 0x00010000 },
    [VIRT_PCIE_ECAM] =          { 0x103f000000, 0x01000000 },
    /* Actual RAM size depends on initial RAM and device memory settings */
    [VIRT_MEM] =                { 0x1100000000, LEGACY_RAMLIMIT_BYTES },
};

static void virt_qcom_machine_class_init(ObjectClass *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);
    VirtMachineClass* vmc = VIRT_MACHINE_CLASS(oc);
    mc->desc = "QEMU ARM Virtual Machine for Qualcomm SoC";

    vmc->base_memmap = qcom_base_memmap;
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