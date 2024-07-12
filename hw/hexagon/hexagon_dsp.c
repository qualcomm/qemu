/*
 * Hexagon DSP Subsystem emulation.  This represents a generic DSP
 * subsystem with few peripherals, like the Compute DSP.
 *
 * Copyright (c) 2020-2024 Qualcomm Innovation Center, Inc. All Rights Reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */


#include "qemu/osdep.h"
#include "qemu/units.h"
#include "exec/address-spaces.h"
#include "hw/hw.h"
#include "hw/boards.h"
#include "hw/qdev-properties.h"
#include "hw/hexagon/hexagon.h"
#include "hw/timer/qct-qtimer.h"
#include "hw/intc/l2vic.h"
#include "hw/char/pl011.h"
#include "hw/loader.h"
#include "hw/timer/qct-qtimer.h"
#include "qapi/error.h"
#include "qemu/error-report.h"
#include "qemu/log.h"
#include "elf.h"
#include "cpu.h"
#include "include/migration/cpu.h"
#include "include/sysemu/sysemu.h"
#include "target/hexagon/internal.h"
#include "sysemu/reset.h"

#include "machine_configs.h.inc"

static void hex_symbol_callback(const char *st_name, int st_info,
                                uint64_t st_value, uint64_t st_size)
{
}

/* Board init.  */
static struct hexagon_board_boot_info hexagon_binfo;

static void hexagon_load_kernel(HexagonCPU *cpu)
{
    uint64_t pentry;
    long kernel_size;

    kernel_size = load_elf_ram_sym(hexagon_binfo.kernel_filename, NULL, NULL,
                      NULL, &pentry, NULL, NULL,
                      &hexagon_binfo.kernel_elf_flags, 0, EM_HEXAGON, 0, 0,
                      &address_space_memory, false, hex_symbol_callback);

    if (kernel_size <= 0) {
        error_report("no kernel file '%s'",
            hexagon_binfo.kernel_filename);
        exit(1);
    }

    qdev_prop_set_uint32(DEVICE(cpu), "exec-start-addr", pentry);
}

static void hexagon_init_bootstrap(MachineState *machine, HexagonCPU *cpu)
{
    if (machine->kernel_filename) {
        hexagon_load_kernel(cpu);
    }
}

static void do_cpu_reset(void *opaque)
{
    HexagonCPU *cpu = opaque;
    CPUState *cs = CPU(cpu);
    cpu_reset(cs);
}

static void hexagon_common_init(MachineState *machine, Rev_t rev,
                                hexagon_machine_config *m_cfg)
{
    memset(&hexagon_binfo, 0, sizeof(hexagon_binfo));
    if (machine->kernel_filename) {
        hexagon_binfo.ram_size = machine->ram_size;
        hexagon_binfo.kernel_filename = machine->kernel_filename;
    }

    machine->enable_graphics = 0;

    MemoryRegion *address_space = get_system_memory();

    MemoryRegion *sram = g_new(MemoryRegion, 1);
    memory_region_init_ram(sram, NULL, "ddr.ram",
        machine->ram_size, &error_fatal);
    memory_region_add_subregion(address_space, 0x0, sram);

    HexagonCPU *cpu_0 = NULL;
    Error **errp = NULL;

    for (int i = 0; i < machine->smp.cpus; i++) {
        HexagonCPU *cpu = HEXAGON_CPU(object_new(machine->cpu_type));
        qemu_register_reset(do_cpu_reset, cpu);

        QCTQtimerState *qtimer = QCT_QTIMER(qdev_new(TYPE_QCT_QTIMER));

        object_property_set_uint(OBJECT(qtimer), "nr_frames",
                2, &error_fatal);
        object_property_set_uint(OBJECT(qtimer), "nr_views",
                1, &error_fatal);
        object_property_set_uint(OBJECT(qtimer), "cnttid",
                0x111, &error_fatal);
        sysbus_realize_and_unref(SYS_BUS_DEVICE(qtimer), &error_fatal);

        DeviceState *l2vic_dev;
        l2vic_dev = sysbus_create_varargs("l2vic", m_cfg->l2vic_base,
                /* IRQ#, Evnt#,CauseCode */
                qdev_get_gpio_in(DEVICE(cpu), 0),
                qdev_get_gpio_in(DEVICE(cpu), 1),
                qdev_get_gpio_in(DEVICE(cpu), 2),
                qdev_get_gpio_in(DEVICE(cpu), 3),
                qdev_get_gpio_in(DEVICE(cpu), 4),
                qdev_get_gpio_in(DEVICE(cpu), 5),
                qdev_get_gpio_in(DEVICE(cpu), 6),
                qdev_get_gpio_in(DEVICE(cpu), 7),
                NULL);
        sysbus_mmio_map(SYS_BUS_DEVICE(l2vic_dev), 1,
            m_cfg->cfgtable.fastl2vic_base);

        sysbus_connect_irq(SYS_BUS_DEVICE(qtimer), 0,
                qdev_get_gpio_in(l2vic_dev, 3));
        sysbus_connect_irq(SYS_BUS_DEVICE(qtimer), 1,
                qdev_get_gpio_in(l2vic_dev, 4));


        /*
         * CPU #0 is the only CPU running at boot, others must be
         * explicitly enabled via start instruction.
         */
        qdev_prop_set_bit(DEVICE(cpu), "start-powered-off", (i != 0));

        if (i == 0) {
            hexagon_init_bootstrap(machine, cpu);
            cpu_0 = cpu;
        }

        if (!qdev_realize_and_unref(DEVICE(cpu), NULL, errp)) {
            return;
        }
    }

    HexagonCPU *cpu = cpu_0;
    DeviceState *l2vic_dev;
    l2vic_dev = sysbus_create_varargs(
        "l2vic", m_cfg->l2vic_base,
        /* IRQ#, Evnt#,CauseCode */
        qdev_get_gpio_in(DEVICE(cpu), 0), /* IRQ 0, 16, 0xc0 */
        qdev_get_gpio_in(DEVICE(cpu), 1), /* IRQ 1, 17, 0xc1 */
        qdev_get_gpio_in(DEVICE(cpu), 2), /* IRQ 2, 18, 0xc2  VIC0 interface */
        qdev_get_gpio_in(DEVICE(cpu), 3), /* IRQ 3, 19, 0xc3  VIC1 interface */
        qdev_get_gpio_in(DEVICE(cpu), 4), /* IRQ 4, 20, 0xc4  VIC2 interface */
        qdev_get_gpio_in(DEVICE(cpu), 5), /* IRQ 5, 21, 0xc5  VIC3 interface */
        qdev_get_gpio_in(DEVICE(cpu), 6), /* IRQ 6, 22, 0xc6 */
        qdev_get_gpio_in(DEVICE(cpu), 7), /* IRQ 7, 23, 0xc7 */
        NULL);
    sysbus_mmio_map(SYS_BUS_DEVICE(l2vic_dev), 1, m_cfg->cfgtable.fastl2vic_base);

    /* for linux dts you must add 32 to these values */
    pl011_create(0x10000000, qdev_get_gpio_in(l2vic_dev, 15), serial_hd(0));

    /* TODO: We should confine these virtios to `virt` machines. */
    /*
     * TODO: We can/should create an array of these which
     * can be populated at the cmdline.
     */
    sysbus_create_simple("virtio-mmio", 0x11000000,
            qdev_get_gpio_in(l2vic_dev, 18));

    sysbus_create_simple("virtio-mmio", 0x12000000,
            qdev_get_gpio_in(l2vic_dev, 19));

    /*
     * This is tightly with the IRQ selected must match the value below
     * or the interrupts will not be seen
     */
    QCTQtimerState *qtimer = QCT_QTIMER(qdev_new(TYPE_QCT_QTIMER));

    object_property_set_uint(OBJECT(qtimer), "nr_frames",
                                     2, &error_fatal);
    object_property_set_uint(OBJECT(qtimer), "nr_views",
                                     1, &error_fatal);
    object_property_set_uint(OBJECT(qtimer), "cnttid",
                                     0x111, &error_fatal);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(qtimer), &error_fatal);


    unsigned QTMR0_IRQ = /* FIXME syscfg_is_linux */ 2;
    sysbus_mmio_map(SYS_BUS_DEVICE(qtimer), 0,
                    0xfab20000);
    sysbus_mmio_map(SYS_BUS_DEVICE(qtimer), 1, m_cfg->qtmr_rg0);
    sysbus_connect_irq(SYS_BUS_DEVICE(qtimer), 0,
                       qdev_get_gpio_in(l2vic_dev, QTMR0_IRQ));
    sysbus_connect_irq(SYS_BUS_DEVICE(qtimer), 1,
                       qdev_get_gpio_in(l2vic_dev, 4));

    hexagon_config_table *config_table = &m_cfg->cfgtable;

    config_table->l2tcm_base =
        HEXAGON_CFG_ADDR_BASE(m_cfg->cfgtable.l2tcm_base);
    config_table->subsystem_base = HEXAGON_CFG_ADDR_BASE(m_cfg->csr_base);
    config_table->vtcm_base = HEXAGON_CFG_ADDR_BASE(m_cfg->cfgtable.vtcm_base);
    config_table->l2cfg_base =
        HEXAGON_CFG_ADDR_BASE(m_cfg->cfgtable.l2cfg_base);
    config_table->fastl2vic_base =
        HEXAGON_CFG_ADDR_BASE(m_cfg->cfgtable.fastl2vic_base);

    rom_add_blob_fixed_as("config_table.rom", config_table,
                          sizeof(*config_table), m_cfg->cfgbase,
                          &address_space_memory);
}

static void init_mc(MachineClass *mc)
{
    mc->block_default_type = IF_SD;
    mc->default_ram_size = 4 * GiB;
    mc->no_parallel = 1;
    mc->no_floppy = 1;
    mc->no_cdrom = 1;
    mc->no_serial = 1;
    mc->no_sdcard = 1;
    mc->is_default = false;
    mc->max_cpus = 8;
}

/* ----------------------------------------------------------------- */
/* Core-specific configuration settings are defined below this line. */
/* Config table values defined in machine_configs.h.inc              */
/* ----------------------------------------------------------------- */

static void v66g_1024_config_init(MachineState *machine)
{
    hexagon_common_init(machine, v66_rev, &v66g_1024);
}

static void v66g_1024_init(ObjectClass *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);

    mc->desc = "Hexagon V66G_1024";
    mc->init = v66g_1024_config_init;
    init_mc(mc);
    mc->is_default = true;
    mc->default_cpu_type = TYPE_HEXAGON_CPU_V66;
    mc->default_cpus = 4;
}

static const TypeInfo hexagon_machine_types[] = {
    {
        .name = MACHINE_TYPE_NAME("V66G_1024"),
        .parent = TYPE_MACHINE,
        .class_init = v66g_1024_init,
    },
};

DEFINE_TYPES(hexagon_machine_types)
