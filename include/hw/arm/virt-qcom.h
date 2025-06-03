/*
 *
 * Copyright (c) 2015 Linaro Limited
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

#ifndef QEMU_ARM_VIRT_QCOM_H
#define QEMU_ARM_VIRT_QCOM_H

#include "exec/hwaddr.h"
#include "qemu/notify.h"
#include "hw/boards.h"
#include "hw/arm/boot.h"
#include "hw/arm/bsa.h"
#include "hw/block/flash.h"
#include "system/kvm.h"
#include "hw/intc/arm_gicv3_common.h"
#include "qom/object.h"

// #define NUM_GICV2M_SPIS       64
// #define NUM_VIRTIO_TRANSPORTS 32
// #define NUM_SMMU_IRQS          4
// 
// /* See Linux kernel arch/arm64/include/asm/pvclock-abi.h */
// #define PVTIME_SIZE_PER_CPU 64
// 
// /* GPIO pins */
// #define GPIO_PIN_POWER_BUTTON  3
// 
// enum {
//     VIRT_FLASH,
//     VIRT_MEM,
//     VIRT_CPUPERIPHS,
//     VIRT_GIC_DIST,
//     VIRT_GIC_CPU,
//     VIRT_GIC_V2M,
//     VIRT_GIC_HYP,
//     VIRT_GIC_VCPU,
//     VIRT_GIC_ITS,
//     VIRT_GIC_REDIST,
//     VIRT_SMMU,
//     VIRT_UART0,
//     VIRT_MMIO,
//     VIRT_RTC,
//     VIRT_FW_CFG,
//     VIRT_PCIE,
//     VIRT_PCIE_MMIO,
//     VIRT_PCIE_PIO,
//     VIRT_PCIE_ECAM,
//     VIRT_PLATFORM_BUS,
//     VIRT_GPIO,
//     VIRT_UART1,
//     VIRT_SECURE_MEM,
//     VIRT_SECURE_GPIO,
//     VIRT_PCDIMM_ACPI,
//     VIRT_ACPI_GED,
//     VIRT_NVDIMM_ACPI,
//     VIRT_PVTIME,
//     VIRT_LOWMEMMAP_LAST,
// };
// 
// /* indices of IO regions located after the RAM */
// enum {
//     VIRT_HIGH_GIC_REDIST2 =  VIRT_LOWMEMMAP_LAST,
//     VIRT_HIGH_PCIE_ECAM,
//     VIRT_HIGH_PCIE_MMIO,
// };
// 
// typedef enum VirtIOMMUType {
//     VIRT_IOMMU_NONE,
//     VIRT_IOMMU_SMMUV3,
//     VIRT_IOMMU_VIRTIO,
// } VirtIOMMUType;
// 
// typedef enum VirtMSIControllerType {
//     VIRT_MSI_CTRL_NONE,
//     VIRT_MSI_CTRL_GICV2M,
//     VIRT_MSI_CTRL_ITS,
// } VirtMSIControllerType;
// 
// typedef enum VirtGICType {
//     VIRT_GIC_VERSION_MAX = 0,
//     VIRT_GIC_VERSION_HOST = 1,
//     /* The concrete GIC values have to match the GIC version number */
//     VIRT_GIC_VERSION_2 = 2,
//     VIRT_GIC_VERSION_3 = 3,
//     VIRT_GIC_VERSION_4 = 4,
//     VIRT_GIC_VERSION_NOSEL,
// } VirtGICType;

#endif /* QEMU_ARM_VIRT_QCOM_H */
