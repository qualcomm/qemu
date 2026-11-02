/*
 * Hexagon virt emulation
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include CONFIG_DEVICES
#include "qapi/error.h"
#include "elf.h"
#include "hw/hexagon/virt.h"
#ifdef CONFIG_X_QUP_GENI_UART_RUST
#include "hw/char/qup_geni_uart.h"
#endif
#include "hw/hexagon/hexagon.h"
#include "hw/hexagon/hex-board.h"
#include "hw/hexagon/hex-subsys.h"
#include "hw/hexagon/hexagon_hvm.h"
#include "hw/core/loader.h"
#include "qemu/datadir.h"
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
    VIRT_BOOT,
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
    [VIRT_UART0] = { 0x10000000, 0x00001000 },
    [VIRT_QUP_UART0] = { 0x10004000, 0x00006000 },
    [VIRT_QUP_WRAPPER0] = { 0x1000a000, 0x00006000 },
    [VIRT_MMIO] = { 0x11000000, 0x00001000 },
    [VIRT_FDT] = { 0x99800000, 0x00400000 },
    [VIRT_BOOT] = { 0x99c00000, 0x00000200 },
};


#define VIRT_KERNEL_ADDR 0xa0000000
/* Where the kernel is linked, and so where its entry point is */
#define VIRT_KERNEL_VADDR 0xc0000000

static uint64_t kernel_translate(void *opaque, uint64_t addr)
{
    return addr + VIRT_KERNEL_ADDR;
}

static uint64_t load_kernel(const HexagonVirtMachineState *vms)
{
    return hex_board_load_elf(MACHINE(vms)->kernel_filename, NULL, NULL, NULL);
}

static bool is_elf(const char *filename)
{
    uint8_t magic[SELFMAG];
    int fd = qemu_open_old(filename, O_RDONLY | O_BINARY);
    bool elf;

    if (fd < 0) {
        return false;
    }
    elf = read(fd, magic, sizeof(magic)) == sizeof(magic) &&
          memcmp(magic, ELFMAG, SELFMAG) == 0;
    qemu_close(fd);
    return elf;
}

/*
 * An ELF firmware starts at its entry point.  Any other firmware is a raw
 * image that starts at the reset vector, address zero, where it is loaded.
 */
static uint64_t load_bios(MachineState *ms, const char *filename)
{
    if (is_elf(filename)) {
        return hex_board_load_elf(filename, NULL, NULL, NULL);
    }
    if (load_image_targphys(filename, 0, ms->ram_size, NULL) < 0) {
        error_report("Could not load BIOS '%s'", filename);
        exit(1);
    }
    return 0;
}

/*
 * Load the kernel, and the initramfs after the room for the device tree, and
 * say where the device tree goes.  Returns the kernel's entry point.
 */
static uint64_t load_linux(MachineState *ms, hwaddr *fdt_addr)
{
    uint64_t entry, high;
    hwaddr guest_ram_end = MIN(ms->ram_size, VIRT_KERNEL_ADDR + 896 * MiB);

    if (ms->ram_size <= VIRT_KERNEL_ADDR + 32 * MiB) {
        error_report("insufficient RAM for the guest kernel");
        exit(1);
    }
    entry = hex_board_load_elf(ms->kernel_filename, kernel_translate, NULL,
                               &high);
    *fdt_addr = QEMU_ALIGN_UP(high + 16 * MiB, 4 * MiB);
    if (*fdt_addr + 4 * MiB >= guest_ram_end) {
        error_report("insufficient RAM for device tree");
        exit(1);
    }
    if (ms->initrd_filename) {
        hwaddr initrd = *fdt_addr + 4 * MiB;
        ssize_t size = load_image_targphys_as(ms->initrd_filename, initrd,
                                              guest_ram_end - initrd,
                                              &address_space_memory,
                                              &error_fatal);
        qemu_fdt_setprop_u64(ms->fdt, "/chosen", "linux,initrd-start", initrd);
        qemu_fdt_setprop_u64(ms->fdt, "/chosen", "linux,initrd-end",
                             initrd + size);
    }
    return entry;
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
                         hex_board_irq(hms, VIRT_QUP_UART0_IRQ),
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
                            "qcom,geni-uart");
    qemu_fdt_setprop_sized_cells(ms->fdt, nodename, "reg", 2, base, 2, size);
    qemu_fdt_setprop_cells(ms->fdt, nodename, "interrupts",
                           HEX_BOARD_L2VIC_SPI_BASE + VIRT_QUP_UART0_IRQ, 0);
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
    HexagonCommonMachineState *hms = HEXAGON_COMMON_MACHINE(ms);
    /* Linux numbers the QTimer frames from L2VIC input 2 */
    static struct hexagon_machine_config virt_cfg;
    const struct hexagon_machine_config *m_cfg = &virt_cfg;
    int32_t l2vic_phandle;
    uint64_t start = 0;
    bool have_start = false;
    hwaddr fdt_addr = base_memmap[VIRT_FDT].base;
    g_autofree char *firmware = NULL;
    bool vm_guest;
    /* A Linux guest: it has a hypervisor firmware, or QEMU is its monitor. */
    bool linux_guest;

    virt_cfg = v68n_1024;
    virt_cfg.qtimer_irq_base = 2;

    if (hms->hvm_enabled && ms->firmware) {
        error_report("hvm=on runs the guest without a hypervisor firmware "
                     "(-bios)");
        exit(1);
    }
    if (ms->firmware) {
        firmware = qemu_find_file(QEMU_FILE_TYPE_BIOS, ms->firmware);
        if (!firmware) {
            error_report("Could not find BIOS '%s'", ms->firmware);
            exit(1);
        }
    }
    hex_board_check_initrd(ms, firmware);
    vm_guest = ms->kernel_filename && firmware;
    linux_guest = vm_guest || (ms->kernel_filename && hms->hvm_enabled);

    hex_board_create_fdt(ms, "hexagon-virt,qemu", "qcom,sm8150",
                         &vms->fdt_size);
    qemu_fdt_setprop_string(ms->fdt, "/soc", "compatible", "simple-bus");
    qemu_fdt_setprop_string(ms->fdt, "/chosen", "bootargs", ms->kernel_cmdline);

    hex_subsys_create(&vms->parent_obj, m_cfg, v68_rev);
    hex_board_create_tcm(m_cfg, &vms->tcm);
    hex_board_fdt_add_hvx(ms, m_cfg);

    l2vic_phandle = hex_board_fdt_add_l2vic(ms, m_cfg);
    hex_board_create_virtio(ms, base_memmap[VIRT_MMIO].base,
                            base_memmap[VIRT_MMIO].size, VIRTIO_IRQ_BASE,
                            VIRTIO_DEV_COUNT, l2vic_phandle);

    if (hms->hvm_enabled && ms->kernel_filename) {
        HexagonHVMState *hvm = HEXAGON_HVM(hms->hvm);

        start = load_linux(ms, &fdt_addr) - VIRT_KERNEL_VADDR +
                VIRT_KERNEL_ADDR;
        have_start = true;
        /* The kernel finds the device tree in r1:r0, 64 bits wide. */
        hvm->info[HVM_INFO_BOOT_R0] = fdt_addr;
        hvm->info[HVM_INFO_BOOT_R1] = fdt_addr >> 32;
    } else if (hms->hvm_enabled) {
        /* A bare guest, which the user loads where it wants to run. */
    } else if (vm_guest) {
        start = hex_board_load_vm_guest(ms, firmware, 0, 0,
                                        base_memmap[VIRT_BOOT].base,
                                        vms->fdt_size, &fdt_addr);
        have_start = true;
    } else if (ms->kernel_filename) {
        start = load_kernel(vms);
        have_start = true;
    } else if (firmware) {
        start = load_bios(ms, firmware);
        have_start = true;
    }
    hex_board_create_cpus(ms, have_start, start,
                          m_cfg->cfgtable.dma_version);

    hex_board_fdt_add_cpus(ms);
    if (linux_guest) {
        hex_board_fdt_add_vm_timer(ms, 0xab000000, 0x1000, 12);
    }
    hex_board_create_uart(ms, base_memmap[VIRT_UART0].base,
                          base_memmap[VIRT_UART0].size, VIRT_UART0_IRQ,
                          l2vic_phandle);
#ifdef CONFIG_X_QUP_GENI_UART_RUST
    fdt_add_qup_uart(ms, l2vic_phandle);
#endif

    hex_board_load_fdt(ms, vms->fdt_size, fdt_addr);
}


static bool virt_get_hvm(Object *obj, Error **errp)
{
    return HEXAGON_COMMON_MACHINE(obj)->hvm_enabled;
}

static void virt_set_hvm(Object *obj, bool value, Error **errp)
{
    HEXAGON_COMMON_MACHINE(obj)->hvm_enabled = value;
}

static void virt_class_init(ObjectClass *oc, const void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);

    object_class_property_add_bool(oc, "hvm", virt_get_hvm, virt_set_hvm);
    object_class_property_set_description(oc, "hvm",
        "Run a Hexagon Virtual Machine guest with QEMU as the monitor "
        "instead of a hypervisor firmware (off by default)");
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
