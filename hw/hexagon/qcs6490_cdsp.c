/*
 * Qualcomm QCS6490 Compute DSP (CDSP) machine.
 *
 * Models the Hexagon v68 CDSP subsystem of the QCS6490 (SM7325 family) as
 * seen on the RubikPi 3.  The CDSP runs the H2 hypervisor and Linux as its
 * guest, so the machine loads H2's "loadlinux" as the firmware and the
 * kernel at the physical address loadlinux expects, and describes the
 * subsystem to the kernel with a generated device tree.
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "qapi/error.h"
#include "elf.h"
#include "hw/char/pl011.h"
#include "hw/core/boards.h"
#include "hw/core/clock.h"
#include "hw/core/loader.h"
#include "hw/core/qdev-clock.h"
#include "hw/core/qdev-properties.h"
#include "hw/core/sysbus.h"
#include "hw/hexagon/hexagon.h"
#include "hw/hexagon/hexagon_globalreg.h"
#include "hw/hexagon/hexagon_tlb.h"
#include "hw/intc/l2vic.h"
#include "hw/timer/qct-qtimer.h"
#include "hw/misc/unimp.h"
#include "qemu/datadir.h"
#include "qemu/error-report.h"
#include "qemu/guest-random.h"
#include "qemu/units.h"
#include "system/address-spaces.h"
#include "system/device_tree.h"
#include "system/reset.h"
#include "system/system.h"
#include "target/hexagon/cpu.h"
#include <libfdt.h>

#include "machine_cfg_qcs6490_cdsp.h.inc"

#define TYPE_QCS6490_CDSP_MACHINE MACHINE_TYPE_NAME("qcs6490-cdsp")
OBJECT_DECLARE_SIMPLE_TYPE(Qcs6490CdspMachineState, QCS6490_CDSP_MACHINE)

struct Qcs6490CdspMachineState {
    MachineState parent_obj;

    int fdt_size;
    hwaddr fdt_addr;
    char *firmware_path;
    Clock *apb_clk;
    DeviceState *l2vic;
};

/*
 * Physical address loadlinux is built to find the kernel at.  It matches
 * where the RubikPi's Linux-on-CDSP flow places the kernel.
 */
#define QCS6490_KERNEL_ADDR 0xa1000000

/*
 * The firmware built for the address above, used when -kernel has no -bios.
 * Unlike the "virt" machine's loadlinux (linked to run at DDR base 0), this
 * one is linked to run inside the QCS6490's DDR window, at ddr_base +
 * 0x08f00000.
 */
#define QCS6490_DEFAULT_FIRMWARE "hexagon_loadlinux_qcs6490_cdsp"

/* The kernel maps at most this much RAM, starting at its load address. */
#define QCS6490_MAX_KERNEL_RAM (896 * MiB)

enum {
    QCS6490_UART0,
    QCS6490_GPT,
    QCS6490_BOOT,
    QCS6490_SS_CSR,
    QCS6490_CFGSPACE,
};

/*
 * The UART is a QEMU-only console: the CDSP has none.  It, and the fake
 * "gpt" window the kernel's H2 timer driver binds to, live outside any
 * address the real subsystem is known to use.
 */
static const MemMapEntry qcs6490_memmap[] = {
    [QCS6490_UART0] = { 0x10000000, 0x00001000 },
    [QCS6490_GPT] = { 0xab000000, 0x00001000 },
    [QCS6490_BOOT] = { 0x99c00000, 0x00000200 },
    /* Reads 2 at offset 0 on the hardware; nothing else is known. */
    [QCS6490_SS_CSR] = { 0x0a380000, 0x00010000 },
    /*
     * Core config, L2 config, CLADE and ECC register windows, which only H2
     * touches.  Modelled as scratch memory until the registers are.
     */
    [QCS6490_CFGSPACE] = { 0x09990000, 0x00070000 },
};

static const int QCS6490_UART0_IRQ = 15;
static const int QCS6490_GPT_IRQ = 12;
/* Interrupt specifiers name l2vic lines offset by the 32 per-cpu lines. */
#define QCS6490_L2VIC_SPI_BASE 32

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

static void qcs6490_create_fdt(Qcs6490CdspMachineState *qms)
{
    MachineState *ms = MACHINE(qms);
    void *fdt = create_device_tree(&qms->fdt_size);
    uint8_t rng_seed[32];

    if (!fdt) {
        error_report("create_device_tree() failed");
        exit(1);
    }

    ms->fdt = fdt;

    qemu_fdt_setprop_cell(fdt, "/", "#address-cells", 0x2);
    qemu_fdt_setprop_cell(fdt, "/", "#size-cells", 0x1);
    qemu_fdt_setprop_string(fdt, "/", "model", "Qualcomm QCS6490 CDSP");
    qemu_fdt_setprop_string(fdt, "/", "compatible", "qcom,qcs6490-cdsp");

    qemu_fdt_add_subnode(fdt, "/soc");
    qemu_fdt_setprop_string(fdt, "/soc", "compatible", "simple-bus");
    qemu_fdt_setprop_cell(fdt, "/soc", "#address-cells", 0x2);
    qemu_fdt_setprop_cell(fdt, "/soc", "#size-cells", 0x1);
    qemu_fdt_setprop(fdt, "/soc", "ranges", NULL, 0);

    qemu_fdt_add_subnode(fdt, "/chosen");
    qemu_guest_getrandom_nofail(rng_seed, sizeof(rng_seed));
    qemu_fdt_setprop(fdt, "/chosen", "rng-seed", rng_seed, sizeof(rng_seed));
}

static int32_t qcs6490_fdt_add_l2vic(Qcs6490CdspMachineState *qms)
{
    MachineState *ms = MACHINE(qms);
    int32_t l2vic_phandle = qemu_fdt_alloc_phandle(ms->fdt);
    g_autofree char *nodename = g_strdup_printf("/soc/interrupt-controller@%x",
                                                qcs6490_cdsp.l2vic_base);
    static const char compat[] = "qcom,h2-pic\0hvm-pic";

    qemu_fdt_setprop_cell(ms->fdt, "/soc", "interrupt-parent", l2vic_phandle);

    qemu_fdt_add_subnode(ms->fdt, nodename);
    qemu_fdt_setprop_cell(ms->fdt, nodename, "#address-cells", 0x0);
    qemu_fdt_setprop_cell(ms->fdt, nodename, "#interrupt-cells", 0x2);
    qemu_fdt_setprop(ms->fdt, nodename, "compatible", compat, sizeof(compat));
    qemu_fdt_setprop_cells(ms->fdt, nodename, "reg", 0,
                           qcs6490_cdsp.l2vic_base, qcs6490_cdsp.l2vic_size);
    qemu_fdt_setprop(ms->fdt, nodename, "interrupt-controller", NULL, 0);
    qemu_fdt_setprop_cell(ms->fdt, nodename, "phandle", l2vic_phandle);

    return l2vic_phandle;
}

/*
 * The H2 hypervisor provides the guest timer through hypercalls, so this
 * node only tells the kernel's timer driver which interrupt to expect.
 */
static void qcs6490_fdt_add_gpt(Qcs6490CdspMachineState *qms)
{
    static const char compat[] = "qcom,h2-timer\0hvm-timer";
    MachineState *ms = MACHINE(qms);
    g_autofree char *name = g_strdup_printf("/soc/gpt@%" PRIx64,
                                            qcs6490_memmap[QCS6490_GPT].base);

    qemu_fdt_add_subnode(ms->fdt, name);
    qemu_fdt_setprop(ms->fdt, name, "compatible", compat, sizeof(compat));
    qemu_fdt_setprop_cells(ms->fdt, name, "interrupts", QCS6490_GPT_IRQ, 0);
    qemu_fdt_setprop_cells(ms->fdt, name, "reg", 0x0,
                           qcs6490_memmap[QCS6490_GPT].base,
                           qcs6490_memmap[QCS6490_GPT].size);
}

static void qcs6490_fdt_add_hvx(Qcs6490CdspMachineState *qms)
{
    const MachineState *ms = MACHINE(qms);
    const hexagon_config_table *t = &qcs6490_cdsp.cfgtable;

    qemu_fdt_add_subnode(ms->fdt, "/soc/vtcm");
    qemu_fdt_setprop_string(ms->fdt, "/soc/vtcm", "compatible",
                            "qcom,hexagon_vtcm");
    qemu_fdt_setprop_cells(ms->fdt, "/soc/vtcm", "reg", 0, t->vtcm_base << 16,
                           t->vtcm_size_kb * 1024);

    qemu_fdt_add_subnode(ms->fdt, "/soc/hvx");
    qemu_fdt_setprop_string(ms->fdt, "/soc/hvx", "compatible",
                            "qcom,hexagon-hvx");
    qemu_fdt_setprop_cells(ms->fdt, "/soc/hvx", "qcom,hvx-max-ctxts",
                           t->ext_contexts);
    qemu_fdt_setprop_cells(ms->fdt, "/soc/hvx", "qcom,hvx-vlength",
                           t->hvx_vec_log_length);
}

static void qcs6490_fdt_add_cpus(const Qcs6490CdspMachineState *qms)
{
    MachineState *ms = MACHINE(qms);
    /* loadlinux gives the kernel a virtual CPU for every thread that runs. */
    int guest_cpus = ms->smp.cpus;

    qemu_fdt_add_subnode(ms->fdt, "/cpus");
    qemu_fdt_setprop_cell(ms->fdt, "/cpus", "#address-cells", 0x1);
    qemu_fdt_setprop_cell(ms->fdt, "/cpus", "#size-cells", 0x0);

    for (int num = guest_cpus - 1; num >= 0; num--) {
        g_autofree char *nodename = g_strdup_printf("/cpus/cpu@%d", num);

        qemu_fdt_add_subnode(ms->fdt, nodename);
        qemu_fdt_setprop_string(ms->fdt, nodename, "device_type", "cpu");
        qemu_fdt_setprop_cell(ms->fdt, nodename, "reg", num);
        qemu_fdt_setprop_cell(ms->fdt, nodename, "phandle",
                              qemu_fdt_alloc_phandle(ms->fdt));
    }
}

static void qcs6490_fdt_add_memory(const Qcs6490CdspMachineState *qms)
{
    MachineState *ms = MACHINE(qms);
    hwaddr ram_end = qcs6490_cdsp.ddr_base + ms->ram_size;
    hwaddr size = MIN(ram_end - QCS6490_KERNEL_ADDR, QCS6490_MAX_KERNEL_RAM);
    g_autofree char *nodename = g_strdup_printf("/memory@%x",
                                                QCS6490_KERNEL_ADDR);

    qemu_fdt_add_subnode(ms->fdt, nodename);
    qemu_fdt_setprop_string(ms->fdt, nodename, "device_type", "memory");
    qemu_fdt_setprop_cells(ms->fdt, nodename, "reg", 0, QCS6490_KERNEL_ADDR,
                           size);
}

/*
 * A QEMU-only console: the CDSP has no UART of its own, but the guest needs
 * somewhere to print.
 */
static void qcs6490_create_uart(Qcs6490CdspMachineState *qms,
                                int32_t l2vic_phandle)
{
    static const char compat[] = "arm,pl011\0arm,primecell";
    static const char clocknames[] = "uartclk\0apb_pclk";
    MachineState *ms = MACHINE(qms);
    hwaddr base = qcs6490_memmap[QCS6490_UART0].base;
    hwaddr size = qcs6490_memmap[QCS6490_UART0].size;
    int32_t clk_phandle = qemu_fdt_alloc_phandle(ms->fdt);
    g_autofree char *nodename = g_strdup_printf("/pl011@%" PRIx64, base);
    DeviceState *dev = qdev_new(TYPE_PL011);
    SysBusDevice *s = SYS_BUS_DEVICE(dev);

    qms->apb_clk = clock_new(OBJECT(ms), "apb-pclk");
    clock_set_hz(qms->apb_clk, 24000000);

    qdev_prop_set_chr(dev, "chardev", serial_hd(0));
    qdev_connect_clock_in(dev, "clk", qms->apb_clk);
    sysbus_realize_and_unref(s, &error_fatal);
    sysbus_mmio_map(s, 0, base);
    sysbus_connect_irq(s, 0, qdev_get_gpio_in(qms->l2vic, QCS6490_UART0_IRQ));

    qemu_fdt_add_subnode(ms->fdt, "/apb-pclk");
    qemu_fdt_setprop_string(ms->fdt, "/apb-pclk", "compatible", "fixed-clock");
    qemu_fdt_setprop_cell(ms->fdt, "/apb-pclk", "#clock-cells", 0x0);
    qemu_fdt_setprop_cell(ms->fdt, "/apb-pclk", "clock-frequency", 24000000);
    qemu_fdt_setprop_string(ms->fdt, "/apb-pclk", "clock-output-names",
                            "clk24mhz");
    qemu_fdt_setprop_cell(ms->fdt, "/apb-pclk", "phandle", clk_phandle);

    qemu_fdt_add_subnode(ms->fdt, nodename);
    /* Can't use setprop_string because of the embedded NUL */
    qemu_fdt_setprop(ms->fdt, nodename, "compatible", compat, sizeof(compat));
    qemu_fdt_setprop_cells(ms->fdt, nodename, "reg", 0, base, size);
    qemu_fdt_setprop_cells(ms->fdt, nodename, "interrupts",
                           QCS6490_L2VIC_SPI_BASE + QCS6490_UART0_IRQ, 0);
    qemu_fdt_setprop_cell(ms->fdt, nodename, "interrupt-parent",
                          l2vic_phandle);
    qemu_fdt_setprop_cells(ms->fdt, nodename, "clocks", clk_phandle,
                           clk_phandle);
    qemu_fdt_setprop(ms->fdt, nodename, "clock-names", clocknames,
                     sizeof(clocknames));

    qemu_fdt_setprop_string(ms->fdt, "/chosen", "stdout-path", nodename);
    qemu_fdt_add_subnode(ms->fdt, "/aliases");
    qemu_fdt_setprop_string(ms->fdt, "/aliases", "serial0", nodename);
}

static uint64_t qcs6490_kernel_translate(void *opaque, uint64_t addr)
{
    return addr + QCS6490_KERNEL_ADDR;
}

/*
 * Through firmware, the kernel ELF (linked at physical address 0, and
 * relocating itself to wherever it finds it) is loaded where the firmware
 * expects it.  Without firmware it is loaded where it was linked and
 * entered directly.
 */
static uint64_t qcs6490_load_kernel(MachineState *ms, bool through_firmware,
                                    hwaddr *image_high)
{
    uint64_t entry = 0;
    uint64_t highaddr = 0;

    if (load_elf_ram_sym(ms->kernel_filename, NULL,
                         through_firmware ? qcs6490_kernel_translate : NULL,
                         NULL, &entry, NULL, &highaddr, NULL, 0, EM_HEXAGON,
                         0, 0, &address_space_memory, false, NULL) <= 0) {
        error_report("error loading '%s'", ms->kernel_filename);
        exit(1);
    }
    *image_high = highaddr;
    return entry;
}

static uint64_t qcs6490_load_firmware(Qcs6490CdspMachineState *qms)
{
    uint64_t entry = 0;

    if (load_elf_ram_sym(qms->firmware_path, NULL, NULL, NULL, &entry, NULL,
                         NULL, NULL, 0, EM_HEXAGON, 0, 0,
                         &address_space_memory, false, NULL) <= 0) {
        error_report("Could not load firmware '%s'", qms->firmware_path);
        exit(1);
    }
    return entry;
}

/*
 * Place the FDT after the kernel image, inside the RAM window the kernel
 * maps, and the initrd after that.
 */
static void qcs6490_load_initrd(Qcs6490CdspMachineState *qms)
{
    MachineState *ms = MACHINE(qms);
    hwaddr start = qms->fdt_addr + 4 * MiB;
    ssize_t size;

    if (!ms->initrd_filename) {
        return;
    }

    size = load_image_targphys_as(ms->initrd_filename, start,
                                  qcs6490_cdsp.ddr_base + ms->ram_size - start,
                                  &address_space_memory, &error_fatal);

    qemu_fdt_setprop_u64(ms->fdt, "/chosen", "linux,initrd-start", start);
    qemu_fdt_setprop_u64(ms->fdt, "/chosen", "linux,initrd-end", start + size);
}

/*
 * A boot stub that passes the FDT address to the firmware in r1:r0, then
 * jumps to the firmware's entry point.
 */
static uint64_t qcs6490_setup_boot_stub(Qcs6490CdspMachineState *qms,
                                        uint64_t firmware_entry)
{
    hwaddr bootl_base = qcs6490_memmap[QCS6490_BOOT].base;

    bootloader[FDT_LO] = cpu_to_le32(extract64(qms->fdt_addr, 0, 32));
    bootloader[FDT_HI] = cpu_to_le32(extract64(qms->fdt_addr, 32, 32));
    bootloader[ENTRY_ADDR] = cpu_to_le32(extract64(firmware_entry, 0, 32));

    g_assert(sizeof(bootloader) <= qcs6490_memmap[QCS6490_BOOT].size);
    rom_add_blob_fixed_as("bootloader", bootloader, sizeof(bootloader),
                          bootl_base, &address_space_memory);

    return bootl_base;
}

static void qcs6490_load_fdt(Qcs6490CdspMachineState *qms)
{
    MachineState *ms = MACHINE(qms);

    rom_add_blob_fixed_as("fdt", ms->fdt, qms->fdt_size, qms->fdt_addr,
                          &address_space_memory);
    qemu_register_reset_nosnapshotload(
        qemu_fdt_randomize_seeds,
        rom_ptr_for_as(&address_space_memory, qms->fdt_addr, qms->fdt_size));
}

/*
 * A kernel is booted through the bundled firmware unless "-bios none" asks
 * for it to be entered directly.
 */
static void qcs6490_resolve_firmware(Qcs6490CdspMachineState *qms)
{
    MachineState *ms = MACHINE(qms);
    const char *name = ms->firmware;

    if (!name && ms->kernel_filename) {
        name = QCS6490_DEFAULT_FIRMWARE;
    }
    if (!name || !strcmp(name, "none")) {
        return;
    }

    qms->firmware_path = qemu_find_file(QEMU_FILE_TYPE_BIOS, name);
    if (!qms->firmware_path) {
        error_report("Could not find firmware '%s'", name);
        exit(1);
    }
}

static void do_cpu_reset(void *opaque)
{
    HexagonCPU *cpu = opaque;
    CPUState *cs = CPU(cpu);
    cpu_reset(cs);
}

static void qcs6490_cdsp_init(MachineState *ms)
{
    Qcs6490CdspMachineState *qms = QCS6490_CDSP_MACHINE(ms);
    MemoryRegion *sysmem = get_system_memory();
    MemoryRegion *cfgspace = g_new(MemoryRegion, 1);
    hwaddr image_high = 0;
    int32_t l2vic_phandle;

    if (ms->kernel_filename &&
        qcs6490_cdsp.ddr_base + ms->ram_size <= QCS6490_KERNEL_ADDR) {
        error_report("RAM size (%" PRIu64 " MiB) too small: the kernel is "
                     "loaded at 0x%x", ms->ram_size / MiB,
                     QCS6490_KERNEL_ADDR);
        exit(1);
    }

    qcs6490_create_fdt(qms);
    qcs6490_resolve_firmware(qms);
    qemu_fdt_setprop_string(ms->fdt, "/chosen", "bootargs", ms->kernel_cmdline);

    /* DDR. */
    MemoryRegion *ram = g_new(MemoryRegion, 1);
    memory_region_init_ram(ram, NULL, "ddr.ram", ms->ram_size, &error_fatal);
    memory_region_add_subregion(sysmem, qcs6490_cdsp.ddr_base, ram);

    /* Config-table ROM, filled in below once the layout is fixed. */
    MemoryRegion *cfgtable_rom = g_new(MemoryRegion, 1);
    memory_region_init_rom(cfgtable_rom, NULL, "config_table.rom",
                           sizeof(qcs6490_cdsp.cfgtable), &error_fatal);
    memory_region_add_subregion(sysmem, qcs6490_cdsp.cfgbase, cfgtable_rom);

    uint32_t vtcm_size_bytes = qcs6490_cdsp.cfgtable.vtcm_size_kb * 1024;
    if (vtcm_size_bytes > 0) {
        MemoryRegion *vtcm = g_new(MemoryRegion, 1);
        memory_region_init_ram(vtcm, NULL, "vtcm.ram", vtcm_size_bytes,
                               &error_fatal);
        memory_region_add_subregion(sysmem,
                                    qcs6490_cdsp.cfgtable.vtcm_base << 16,
                                    vtcm);
    }

    DeviceState *glob_regs_dev = qdev_new(TYPE_HEXAGON_GLOBALREG);
    object_property_add_child(OBJECT(ms), "global-regs",
                              OBJECT(glob_regs_dev));
    qdev_prop_set_uint64(glob_regs_dev, "config-table-addr",
                         qcs6490_cdsp.cfgbase);
    qdev_prop_set_uint32(glob_regs_dev, "dsp-rev", v68_qcs6490_rev);

    DeviceState *tlb_dev = qdev_new(TYPE_HEXAGON_TLB);
    object_property_add_child(OBJECT(ms), "hexagon-tlb", OBJECT(tlb_dev));
    qdev_prop_set_uint32(tlb_dev, "num-entries",
                         qcs6490_cdsp.cfgtable.jtlb_size_entries);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(tlb_dev), &error_fatal);

    qms->l2vic = qdev_new(TYPE_L2VIC);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(qms->l2vic), &error_fatal);
    sysbus_mmio_map(SYS_BUS_DEVICE(qms->l2vic), 0, qcs6490_cdsp.l2vic_base);
    object_property_set_link(OBJECT(glob_regs_dev), "l2vic",
                             OBJECT(qms->l2vic), &error_fatal);

    QCTQtimerState *qtimer = QCT_QTIMER(qdev_new(TYPE_QCT_QTIMER));
    object_property_set_uint(OBJECT(qtimer), "nr_frames", 3, &error_fatal);
    object_property_set_uint(OBJECT(qtimer), "nr_views", 1, &error_fatal);
    object_property_set_uint(OBJECT(qtimer), "cnttid", 0x111, &error_fatal);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(qtimer), &error_fatal);
    sysbus_mmio_map(SYS_BUS_DEVICE(qtimer), 0, qcs6490_cdsp.csr_base);
    sysbus_mmio_map(SYS_BUS_DEVICE(qtimer), 1, qcs6490_cdsp.qtmr_region);
    /* H2 takes the timer on L2VIC line 2 (its "timer interrupt" 34). */
    for (int i = 0; i < 3; i++) {
        sysbus_connect_irq(SYS_BUS_DEVICE(qtimer), i,
                           qdev_get_gpio_in(qms->l2vic, 2 + i));
    }
    object_property_set_link(OBJECT(glob_regs_dev), "qtimer",
                             OBJECT(qtimer), &error_fatal);

    sysbus_realize_and_unref(SYS_BUS_DEVICE(glob_regs_dev), &error_fatal);

    /*
     * Behind everything else, so the L2VIC and other real devices near the
     * config table win where they overlap.
     */
    memory_region_init_ram(cfgspace, NULL, "cfgspace.ram",
                           qcs6490_memmap[QCS6490_CFGSPACE].size,
                           &error_fatal);
    memory_region_add_subregion_overlap(sysmem,
                                        qcs6490_memmap[QCS6490_CFGSPACE].base,
                                        cfgspace, -1);
    create_unimplemented_device("qcs6490-cdsp.ss-csr",
                                qcs6490_memmap[QCS6490_SS_CSR].base,
                                qcs6490_memmap[QCS6490_SS_CSR].size);

    qcs6490_fdt_add_hvx(qms);
    l2vic_phandle = qcs6490_fdt_add_l2vic(qms);
    qcs6490_fdt_add_gpt(qms);
    qcs6490_fdt_add_cpus(qms);
    qcs6490_fdt_add_memory(qms);
    qcs6490_create_uart(qms, l2vic_phandle);

    g_autofree HexagonCPU **cpus = g_new(HexagonCPU *, ms->smp.cpus);

    for (int i = 0; i < ms->smp.cpus; i++) {
        HexagonCPU *cpu = HEXAGON_CPU(object_new(ms->cpu_type));

        cpus[i] = cpu;
        qemu_register_reset(do_cpu_reset, cpu);

        qdev_prop_set_bit(DEVICE(cpu), "start-powered-off", (i != 0));
        qdev_prop_set_uint32(DEVICE(cpu), "dsp-rev", v68_qcs6490_rev);
        qdev_prop_set_uint32(DEVICE(cpu), "hvx-contexts",
                             qcs6490_cdsp.cfgtable.ext_contexts);
        qdev_prop_set_uint32(DEVICE(cpu), "jtlb-entries",
                             qcs6490_cdsp.cfgtable.jtlb_size_entries);
        qdev_prop_set_bit(DEVICE(cpu), "sched-limit", true);
        object_property_set_link(OBJECT(cpu), "global-regs",
                                 OBJECT(glob_regs_dev), &error_fatal);
        object_property_set_link(OBJECT(cpu), "tlb", OBJECT(tlb_dev),
                                 &error_fatal);
        object_property_set_link(OBJECT(cpu), "l2vic", OBJECT(qms->l2vic),
                                 &error_fatal);

        if (i == 0) {
            if (qms->firmware_path && ms->kernel_filename) {
                uint64_t firmware_entry = qcs6490_load_firmware(qms);

                qcs6490_load_kernel(ms, true, &image_high);
                qms->fdt_addr = QEMU_ALIGN_UP(image_high + 16 * MiB, 4 * MiB);
                qcs6490_load_initrd(qms);
                qdev_prop_set_uint32(DEVICE(cpu), "exec-start-addr",
                                     qcs6490_setup_boot_stub(qms,
                                                             firmware_entry));
            } else if (ms->kernel_filename) {
                qdev_prop_set_uint32(DEVICE(cpu), "exec-start-addr",
                                     qcs6490_load_kernel(ms, false,
                                                         &image_high));
            } else if (qms->firmware_path) {
                qdev_prop_set_uint32(DEVICE(cpu), "exec-start-addr",
                                     qcs6490_load_firmware(qms));
            }
        }
    }

    for (int i = 0; i < ms->smp.cpus; i++) {
        qdev_realize_and_unref(DEVICE(cpus[i]), NULL, &error_fatal);
    }

    for (int i = 0; i < 8; i++) {
        sysbus_connect_irq(SYS_BUS_DEVICE(qms->l2vic), i,
                           qdev_get_gpio_in(DEVICE(cpus[0]), i));
    }
    sysbus_mmio_map(SYS_BUS_DEVICE(qms->l2vic), 1,
                    qcs6490_cdsp.cfgtable.fastl2vic_base << 16);

    /* Convert to LE for guest memory */
    hexagon_config_table *guest_config_table = g_new(hexagon_config_table, 1);
    /*
     * Unlike hexagon_dsp.c's machines, keep the table's own subsystem_base:
     * H2 finds the L2VIC and the QTimer at fixed offsets from it, and on the
     * QCS6490 it is not the QTimer frame (csr_base).
     */
    memcpy(guest_config_table, &qcs6490_cdsp.cfgtable,
          sizeof(*guest_config_table));

    for (int i = 0; i < ARRAY_SIZE(guest_config_table->raw); i++) {
        guest_config_table->raw[i] = cpu_to_le32(guest_config_table->raw[i]);
    }

    rom_add_blob_fixed_as("config_table.rom", guest_config_table,
                          sizeof(*guest_config_table), qcs6490_cdsp.cfgbase,
                          &address_space_memory);
    g_free(guest_config_table);

    if (qms->firmware_path && ms->kernel_filename) {
        qcs6490_load_fdt(qms);
    }
}

static void qcs6490_cdsp_class_init(ObjectClass *oc, const void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);

    mc->desc = "Qualcomm QCS6490 Compute DSP";
    mc->init = qcs6490_cdsp_init;
    mc->default_cpu_type = HEXAGON_CPU_TYPE_NAME("v68");
    mc->default_ram_size = 1 * GiB;
    mc->max_cpus = 6;
    mc->default_cpus = 6;
    mc->is_default = false;
    mc->no_cdrom = 1;
    mc->no_floppy = 1;
    mc->no_parallel = 1;
    mc->numa_mem_supported = false;
}

static const TypeInfo qcs6490_cdsp_types[] = {
    {
        .name = TYPE_QCS6490_CDSP_MACHINE,
        .parent = TYPE_MACHINE,
        .instance_size = sizeof(Qcs6490CdspMachineState),
        .class_init = qcs6490_cdsp_class_init,
    },
};

DEFINE_TYPES(qcs6490_cdsp_types)
