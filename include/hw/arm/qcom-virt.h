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

#ifndef QEMU_QCOM_VIRT_H
#define QEMU_QCOM_VIRT_H

#include "qemu/osdep.h"
#include "exec/hwaddr.h"
#include "hw/boards.h"
#include "hw/arm/virt.h"
#include "qom/object.h"

#include "hw/qcom/crm-v2.h"
#include "hw/qcom/cmd-db.h"
#include "hw/qcom/rpmh-rsc.h"

// Top address of the real hardware
// Depends on the board being emulated
#define QCOM_VIRT_HW_BASE_ADDR  0x0000000000
#define QCOM_VIRT_HW_TOP_ADDR   0x1000000000

#define TYPE_QCOM_VIRT_MACHINE MACHINE_TYPE_NAME("qcom-virt")
OBJECT_DECLARE_TYPE(QcomVirtMachineState, QcomVirtMachineClass, QCOM_VIRT_MACHINE)

/*
 * Qualcomm-specific devices
 *
 * Note that order matters.
 */
enum QcomVirtDeviceType {
    // fallthrough device, to get some logs on unhandled accesses on the soc
    VIRT_QCOM_LOGGER,
    VIRT_QCOM_RPMH_RSC_CAM,
    VIRT_QCOM_RPMH_RSC_APPS,
    VIRT_QCOM_CMD_DB,
    VIRT_QCOM_CRM_DISP,
    VIRT_QCOM_GRAPHICS,
};

struct QcomVirtDevice;

struct QcomVirtDevice {
    uint64_t mem_size;
    const char* label;

    void (*device_create)(const struct QcomVirtDevice* qdev, void* fdt, QcomVirtMachineState* vms, MemoryRegion* mem);
    void (*update_fdt)(void* fdt, QcomVirtMachineState* vms);
};

struct QcomVirtMachineState {
    VirtMachineState parent;

    // base address of the qcom soc
    hwaddr base_addr;
    // highest address of the qcom soc
    hwaddr highest_gpa;

    // path to qualcomm's dtb
    char* dtb;

    // fdt blob
    void* fdt;
    // fdt blob size
    int fdt_sz;

    // Devices
    QcomCmdDbState* cmd_db;
    QcomRpmhRscState* rpmh_rsc_cam;
    QcomRpmhRscState* rpmh_rsc_apps;
    QcomCrmState* crm_disp;
};

struct QcomVirtMachineClass {
    VirtMachineClass parent;
};

#endif /* QEMU_QCOM_VIRT_H */
