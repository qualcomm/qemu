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
 * The global memory organization is as follow (addresses may change in the future):
 * 
 * +----------------------------------------+   <---    0x00000000000000
 * |                                        |
 * |                                        |
 * |                                        |
 * |       Virt board memory region         |
 * |                                        |
 * |                                        |
 * |                                        |
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
#include "hw/qcom/graphics.h"

static void graphics_create(QcomVirtMachineState* qvms, MemoryRegion* mem, hwaddr machine_base)
{
    DeviceState* dev = qdev_new(TYPE_QCOM_GRAPHICS);
    SysBusDevice* s = SYS_BUS_DEVICE(dev);

    sysbus_realize_and_unref(s, &error_fatal);
    memory_region_add_subregion(mem, machine_base + QCOM_GRAPHICS_BASE, sysbus_mmio_get_region(s, 0));
}

// // TODO: move this to a function of a common qcom device, taking base as paramter.
// // TODO: use dtsi files directly? for now we create a minimal fdt configuration to check if it works.
// static void graphics_update_fdt(void* fdt, QcomVirtMachineState* qvms, hwaddr machine_base)
// {
//     const char nodename[] = "/msm_gpu";
//     static const char* compat[] = {
//         "qcom,adreno-gpu-gen8-2-0",
//         "qcom,kgsl-3d0",
//     };
//     static const char* reg_names[] = {
//         "kgsl_3d0_reg_memory",
//         "rscc",
//         "cx_dbgc",
//         "cx_misc",
//     };
// 
//     hwaddr graphics_base = base + QCOM_GRAPHICS_BASE;
//     uint32_t address_cells_sz = qemu_fdt_getprop_cell(
//         fdt, "/", "#address-cells", NULL, &error_fatal
//     );
//     uint32_t size_cells_sz = qemu_fdt_getprop_cell(
//         fdt, "/", "#size-cells", NULL, &error_fatal
//     );
// 
// 
//     // === kgsl ====
//     qemu_fdt_add_subnode(fdt, nodename);
//     qemu_fdt_setprop_string_array(fdt, nodename, "compatible",
//         (char**) compat,
//         ARRAY_SIZE(compat)
//     );
//     qemu_fdt_setprop_string(fdt, nodename, "status", "ok");
//     qemu_fdt_setprop_string_array(
//         fdt, nodename, "reg-names",
//         (char**) reg_names, ARRAY_SIZE(reg_names)
//     );
//     qemu_fdt_setprop_sized_cells(fdt, nodename, "reg",
//             // kgsl_3d0_reg_memory
//             address_cells_sz, graphics_base + QCOM_GRAPHICS_KGSL_OFFSET,
//             size_cells_sz, QCOM_GRAPHICS_KGSL_SIZE,
// 
//             // rscc
//             address_cells_sz, graphics_base + QCOM_GRAPHICS_RSCC_OFFSET,
//             size_cells_sz, QCOM_GRAPHICS_RSCC_SIZE,
// 
//             // cx_dbgc
//             address_cells_sz, graphics_base + QCOM_GRAPHICS_CX_DBGC_OFFSET,
//             size_cells_sz, QCOM_GRAPHICS_CX_DBGC_SIZE,
// 
//             // cx_misc
//             address_cells_sz, graphics_base + QCOM_GRAPHICS_CX_MISC_OFFSET,
//             size_cells_sz, QCOM_GRAPHICS_CX_MISC_SIZE,
//     );
//     // === kgsl end ====
// }

static void graphics_update_fdt(void* fdt, QcomVirtMachineState* qvms, hwaddr machine_base)
{
    void* qcom_fdt = qvms->fdt;

    // extract interesting nodes from qcom dtb
    const char* gpu_node = qemu_fdt_node_path_by_label(qcom_fdt, "msm_gpu", &error_abort);
    const char* smmu_node = qemu_fdt_node_path_by_label(qcom_fdt, "kgsl_msm_iommu", &error_abort);
    const char* gmu_node = qemu_fdt_node_path_by_label(qcom_fdt, "gmu", &error_abort);

    info_report("gpu node: %s\n", gpu_node);
    info_report("smmu node: %s\n", smmu_node);
    info_report("gmu node: %s\n", gmu_node);
}

static const struct QcomVirtDevice qcom_devices[] = {
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
    for (size_t i = 0; i < ARRAY_SIZE(qcom_devices); ++i) {
        qcom_devices[i].device_create(qvms, sysmem, qvms->base_addr);
        qcom_devices[i].update_fdt(machine->fdt, qvms, qvms->base_addr);
    }
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
