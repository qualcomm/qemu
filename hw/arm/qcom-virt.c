/*
 * ARM mach-virt emulation for Qualcomm Snapdragon devices.
 * The only supported target at the moment is KaanapaliT r2.
 * This board uses the generic ARM virt board as a base, and
 * adds Qualcomm's devices devices at a higher base address.
 * 
 * It should have a very similar behavior to a generic virt board.
 * It only adds Qualcomm devices "on top" of virt devices.
 * MMIO regions are mapped according 
 *
 * Copyright (c) 2025 Qualcomm Technologies, Inc. All Rights Reserved.
 *
 * Author: Romain Malmain <rmalmain@qti.qualcomm.com>
 * 
 * The global memory organization is as follow (addresses may be different in your case):
 * 
 * +----------------------------------------+   <---    0x00000000000000
 * |                                        |
 * |                                        |
 * |                                        |
 * |       Virt board memory region         |
 * |                                        |
 * |                                        |
 * +----------------------------------------+   <---    0x00011000000000
 * |         Unused padding space           |
 * +----------------------------------------+   <---    0x00010000000000 (must be aligned on qcom soc size)
 * |                                        |
 * |      Qualcomm SoC memory region        |
 * |                                        |
 * +----------------------------------------+   <---    0x00011000000000
 * |                                        |
 * |                                        |
 * |                                        |
 * |                                        |
 * |                                        |
 * |                                        |
 * |                                        |
 * |         Unused remaining space         |
 * |                                        |
 * |                                        |
 * |                                        |
 * |                                        |
 * |                                        |
 * |                                        |
 * |                                        |
 * |                                        |
 * +----------------------------------------+   <---    0x10000000000000 (max physical address addressable for ARM)
 * 
 */

#include "qemu/osdep.h"
#include "qemu/datadir.h"
#include "qemu/option.h"
#include "hw/sysbus.h"
#include "hw/arm/virt.h"
#include "system/device_tree.h"
#include "qapi/error.h"
#include "qemu/error-report.h"
#include "qemu/module.h"
#include "hw/core/sysbus-fdt.h"
#include "hw/platform-bus.h"
#include "hw/qdev-properties.h"
#include "hw/irq.h"
#include "qemu/guest-random.h"
#include "exec/address-spaces.h"
#include "target/arm/internals.h"

#include "hw/arm/qcom-virt.h"

// Qualcomm peripherals
#include "hw/qcom/logger.h"
#include "hw/qcom/graphics.h"
#include "hw/qcom/crm-v2.h"

static void logger_create(const struct QcomVirtDevice* qdev, void* fdt, QcomVirtMachineState* qvms, MemoryRegion* mem)
{
    hwaddr machine_base = qvms->base_addr;

    DeviceState* dev = qdev_new(TYPE_QCOM_LOGGER);
    SysBusDevice* s = SYS_BUS_DEVICE(dev);

    sysbus_realize_and_unref(s, &error_fatal);
    memory_region_add_subregion_overlap(mem, machine_base, sysbus_mmio_get_region(s, 0), -1);
}

static void logger_update_fdt(void* fdt, QcomVirtMachineState* qvms)
{}

static void add_cmd_db(const struct QcomVirtDevice* qdev, void* fdt, QcomVirtMachineState* qvms, MemoryRegion* mem)
{
    hwaddr machine_base = qvms->base_addr;
    void* qcom_fdt = qvms->fdt;

    qvms->cmd_db = cmd_db_create_by_label(fdt, qcom_fdt, qdev->label, qdev->mem_size);
    OfSysBusDevice* ofdev = OF_SYS_BUS_DEVICE(qvms->cmd_db);
    SysBusDevice* s = SYS_BUS_DEVICE(ofdev);

    // a base address should have been found.
    assert(ofdev->base_addr);

    memory_region_add_subregion(mem, machine_base + *ofdev->base_addr, sysbus_mmio_get_region(s, 0));
}

static void crm_disp_create(const struct QcomVirtDevice* qdev, void* fdt, QcomVirtMachineState* qvms, MemoryRegion* mem)
{
    hwaddr machine_base = qvms->base_addr;
    void* qcom_fdt = qvms->fdt;

    qvms->crm_disp = crm_v2_create_by_label(fdt, qcom_fdt, qdev->label, qdev->mem_size);
    OfSysBusDevice* ofdev = OF_SYS_BUS_DEVICE(qvms->crm_disp);
    VirtMachineState* vms = VIRT_MACHINE(qvms);
    SysBusDevice* s = SYS_BUS_DEVICE(ofdev);

    // a base address should have been found.
    assert(ofdev->base_addr);

    // interrupts should be set for this device
    assert(ofdev->interrupts);
    assert(ofdev->interrupts->interrupt_controller_phandle == vms->gic_phandle);

    for (size_t i = 0; i < ofdev->interrupts->nb_interrupts; ++i) {
        int interrupt = ofdev->interrupts->interrupts[3 * i + 1];
        sysbus_connect_irq(s, i, qdev_get_gpio_in(vms->gic, interrupt));
    }

    memory_region_add_subregion(mem, machine_base + *ofdev->base_addr, sysbus_mmio_get_region(s, 0));
}

static void graphics_create(const struct QcomVirtDevice* qdev, void* fdt, QcomVirtMachineState* qvms, MemoryRegion* mem)
{
    hwaddr machine_base = qvms->base_addr;

    DeviceState* dev = qdev_new(TYPE_QCOM_GRAPHICS);
    SysBusDevice* s = SYS_BUS_DEVICE(dev);

    sysbus_realize_and_unref(s, &error_fatal);
    memory_region_add_subregion(mem, machine_base + QCOM_GRAPHICS_BASE, sysbus_mmio_get_region(s, 0));
}

static void graphics_update_fdt(void* fdt, QcomVirtMachineState* qvms)
{
    void* qcom_fdt = qvms->fdt;

    // graphics dependencies
    const char* apps_rsc_node = qemu_fdt_node_path_by_label(qcom_fdt, "apps_rsc", &error_abort);
    assert(qemu_fdt_copy_node(fdt, qcom_fdt, apps_rsc_node, &error_abort));

    // const char* disp_crm_node = qemu_fdt_node_path_by_label(qcom_fdt, "disp_crm", &error_abort);
    // qemu_fdt_copy_node(fdt, qcom_fdt, disp_crm_node, &error_abort);

    // const char* cam_crm_node = qemu_fdt_node_path_by_label(qcom_fdt, "cam_crm", &error_abort);
    // qemu_fdt_copy_node(fdt, qcom_fdt, cam_crm_node, &error_abort);

    // extract interesting nodes from qcom dtb.
    const char* gpu_node = qemu_fdt_node_path_by_label(qcom_fdt, "msm_gpu", &error_abort);
    const char* smmu_node = qemu_fdt_node_path_by_label(qcom_fdt, "kgsl_msm_iommu", &error_abort);
    const char* gmu_node = qemu_fdt_node_path_by_label(qcom_fdt, "gmu", &error_abort);

    // copy nodes from qemu dtb to qcom virt dtb.
    qemu_fdt_copy_node(fdt, qcom_fdt, gpu_node, &error_abort);
    qemu_fdt_copy_node(fdt, qcom_fdt, smmu_node, &error_abort);
    qemu_fdt_copy_node(fdt, qcom_fdt, gmu_node, &error_abort);
}

static const struct QcomVirtDevice qcom_devices[] = {
    [VIRT_QCOM_LOGGER] = {
        .device_create = logger_create,
        .update_fdt = logger_update_fdt,
    },
    [VIRT_QCOM_CMD_DB] = {
        .label = "aop_cmd_db_mem",
        .mem_size = 0x20000,
        .device_create = add_cmd_db,
    },
    [VIRT_QCOM_CRM_DISP] = {
        .label = "disp_crm",
        .mem_size = 0xd000,
        .device_create = crm_disp_create,
    },
    [VIRT_QCOM_GRAPHICS] = {
        .device_create = graphics_create,
        .update_fdt = graphics_update_fdt,
    },
};

static void qcom_create_devices(MachineState* machine)
{
    VirtMachineState* vms = VIRT_MACHINE(machine);
    QcomVirtMachineState* qvms = QCOM_VIRT_MACHINE(vms);
    MemoryRegion* sysmem = get_system_memory();

    // first, load target's DTB blob
    if (!qvms->dtb) {
        error_report("No dtb provided. Qualcomm devices cannot be initialized correctly.");
        exit(1);
    }

    char* filename = qemu_find_file(QEMU_FILE_TYPE_BIOS, qvms->dtb);

    if (!filename) {
        error_report("Could not find dtb file: %s", qvms->dtb);
        exit(1);
    }

    qvms->fdt = load_device_tree(qvms->dtb, &qvms->fdt_sz);
    if (!qvms->fdt) {
        error_report("Failed to load dtb file: %s", qvms->dtb);
        exit(1);
    }

    // find a suitable base address for qualcomm devices
    hwaddr min_addr = vms->highest_gpa;
    assert(min_addr != 0);

    qvms->base_addr = QEMU_ALIGN_UP(min_addr, QCOM_VIRT_HW_TOP_ADDR);
    qvms->highest_gpa = qvms->base_addr - 1 + QCOM_VIRT_HW_TOP_ADDR;

    // check our custom address range is valid
    int requested_pa_size = 64 - clz64(qvms->highest_gpa);
    int pamax = arm_pamax(ARM_CPU(first_cpu));

    if (pamax < requested_pa_size) {
        error_report("VCPU supports less PA bits (%d) than "
                        "required by Qualcomm address space (%d)",
                        pamax, requested_pa_size);
        exit(1);
    }

    // initialize qualcomm devices
    // TODO: change by incorporating this in the new qemu object.
    for (size_t i = 0; i < ARRAY_SIZE(qcom_devices); ++i) {
        qcom_devices[i].device_create(&qcom_devices[i], machine->fdt, qvms, sysmem);
        if (qcom_devices[i].update_fdt) {
            qcom_devices[i].update_fdt(machine->fdt, qvms);
        }
    }

    // replace the range with an empty range, so that the kernel is happy.
    // otherwise, the kernel thinks the soc is mapped at address 0 and does not
    // map the devices at all...
    // TODO: check ranges correctly in the DT!
    qemu_fdt_delprop(machine->fdt, "/soc", "ranges", &error_abort);
    qemu_fdt_setprop_bool(machine->fdt, "/soc", "ranges");

    // edit base addresses with the new one
    qemu_fdt_set_nodes_addr(machine->fdt, "/soc", qvms->base_addr, &error_abort);
    // TODO: find a better way to do that...
    qemu_fdt_set_nodes_addr(machine->fdt, "/reserved-memory/aop_cmd_db_region@81c60000", qvms->base_addr, &error_abort);

    // check global fdt consistency
    qemu_fdt_check_memory_consistency(machine->fdt, "/soc", get_system_io(), &error_abort);

    save_device_tree(machine->fdt, "/tmp/qemu.dtb", &error_abort);
}

static char *qcom_machine_get_dtb(Object *obj, Error **errp)
{
    MachineState *ms = MACHINE(obj);
    QcomVirtMachineState* qvms = QCOM_VIRT_MACHINE(ms);

    return g_strdup(qvms->dtb);
}

static void qcom_machine_set_dtb(Object *obj, const char *value, Error **errp)
{
    QcomVirtMachineState* qvms = QCOM_VIRT_MACHINE(obj);

    g_free(qvms->dtb);
    qvms->dtb = g_strdup(value);
}

static void qcom_virt_machine_instance_init(Object *obj)
{
}

static void qcom_virt_machine_class_init(ObjectClass *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);
    VirtMachineClass* vmc = VIRT_MACHINE_CLASS(oc);
    mc->desc = "QEMU ARM Virtual Machine for Qualcomm SoC";

    vmc->create_extra_devices = qcom_create_devices;

    object_class_property_add_str(oc, "qcom-dtb",
        qcom_machine_get_dtb, qcom_machine_set_dtb);
    object_class_property_set_description(oc, "qcom-dtb",
        "Qualcomm SoC device tree file");
}

static const TypeInfo virt_qcom_machine_info = {
    .name = TYPE_QCOM_VIRT_MACHINE,
    .parent = TYPE_VIRT_MACHINE,
    .instance_init = qcom_virt_machine_instance_init,
    .instance_size = sizeof(QcomVirtMachineState),
    .class_init = qcom_virt_machine_class_init,
    .class_size = sizeof(QcomVirtMachineClass),
};

static void virt_qcom_machine_register(void)
{
    type_register_static(&virt_qcom_machine_info);
}

type_init(virt_qcom_machine_register);
