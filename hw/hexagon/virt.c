/*
 * Hexagon virt emulation
 *
 * Copyright (c) 2024-2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "hw/hexagon/virt.h"
#include "elf.h"
#include "hw/char/pl011.h"
#include "hw/core/clock.h"
#include "hw/core/sysbus-fdt.h"
#include "hw/hexagon/hexagon.h"
#include "hw/hexagon/hexagon_globalreg.h"
#include "hw/hexagon/hexagon_tlb.h"
#include "hw/core/loader.h"
#include "hw/core/qdev-clock.h"
#include "hw/core/qdev-properties.h"
#include "hw/core/register.h"
#include "hw/core/sysbus.h"
#include "hw/timer/qct-qtimer.h"
#include "hw/misc/cdsp-pll.h"
#include "qapi/error.h"
#include "qemu/error-report.h"
#include "qemu/guest-random.h"
#include "qemu/units.h"
#include "qapi/error.h"
#include "elf.h"
#include "machine_cfg_v68n_1024.h.inc"
#include "system/address-spaces.h"
#include "system/device_tree.h"
#include "system/reset.h"
#include "system/system.h"
#include <libfdt.h>
#include "target/hexagon/macros.h"

static const int VIRTIO_DEV_COUNT = 8;

static const MemMapEntry base_memmap[] = {
    [VIRT_UART0] = { 0x10000000, 0x00000200 },
    [VIRT_MMIO] = { 0x11000000, 0x1000000, },
    [VIRT_GPT] = { 0xab000000, 0x00001000 },
    [VIRT_FDT] = { 0xbf800000, 0x00400000 },
    [VIRT_BOOT] = { 0x99c00000, 0x00000200 },
    [VIRT_PLL] = { 0x26300000, 0x00001000 },
};

static const int irqmap[] = {
    [VIRT_MMIO] = 18, /* ...to 18 + VIRTIO_DEV_COUNT - 1 */
    [VIRT_GPT] = 12,
    [VIRT_UART0] = 15,
    [VIRT_QTMR0] = 2,
    [VIRT_QTMR1] = 4,
};


static void create_fdt(HexagonVirtMachineState *vms)
{
    MachineState *ms = MACHINE(vms);
    void *fdt = create_device_tree(&vms->fdt_size);

    if (!fdt) {
        error_report("create_device_tree() failed");
        exit(1);
    }

    ms->fdt = fdt;

    qemu_fdt_setprop_cell(fdt, "/", "#address-cells", 0x2);
    qemu_fdt_setprop_cell(fdt, "/", "#size-cells", 0x1);
    qemu_fdt_setprop_string(fdt, "/", "model", "hexagon-virt,qemu");
    qemu_fdt_setprop_string(fdt, "/", "compatible", "qcom,sm8150");

    qemu_fdt_add_subnode(fdt, "/soc");
    qemu_fdt_setprop_cell(fdt, "/soc", "#address-cells", 0x2);
    qemu_fdt_setprop_cell(fdt, "/soc", "#size-cells", 0x1);
    qemu_fdt_setprop(fdt, "/soc", "ranges", NULL, 0);

    qemu_fdt_add_subnode(fdt, "/chosen");

    uint8_t rng_seed[32];
    qemu_guest_getrandom_nofail(rng_seed, sizeof(rng_seed));
    qemu_fdt_setprop(fdt, "/chosen", "rng-seed", rng_seed, sizeof(rng_seed));
}

static void fdt_add_hvx(HexagonVirtMachineState *vms,
                        const hexagon_machine_config *m_cfg, Error **errp)
{
    const MachineState *ms = MACHINE(vms);
    uint32_t vtcm_size_bytes = m_cfg->cfgtable.vtcm_size_kb * 1024;
    if (vtcm_size_bytes > 0) {
        memory_region_init_ram(&vms->vtcm, NULL, "vtcm.ram", vtcm_size_bytes,
                               errp);
        memory_region_add_subregion(vms->sys, m_cfg->cfgtable.vtcm_base << 16,
                                    &vms->vtcm);

        qemu_fdt_add_subnode(ms->fdt, "/soc/vtcm");
        qemu_fdt_setprop_string(ms->fdt, "/soc/vtcm", "compatible",
                                "qcom,hexagon_vtcm");

        assert(sizeof(m_cfg->cfgtable.vtcm_base) == sizeof(uint32_t));
        qemu_fdt_setprop_cells(ms->fdt, "/soc/vtcm", "reg", 0,
                               m_cfg->cfgtable.vtcm_base << 16,
                               vtcm_size_bytes);
    }

    if (m_cfg->cfgtable.ext_contexts > 0) {
        qemu_fdt_add_subnode(ms->fdt, "/soc/hvx");
        qemu_fdt_setprop_string(ms->fdt, "/soc/hvx", "compatible",
                                "qcom,hexagon-hvx");
        qemu_fdt_setprop_cells(ms->fdt, "/soc/hvx", "qcom,hvx-max-ctxts",
                               m_cfg->cfgtable.ext_contexts);
        qemu_fdt_setprop_cells(ms->fdt, "/soc/hvx", "qcom,hvx-vlength",
                               m_cfg->cfgtable.hvx_vec_log_length);
    }
}

static int32_t irq_hvm_ic_phandle = -1;
static void fdt_add_hvm_pic_node(HexagonVirtMachineState *vms,
                                 const hexagon_machine_config *m_cfg)
{
    MachineState *ms = MACHINE(vms);
    irq_hvm_ic_phandle = qemu_fdt_alloc_phandle(ms->fdt);

    qemu_fdt_setprop_cell(ms->fdt, "/soc", "interrupt-parent",
                          irq_hvm_ic_phandle);

    qemu_fdt_add_subnode(ms->fdt, "/soc/interrupt-controller");
    qemu_fdt_setprop_cell(ms->fdt, "/soc/interrupt-controller",
                          "#address-cells", 2);
    qemu_fdt_setprop_cell(ms->fdt, "/soc/interrupt-controller",
                          "#interrupt-cells", 2);
    {
        /* string list, not one string with embedded commas */
        static const char pic_compat[] = "qcom,h2-pic\0hvm-pic";
        qemu_fdt_setprop(ms->fdt, "/soc/interrupt-controller", "compatible",
                         pic_compat, sizeof(pic_compat));
    }
    qemu_fdt_setprop(ms->fdt, "/soc/interrupt-controller",
                     "interrupt-controller", NULL, 0);
    qemu_fdt_setprop_cell(ms->fdt, "/soc/interrupt-controller", "phandle",
                          irq_hvm_ic_phandle);

    sysbus_mmio_map(SYS_BUS_DEVICE(vms->l2vic), 0,
                    m_cfg->l2vic_base);
    sysbus_mmio_map(SYS_BUS_DEVICE(vms->l2vic), 1,
                    m_cfg->cfgtable.fastl2vic_base << 16);
}


static void fdt_add_gpt_node(HexagonVirtMachineState *vms)
{
    g_autofree char *name = NULL;
    MachineState *ms = MACHINE(vms);

    name = g_strdup_printf("/soc/gpt@%" PRIx64,
                           (int64_t)base_memmap[VIRT_GPT].base);
    qemu_fdt_add_subnode(ms->fdt, name);
    {
        /* string list, not one string with embedded commas */
        static const char gpt_compat[] = "qcom,h2-timer\0hvm-timer";
        qemu_fdt_setprop(ms->fdt, name, "compatible",
                         gpt_compat, sizeof(gpt_compat));
    }
    qemu_fdt_setprop_cells(ms->fdt, name, "interrupts", irqmap[VIRT_GPT], 0);
    qemu_fdt_setprop_cells(ms->fdt, name, "reg", 0x0,
                           base_memmap[VIRT_GPT].base,
                           base_memmap[VIRT_GPT].size);
}

static int32_t clock_phandle = -1;
static void fdt_add_clocks(const HexagonVirtMachineState *vms)
{
    MachineState *ms = MACHINE(vms);
    clock_phandle = qemu_fdt_alloc_phandle(ms->fdt);
    qemu_fdt_add_subnode(ms->fdt, "/apb-pclk");
    qemu_fdt_setprop_string(ms->fdt, "/apb-pclk", "compatible", "fixed-clock");
    qemu_fdt_setprop_cell(ms->fdt, "/apb-pclk", "#clock-cells", 0x0);
    qemu_fdt_setprop_cell(ms->fdt, "/apb-pclk", "clock-frequency", 24000000);
    qemu_fdt_setprop_string(ms->fdt, "/apb-pclk", "clock-output-names",
                            "clk24mhz");
    qemu_fdt_setprop_cell(ms->fdt, "/apb-pclk", "phandle", clock_phandle);
}

static void fdt_add_uart(const HexagonVirtMachineState *vms, int uart)
{
    char *nodename;
    hwaddr base = base_memmap[uart].base;
    hwaddr size = base_memmap[uart].size;
    assert(uart == 0);
    int irq = irqmap[VIRT_UART0 + uart];
    const char compat[] = "arm,pl011\0arm,primecell";
    const char clocknames[] = "uartclk\0apb_pclk";
    MachineState *ms = MACHINE(vms);

    DeviceState *pl011_dev = qdev_new("pl011");
    SysBusDevice *s = SYS_BUS_DEVICE(pl011_dev);
    qdev_prop_set_chr(pl011_dev, "chardev", serial_hd(0));

    qdev_connect_clock_in(pl011_dev, "clk", vms->apb_pclk);
    sysbus_realize_and_unref(s, &error_fatal);
    sysbus_mmio_map(s, 0, base);
    sysbus_connect_irq(s, 0, qdev_get_gpio_in(vms->l2vic, irq));

    nodename = g_strdup_printf("/pl011@%" PRIx64, base);
    qemu_fdt_add_subnode(ms->fdt, nodename);

    /* Note that we can't use setprop_string because of the embedded NUL */
    qemu_fdt_setprop(ms->fdt, nodename, "compatible", compat, sizeof(compat));
    qemu_fdt_setprop_cells(ms->fdt, nodename, "reg", 0, base, size);
    qemu_fdt_setprop_cells(ms->fdt, nodename, "interrupts", 32 + irq, 0);
    qemu_fdt_setprop_cells(ms->fdt, nodename, "clocks", clock_phandle,
                           clock_phandle);
    qemu_fdt_setprop(ms->fdt, nodename, "clock-names", clocknames,
                     sizeof(clocknames));
    qemu_fdt_setprop_cell(ms->fdt, nodename, "interrupt-parent",
                          irq_hvm_ic_phandle);

    qemu_fdt_setprop_string(ms->fdt, "/chosen", "stdout-path", nodename);
    qemu_fdt_add_subnode(ms->fdt, "/aliases");
    qemu_fdt_setprop_string(ms->fdt, "/aliases", "serial0", nodename);

    g_free(nodename);
}

static void fdt_add_cpu_nodes(const HexagonVirtMachineState *vms)
{
    MachineState *ms = MACHINE(vms);
    qemu_fdt_add_subnode(ms->fdt, "/cpus");
    qemu_fdt_setprop_cell(ms->fdt, "/cpus", "#address-cells", 0x1);
    qemu_fdt_setprop_cell(ms->fdt, "/cpus", "#size-cells", 0x0);

    /* cpu nodes */
    for (int num = ms->smp.cpus - 1; num >= 0; num--) {
        char *nodename = g_strdup_printf("/cpus/cpu@%d", num);
        qemu_fdt_add_subnode(ms->fdt, nodename);
        qemu_fdt_setprop_string(ms->fdt, nodename, "device_type", "cpu");
        qemu_fdt_setprop_cell(ms->fdt, nodename, "reg", num);
        qemu_fdt_setprop_cell(ms->fdt, nodename, "phandle",
                              qemu_fdt_alloc_phandle(ms->fdt));
        g_free(nodename);
    }
}


static void fdt_add_virtio_devices(const HexagonVirtMachineState *vms)
{
    MachineState *ms = MACHINE(vms);
    /* VirtIO MMIO devices */
    for (int i = 0; i < VIRTIO_DEV_COUNT; i++) {
        char *nodename;
        int irq = irqmap[VIRT_MMIO] + i;
        size_t size = base_memmap[VIRT_MMIO].size;
        hwaddr base = base_memmap[VIRT_MMIO].base + i * size;

        nodename = g_strdup_printf("/virtio_mmio@%" PRIx64, base);
        qemu_fdt_add_subnode(ms->fdt, nodename);
        qemu_fdt_setprop_string(ms->fdt, nodename, "compatible", "virtio,mmio");
        qemu_fdt_setprop_sized_cells(ms->fdt, nodename, "reg", 2, base, 1,
                                     size);
        qemu_fdt_setprop_cells(ms->fdt, nodename, "interrupts", irq, 0);
        qemu_fdt_setprop_cell(ms->fdt, nodename, "interrupt-parent",
                              irq_hvm_ic_phandle);

        sysbus_create_simple(
            "virtio-mmio", base,
            qdev_get_gpio_in(vms->l2vic, irqmap[VIRT_MMIO] + i));

        g_free(nodename);
    }
}

static void create_qtimer(HexagonVirtMachineState *vms,
        const hexagon_machine_config *m_cfg)
{
    Error **errp = NULL;
    vms->qtimer = QCT_QTIMER(qdev_new(TYPE_QCT_QTIMER));
    object_property_set_uint(OBJECT(vms->qtimer), "nr_frames", 2, errp);
    object_property_set_uint(OBJECT(vms->qtimer), "nr_views", 1, errp);
    object_property_set_uint(OBJECT(vms->qtimer), "cnttid_0", 0x111, errp);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(vms->qtimer), errp);

    sysbus_mmio_map(SYS_BUS_DEVICE(vms->qtimer), 1, m_cfg->qtmr_region);
}

static void create_pll(HexagonVirtMachineState *vms)
{
    Error **errp = NULL;
    CdspPLLState *pll = CDSP_PLL(qdev_new(TYPE_CDSP_PLL));

    object_property_set_uint(OBJECT(pll), "base-freq", 19200000, errp);
    object_property_set_uint(OBJECT(pll), "default-l-val", 62, errp);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(pll), errp);

    sysbus_mmio_map(SYS_BUS_DEVICE(pll), 0, base_memmap[VIRT_PLL].base);
}

static void fdt_add_pll_node(HexagonVirtMachineState *vms)
{
    g_autofree char *name = NULL;
    MachineState *ms = MACHINE(vms);

    name = g_strdup_printf("/soc/pll@%" PRIx64,
                           (int64_t)base_memmap[VIRT_PLL].base);
    qemu_fdt_add_subnode(ms->fdt, name);
    qemu_fdt_setprop_string(ms->fdt, name, "compatible",
                            "qcom,cdsp-fabia-pll");
    qemu_fdt_setprop_cells(ms->fdt, name, "reg", 0x0,
                           base_memmap[VIRT_PLL].base,
                           base_memmap[VIRT_PLL].size);
    qemu_fdt_setprop_cell(ms->fdt, name, "base-freq", 19200000);
    qemu_fdt_setprop_cell(ms->fdt, name, "default-l-val", 62);
}

static void virt_instance_init(Object *obj)
{
    HexagonVirtMachineState *vms = HEXAGON_VIRT_MACHINE(obj);

    vms->apb_pclk = clock_new(obj, "apb-pclk");
    clock_set_hz(vms->apb_pclk, 24000000);

    /* Initialize boot info */
    memset(&vms->bootinfo, 0, sizeof(vms->bootinfo));

    create_fdt(vms);
}

static void virt_instance_finalize(Object *obj)
{
    HexagonVirtMachineState *vms = HEXAGON_VIRT_MACHINE(obj);
    MachineState *ms = MACHINE(vms);

    g_free(ms->fdt);
}


void hexagon_load_fdt(const HexagonVirtMachineState *vms)
{
    MachineState *ms = MACHINE(vms);
    hwaddr fdt_addr = base_memmap[VIRT_FDT].base;
    uint32_t fdtsize = vms->fdt_size;

    g_assert(fdtsize <= base_memmap[VIRT_FDT].size);
    /* copy in the device tree */
    rom_add_blob_fixed_as("fdt", ms->fdt, fdtsize, fdt_addr,
                          &address_space_memory);
    qemu_register_reset_nosnapshotload(
        qemu_fdt_randomize_seeds,
        rom_ptr_for_as(&address_space_memory, fdt_addr, fdtsize));
}

static uint32_t bootloader[] = {
    /* Load fdt_base_low value into r0: */
    0x099c4000, /* { immext(#0x99c00000) */
    0x7800c606, /*   r6 = ##-0x662fffd0 } */
    0x9186c000, /* { r0 = memw(r6+#0x0) } */

    /* Load fdt_base_high value into r1: */
    0x099c4000, /* { immext(#0x99c00000) */
    0x7800c586, /*   r6 = ##-0x662fffd4 } */
    0x9186c001, /* { r1 = memw(r6+#0x0) } */

    /* Load next_stage_entry value into r7: */
    0x099c4000, /* { immext(#0x99c00000) */
    0x7800c687, /*   r7 = ##-0x662fffcc } */
    0x9187c007, /* { r7 = memw(r7+#0x0) } */

    /* Jump to next_stage_entry, r1:0 now contains fdt_base: */
    0x5287c000, /* { jumpr r7 } */
    0x0, /* Invalid packet */
    0x0, /* Pad for fdt_base_high */
    0x0, /* Pad for fdt_base_low */
    0x0, /* Pad for next_stage_entry */
};

enum {
    FDT_HI = 11,
    FDT_LO,
    ENTRY_ADDR,
};


static void hexagon_load_initrd(MachineState *machine, HexagonBootInfo *info)
{
    const char *filename = machine->initrd_filename;
    uint64_t mem_size = machine->ram_size;
    void *fdt = machine->fdt;
    hwaddr start, end;
    ssize_t size;

    g_assert(filename != NULL);

    /*
     * Place the initrd in RAM after the kernel, with enough space to avoid
     * kernel decompression clobbering it. Following ARM/RISC-V approach:
     * - For smaller memory systems (< 1GB), place at halfway point
     * - For larger systems, place at 512MB to allow large kernels
     * - Ensure it's after the kernel image with some padding
     */
    if (mem_size < 1 * GiB) {
        start = mem_size / 2;
    } else {
        start = 512 * MiB;
    }

    /* Ensure we're after the kernel image with at least 64MB padding */
    if (start < info->image_high_addr + 64 * MiB) {
        start = info->image_high_addr + 64 * MiB;
    }

    start = QEMU_ALIGN_UP(start, 4 * MiB); /* Align to 4MB boundary */

    size = load_ramdisk(filename, start, mem_size - start);
    if (size == -1) {
        size = load_image_targphys(filename, start, mem_size - start, NULL);
        if (size == -1) {
            error_report("could not load ramdisk '%s'", filename);
            exit(1);
        }
    }

    info->initrd_start = start;
    info->initrd_size = size;

    if (fdt) {
        end = start + size;
        qemu_fdt_setprop_u64(fdt, "/chosen", "linux,initrd-start", start);
        qemu_fdt_setprop_u64(fdt, "/chosen", "linux,initrd-end", end);
    }
}

static uint64_t load_kernel(HexagonVirtMachineState *vms)
{
    MachineState *ms = MACHINE(vms);
    HexagonBootInfo *info = &vms->bootinfo;
    uint64_t entry = 0;
    uint64_t lowaddr = 0, highaddr = 0;

    if (load_elf_ram_sym(ms->kernel_filename, NULL, NULL, NULL, &entry,
                         &lowaddr, &highaddr, NULL, 0, EM_HEXAGON, 0, 0,
                         &address_space_memory, false, NULL) > 0) {
        info->kernel_start = entry;
        info->image_low_addr = lowaddr;
        info->image_high_addr = highaddr;
        info->kernel_size = highaddr - lowaddr;

        /* Load initrd if specified */
        if (ms->initrd_filename) {
            hexagon_load_initrd(ms, info);
        }

        return entry;
    }
    error_report("error loading '%s'", ms->kernel_filename);
    exit(1);
}

static uint64_t setup_boot(HexagonVirtMachineState *vms)
{
    uint64_t entry_addr = load_kernel(vms);
    uint32_t entry_addr_low = extract64(entry_addr, 0, 32);

    uint64_t fdt_base = base_memmap[VIRT_FDT].base;
    uint32_t fdt_base_low = extract64(fdt_base, 0, 32);
    uint32_t fdt_base_high = extract64(fdt_base, 32, 32);
    bootloader[FDT_LO] = cpu_to_le32(fdt_base_low);
    bootloader[FDT_HI] = cpu_to_le32(fdt_base_high);
    bootloader[ENTRY_ADDR] = cpu_to_le32(entry_addr_low);

    uint64_t bootl_base = base_memmap[VIRT_BOOT].base;
    g_assert(sizeof(bootloader) <= base_memmap[VIRT_BOOT].size);
    rom_add_blob_fixed_as("bootloader", bootloader, sizeof(bootloader),
        bootl_base, &address_space_memory);

    return bootl_base;
}

static uint64_t load_bios(HexagonVirtMachineState *vms)
{
    MachineState *ms = MACHINE(vms);
    /* Load BIOS at reset vector address 0x0 */
    uint64_t bios_addr = 0x0;
    int bios_size;

    bios_size = load_image_targphys(ms->firmware, bios_addr, 64 * 1024, NULL);
    if (bios_size < 0) {
        error_report("Could not load BIOS '%s'", ms->firmware);
        exit(1);
    }

    return bios_addr;
}

static void do_cpu_reset(void *opaque)
{
    HexagonCPU *cpu = opaque;
    CPUState *cs = CPU(cpu);
    cpu_reset(cs);
}

static void virt_init(MachineState *ms)
{
    HexagonVirtMachineState *vms = HEXAGON_VIRT_MACHINE(ms);
    Error **errp = NULL;
    const hexagon_machine_config *m_cfg = &v68n_1024;

    /*
     * If an external DTB is specified, load it instead of generating one.
     * Load it early so that runtime properties (like initrd) can be added.
     */
    if (ms->dtb) {
        ms->fdt = load_device_tree(ms->dtb, &vms->fdt_size);
        if (!ms->fdt) {
            error_report("load_device_tree() failed");
            exit(1);
        }
    }

    /* Set kernel command line in chosen node only if using generated FDT */
    if (ms->dtb == NULL && ms->kernel_cmdline && *ms->kernel_cmdline) {
        qemu_fdt_setprop_string(ms->fdt, "/chosen", "bootargs",
                                ms->kernel_cmdline);
    }

    vms->sys = get_system_memory();

    memory_region_init_ram(&vms->ram, NULL, "ddr.ram", ms->ram_size, errp);
    memory_region_add_subregion(vms->sys, 0x0, &vms->ram);

    if (m_cfg->l2tcm_size) {
        memory_region_init_ram(&vms->tcm, NULL, "tcm.ram", m_cfg->l2tcm_size,
                               errp);
        memory_region_add_subregion(vms->sys, m_cfg->cfgtable.l2tcm_base << 16,
                                    &vms->tcm);
    }

    memory_region_init_rom(&vms->cfgtable, NULL, "config_table.rom",
                           sizeof(m_cfg->cfgtable), errp);
    memory_region_add_subregion(vms->sys, m_cfg->cfgbase, &vms->cfgtable);

    /* Only add dynamic device tree nodes if using generated FDT */
    if (ms->dtb == NULL) {
        fdt_add_hvx(vms, m_cfg, errp);
    }

    const char *cpu_model = ms->cpu_type;

    if (!cpu_model) {
        cpu_model = HEXAGON_CPU_TYPE_NAME("v73");
    }

    vms->gsregs = qdev_new(TYPE_HEXAGON_GLOBALREG);
    create_qtimer(vms, m_cfg);
    vms->l2vic = qdev_new(TYPE_L2VIC);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(vms->l2vic), errp);

    HexagonCPU **cpus = g_malloc_n(ms->smp.cpus, sizeof(HexagonCPU *));
    for (int i = 0; i < ms->smp.cpus; i++) {
        HexagonCPU *cpu = HEXAGON_CPU(object_new(ms->cpu_type));
        cpus[i] = cpu;
        qemu_register_reset(do_cpu_reset, cpu);

        if (i == 0) {
            if (ms->kernel_filename) {
                uint64_t entry = setup_boot(vms);
                qdev_prop_set_uint32(vms->gsregs, "boot-evb", entry);
            } else if (ms->firmware) {
                uint64_t entry = load_bios(vms);
                qdev_prop_set_uint32(vms->gsregs, "boot-evb", entry);
            }
        }
        qdev_prop_set_bit(DEVICE(cpu), "start-powered-off", (i != 0));
        qdev_prop_set_uint32(DEVICE(cpu), "hvx-contexts",
                             m_cfg->cfgtable.ext_contexts);
        qdev_prop_set_uint32(DEVICE(cpu), "thread-count", ms->smp.cpus);
        qdev_prop_set_uint32(DEVICE(cpu), "vtcm-base-addr",
                             m_cfg->cfgtable.vtcm_base << 16);
        qdev_prop_set_uint32(DEVICE(cpu), "vtcm-size-kb",
                             m_cfg->cfgtable.vtcm_size_kb);
        qdev_prop_set_uint32(DEVICE(cpu), "l2line-size",
                             m_cfg->cfgtable.l2line_size);
        qdev_prop_set_bit(DEVICE(cpu), "coproc2-bfloat",
                             (m_cfg->cfgtable.coproc2_fp16_acc_exp >> 0) & 1);
        qdev_prop_set_bit(DEVICE(cpu), "hvx-bfloat",
                             (m_cfg->cfgtable.coproc2_fp16_acc_exp >> 1) & 1);
    }

    /* Create TLB object first */
    DeviceState *tlb_dev = qdev_new(TYPE_HEXAGON_TLB);
    object_property_add_child(OBJECT(ms), "hexagon-tlb", OBJECT(tlb_dev));

    /* Set the number of TLB entries based on machine configuration */
    qdev_prop_set_uint32(tlb_dev, "num-entries",
                         m_cfg->cfgtable.jtlb_size_entries);
    qdev_prop_set_uint32(tlb_dev, "dma-entries",
                         m_cfg->cfgtable.dma_jtlb_entries);

    if (!sysbus_realize(SYS_BUS_DEVICE(tlb_dev), errp)) {
        error_report("Failed to realize TLB object");
        goto out;
    }

    object_property_add_child(OBJECT(ms), "global-regs", OBJECT(vms->gsregs));
    qdev_prop_set_uint64(vms->gsregs, "config-table-addr", m_cfg->cfgbase);
    g_assert(m_cfg->rev_id);
    qdev_prop_set_uint32(vms->gsregs, "dsp-rev", m_cfg->rev_id);

    /* Link the qtimer interface to globalreg */
    if (!object_property_set_link(OBJECT(vms->gsregs), "qtimer",
                                  OBJECT(vms->qtimer), errp)) {
        error_report("Failed to link qtimer interface to global registers");
        goto out;
    }

    /* Link the L2VIC interface to globalreg */
    if (!object_property_set_link(OBJECT(vms->gsregs), "l2vic",
                                  OBJECT(vms->l2vic), errp)) {
        error_report("Failed to link L2VIC interface to global registers");
        goto out;
    }

    /* Realize the device on sysbus */
    sysbus_realize_and_unref(SYS_BUS_DEVICE(vms->gsregs), errp);

    /* Link the global system registers object to all CPUs */
    for (int i = 0; i < ms->smp.cpus; i++) {
        if (!object_property_set_link(OBJECT(cpus[i]), "global-regs",
                                      OBJECT(vms->gsregs), errp)) {
            error_report("Failed to link global system registers to CPU %d", i);
            goto out;
        }
        if (!object_property_set_link(OBJECT(cpus[i]), "tlb",
                                      OBJECT(tlb_dev), errp)) {
            error_report("Failed to link TLB to CPU %d", i);
            goto out;
        }
        if (!object_property_set_link(OBJECT(cpus[i]), "l2vic",
                                      OBJECT(vms->l2vic), errp)) {
            error_report("Failed to link L2VIC interface to CPU %d", i);
            goto out;
        }
        if (!qdev_realize_and_unref(DEVICE(cpus[i]), NULL, errp)) {
            error_report("Failed to realize CPU %d", i);
            goto out;
        }
    }

    for (int i = 0; i < 8; i++) {
        sysbus_connect_irq(SYS_BUS_DEVICE(vms->l2vic), i,
            qdev_get_gpio_in(DEVICE(cpus[0]), i));
    }

    /* Only add device tree nodes if using generated FDT */
    if (ms->dtb == NULL) {
        fdt_add_hvm_pic_node(vms, m_cfg);
        fdt_add_virtio_devices(vms);
        fdt_add_cpu_nodes(vms);
        fdt_add_clocks(vms);
        fdt_add_uart(vms, VIRT_UART0);
        fdt_add_gpt_node(vms);
    }
    sysbus_connect_irq(SYS_BUS_DEVICE(vms->qtimer), 0,
                       qdev_get_gpio_in(vms->l2vic, irqmap[VIRT_QTMR0]));
    sysbus_connect_irq(SYS_BUS_DEVICE(vms->qtimer), 1,
                       qdev_get_gpio_in(vms->l2vic, irqmap[VIRT_QTMR1]));
    create_pll(vms);
    fdt_add_pll_node(vms);

    hexagon_config_table *config_table =
        (hexagon_config_table *)&m_cfg->cfgtable;

    /* FIXME: can we fix this? */
    config_table->subsystem_base = HEXAGON_CFG_ADDR_BASE(m_cfg->csr_base);

    /* Create a copy with little-endian byte order for guest memory */
    hexagon_config_table *guest_config_table = g_malloc(sizeof(*config_table));
    memcpy(guest_config_table, config_table, sizeof(*config_table));

    /* Convert all uint32_t fields to little-endian for the guest */
    for (int i = 0; i < ARRAY_SIZE(guest_config_table->raw); i++) {
        guest_config_table->raw[i] = cpu_to_le32(guest_config_table->raw[i]);
    }

    rom_add_blob_fixed_as("config_table.rom", guest_config_table,
                          sizeof(*guest_config_table), m_cfg->cfgbase,
                          &address_space_memory);
    g_free(guest_config_table);

    hexagon_load_fdt(vms);
out:
    g_free(cpus);
}


static void virt_class_init(ObjectClass *oc, const void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);

    mc->init = virt_init;
    mc->default_cpu_type = HEXAGON_CPU_TYPE_NAME("v81");
    mc->default_ram_size = 4 * GiB;
    mc->max_cpus = THREADS_MAX;
    mc->default_cpus = THREADS_MAX;
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
    .parent = TYPE_MACHINE,
    .instance_size = sizeof(HexagonVirtMachineState),
    .class_init = virt_class_init,
    .instance_init = virt_instance_init,
    .instance_finalize = virt_instance_finalize,
} };

DEFINE_TYPES(virt_machine_types)
