/*
 * Hexagon virt emulation
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include CONFIG_DEVICES
#include "qapi/error.h"
#include "hw/hexagon/virt.h"
#ifdef CONFIG_X_QUP_GENI_UART_RUST
#include "hw/char/qup_geni_uart.h"
#endif
#include "hw/hexagon/hexagon.h"
#include "hw/hexagon/hex-board.h"
#include "hw/hexagon/hex-subsys.h"
#include "hw/core/loader.h"
#include "hw/core/sysbus.h"
#include "hw/misc/qup-wrapper.h"
#include "qemu/error-report.h"
#include "qemu/target-info.h"
#include "qemu/units.h"
#include "machine_cfg_v68n_1024.h.inc"
#include "system/device_tree.h"
#include "system/system.h"

enum {
    VIRT_UART0,
    VIRT_QUP_UART0,
    VIRT_QUP_WRAPPER0,
    VIRT_MMIO,
    VIRT_FDT,
};

/*
 * Virtio IRQs run from VIRTIO_IRQ_BASE to
 * VIRTIO_IRQ_BASE + VIRTIO_DEV_COUNT - 1
 */
static const int VIRTIO_IRQ_BASE = 16;
static const int VIRT_UART0_IRQ = 15;
/* After the last virtio IRQ */
#ifdef CONFIG_X_QUP_GENI_UART_RUST
static const int VIRT_QUP_UART0_IRQ = 24;
#endif

static const MemMapEntry base_memmap[] = {
    [VIRT_UART0] = { 0x10000000, 0x00000200 },
    [VIRT_QUP_UART0] = { 0x10004000, 0x00006000 },
    [VIRT_QUP_WRAPPER0] = { 0x1000a000, 0x00006000 },
    [VIRT_MMIO] = { 0x11000000, 0x00001000 },
    [VIRT_FDT] = { 0x99800000, 0x00400000 },
};


static uint64_t load_kernel(const HexagonVirtMachineState *vms)
{
    return hex_board_load_elf(MACHINE(vms)->kernel_filename, NULL, NULL, NULL);
}

static uint64_t load_bios(HexagonVirtMachineState *vms)
{
    MachineState *ms = MACHINE(vms);
    uint64_t bios_addr = 0x0;  /* Load BIOS at reset vector address 0x0 */
    int bios_size;

    bios_size = load_image_targphys(ms->firmware ?: "",
                                    bios_addr, 64 * 1024, NULL);
    if (bios_size < 0) {
        error_report("Could not load BIOS '%s'", ms->firmware ?: "");
        exit(1);
    }

    return bios_addr;  /* Return entry point at address 0x0 */
}

#ifdef CONFIG_X_QUP_GENI_UART_RUST
static void fdt_add_qup_uart(MachineState *ms, int32_t l2vic_phandle)
{
    HexagonCommonMachineState *hms = HEXAGON_COMMON_MACHINE(ms);
    hwaddr wrapper_base = base_memmap[VIRT_QUP_WRAPPER0].base;
    hwaddr wrapper_size = base_memmap[VIRT_QUP_WRAPPER0].size;
    hwaddr base = base_memmap[VIRT_QUP_UART0].base;
    hwaddr size = base_memmap[VIRT_QUP_UART0].size;
    int32_t clk_phandle = qemu_fdt_alloc_phandle(ms->fdt);
    g_autofree char *wrappername =
        g_strdup_printf("/soc/geni-se-qup@%" PRIx64, wrapper_base);
    g_autofree char *nodename =
        g_strdup_printf("%s/serial@%" PRIx64, wrappername, base);
    DeviceState *wrapper_dev;

    /*
     * The QUP wrapper is the DT parent of the individual GENI serial
     * engines.  The Linux qcom_geni_se driver probes the wrapper and the
     * child drivers fetch it back via dev_get_drvdata(pdev->dev.parent);
     * without it geni_se_get_qup_hw_version() dereferences NULL.
     */
    wrapper_dev = qdev_new(TYPE_QUP_WRAPPER);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(wrapper_dev), &error_fatal);
    sysbus_mmio_map(SYS_BUS_DEVICE(wrapper_dev), 0, wrapper_base);

    qup_geni_uart_create(base,
                         qdev_get_gpio_in(hms->l2vic, VIRT_QUP_UART0_IRQ),
                         serial_hd(1));

    qemu_fdt_add_subnode(ms->fdt, "/qup-se-clk");
    qemu_fdt_setprop_string(ms->fdt, "/qup-se-clk", "compatible",
                            "fixed-clock");
    qemu_fdt_setprop_cell(ms->fdt, "/qup-se-clk", "#clock-cells", 0x0);
    qemu_fdt_setprop_cell(ms->fdt, "/qup-se-clk", "clock-frequency",
                          48000000);
    qemu_fdt_setprop_cell(ms->fdt, "/qup-se-clk", "phandle", clk_phandle);

    qemu_fdt_add_subnode(ms->fdt, wrappername);
    qemu_fdt_setprop_string(ms->fdt, wrappername, "compatible",
                            "qcom,sa8255p-geni-se-qup");
    qemu_fdt_setprop_cells(ms->fdt, wrappername, "reg", 0, wrapper_base,
                           wrapper_size);
    qemu_fdt_setprop_cell(ms->fdt, wrappername, "#address-cells", 2);
    qemu_fdt_setprop_cell(ms->fdt, wrappername, "#size-cells", 2);
    qemu_fdt_setprop(ms->fdt, wrappername, "ranges", NULL, 0);

    qemu_fdt_add_subnode(ms->fdt, nodename);
    qemu_fdt_setprop_string(ms->fdt, nodename, "compatible",
                            "qcom,geni-debug-uart");
    qemu_fdt_setprop_sized_cells(ms->fdt, nodename, "reg", 2, base, 2, size);
    qemu_fdt_setprop_cell(ms->fdt, nodename, "interrupts", VIRT_QUP_UART0_IRQ);
    qemu_fdt_setprop_cell(ms->fdt, nodename, "interrupt-parent",
                          l2vic_phandle);
    qemu_fdt_setprop_cell(ms->fdt, nodename, "clocks", clk_phandle);
    qemu_fdt_setprop_string(ms->fdt, nodename, "clock-names", "se");

    qemu_fdt_setprop_string(ms->fdt, "/aliases", "serial1", nodename);
}
#endif

static void virt_init(MachineState *ms)
{
    HexagonVirtMachineState *vms = HEXAGON_VIRT_MACHINE(ms);
    const struct hexagon_machine_config *m_cfg = &v68n_1024;
    int32_t l2vic_phandle;
    uint64_t start = 0;
    bool have_start = false;

    hex_board_create_fdt(ms, "hexagon-virt,qemu", "qcom,sm8150",
                         &vms->fdt_size);
    qemu_fdt_setprop_string(ms->fdt, "/chosen", "bootargs", ms->kernel_cmdline);

    hex_subsys_create(&vms->parent_obj, m_cfg, v68_rev);
    hex_board_create_tcm(m_cfg, &vms->tcm);
    hex_board_fdt_add_hvx(ms, m_cfg);

    l2vic_phandle = hex_board_fdt_add_l2vic(ms, m_cfg);
    hex_board_create_virtio(ms, base_memmap[VIRT_MMIO].base,
                            base_memmap[VIRT_MMIO].size, VIRTIO_IRQ_BASE,
                            VIRTIO_DEV_COUNT, l2vic_phandle);

    if (ms->kernel_filename) {
        start = load_kernel(vms);
        have_start = true;
    } else if (ms->firmware) {
        start = load_bios(vms);
        have_start = true;
    }
    hex_board_create_cpus(ms, have_start, start,
                          m_cfg->cfgtable.dma_version);

    hex_board_fdt_add_cpus(ms);
    hex_board_create_uart(ms, base_memmap[VIRT_UART0].base,
                          base_memmap[VIRT_UART0].size, VIRT_UART0_IRQ,
                          l2vic_phandle);

    hex_board_load_fdt(ms, vms->fdt_size, base_memmap[VIRT_FDT].base);
}


static void virt_class_init(ObjectClass *oc, const void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);

    mc->desc = "Hexagon Virtual Machine";
    mc->init = virt_init;
    mc->default_cpu_type = HEXAGON_CPU_TYPE_NAME("v68");
    mc->default_ram_size = 4 * GiB;
    mc->max_cpus = 8;
    mc->default_cpus = 8;
    mc->is_default = false;
    mc->default_kernel_irqchip_split = false;
    mc->block_default_type = IF_VIRTIO;
    mc->default_boot_order = NULL;
    mc->no_cdrom = 1;
    mc->numa_mem_supported = false;
    mc->default_nic = "virtio-mmio-bus";
}


static const TypeInfo virt_machine_types[] = { {
    .name = TYPE_HEXAGON_VIRT_MACHINE,
    .parent = TYPE_HEXAGON_COMMON_MACHINE,
    .instance_size = sizeof(HexagonVirtMachineState),
    .class_init = virt_class_init,
    .is_available = target_hexagon,
} };

DEFINE_TYPES(virt_machine_types)
