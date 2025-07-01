/*
 * ARM mach-virt emulation for Qualcomm Snapdragon devices.
 *
 * Copyright (c) 2025 Qualcomm Technologies, Inc. All Rights Reserved.
 *
 * Author: Romain Malmain <rmalmain@qti.qualcomm.com>
 * 
 * More details can be found in "qcom-virt.c".
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
#include "hw/qcom/cc/cc.h"
#include "hw/qcom/smmu.h"
#include "hw/qcom/qmp.h"

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
    VIRT_QCOM_CMD_DB,
    VIRT_QCOM_RPMH_RSC_APPS,
    VIRT_QCOM_RPMH_RSC_CAM,
    VIRT_QCOM_CC_CANOE_DISPCC,
    VIRT_QCOM_CC_CANOE_GPUCC,
    VIRT_QCOM_CC_CANOE_GCC,
    VIRT_QCOM_CRM_DISP,
    VIRT_QCOM_CRM_PCIE,
    VIRT_QCOM_QMP,
    VIRT_QCOM_KGSL_SMMU,
    VIRT_QCOM_GRAPHICS,
};

enum qcom_rpmh_kind {
    RPMH_RSC_CAM,
    RPMH_RSC_APPS,
    RPMH_RSC_MAX,
};

enum qcom_crm_kind {
    CRM_DISP,
    CRM_PCIE,
    CRM_MAX,
};

enum qcom_cc_kind {
    CC_CANOE_DISPCC,
    CC_CANOE_GPUCC,
    CC_CANOE_GCC,
    CC_MAX,
};

struct QcomVirtDevice;

struct QcomVirtDevice {
    uint64_t mem_size;

    // dt label
    const char* label;

    // informative name, if label is null
    const char* name;

    size_t idx;
    int priority;

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
    QcomRpmhRscState* rpmh_rsc[RPMH_RSC_MAX];
    QcomCrmState* crm[CRM_MAX];
    QcomCCState* cc[CC_MAX];
    QcomSMMUState* kgsl_smmu;
    QcomQMPState* qmp;
};

struct QcomVirtMachineClass {
    VirtMachineClass parent;
};

#endif /* QEMU_QCOM_VIRT_H */
