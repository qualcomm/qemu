/*
 * Hexagon Baseboard System emulation.
 *
 * Copyright (c) 2020 Qualcomm Innovation Center, Inc. All Rights Reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
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
#include "hw/loader.h"
#include "qapi/error.h"
#include "qemu/error-report.h"
#include "qemu/log.h"
#include "elf.h"
#include "cpu.h"
#include "hex_mmu.h"
#include "include/migration/cpu.h"
#include "include/sysemu/sysemu.h"
#include "target/hexagon/internal.h"
#include "libgen.h"
#include "sysemu/reset.h"

#include "machine_configs.h.inc"

static bool syscfg_is_linux;


/* Board init.  */
static struct hexagon_board_boot_info hexagon_binfo;

static hwaddr isdb_secure_flag;
static hwaddr isdb_trusted_flag;
static void hex_symbol_callback(const char *st_name, int st_info,
                                uint64_t st_value, uint64_t st_size) {
    if (!g_strcmp0("isdb_secure_flag", st_name)) {
        isdb_secure_flag = st_value;
    }
    if (!g_strcmp0("isdb_trusted_flag", st_name)) {
        isdb_trusted_flag = st_value;
    }
}


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
        if (isdb_secure_flag || isdb_trusted_flag) {
            /* By convention these flags are at offsets 0x30 and 0x34 */
            uint32_t  mem;
            cpu_physical_memory_read(isdb_secure_flag, &mem, sizeof(mem));
            if (mem == 0x0) {
                mem = 1;
                cpu_physical_memory_write(isdb_secure_flag, &mem, sizeof(mem));
            }
            cpu_physical_memory_read(isdb_trusted_flag, &mem, sizeof(mem));
            if (mem == 0x0) {
                mem = 1;
                cpu_physical_memory_write(isdb_trusted_flag, &mem, sizeof(mem));
            }
        }
    }
}

#define SHMEM_VTCM "hexagon_vtcm"

static void *vtcm_addr;
static uint64_t vtcm_size;
static int memfd_fd = -1;

static void vtcm_exit_handler(void)
{
    if (vtcm_addr) {
        g_free(vtcm_addr);
    }
}

static void *setup_vtcm(uint64_t vtcm_size)
{
    atexit(vtcm_exit_handler);
    return g_malloc0(vtcm_size);
}

static void do_cpu_reset(void *opaque)
{
    HexagonCPU *cpu = opaque;
    CPUState *cs = CPU(cpu);
    cpu_reset(cs);
}

static void hexagon_common_init(MachineState *machine, Rev_t rev,
    hexagon_config_table *cfgTable, hexagon_config_extensions *cfgExtensions)
{
    memset(&hexagon_binfo, 0, sizeof(hexagon_binfo));
    if (machine->kernel_filename) {
        hexagon_binfo.ram_size = machine->ram_size;
        hexagon_binfo.kernel_filename = machine->kernel_filename;
    }

    machine->enable_graphics = 0;

    MemoryRegion *address_space = get_system_memory();

    MemoryRegion *config_table_rom = g_new(MemoryRegion, 1);
    memory_region_init_rom(config_table_rom, NULL, "config_table.rom",
                           sizeof(*cfgTable), &error_fatal);
    memory_region_add_subregion(address_space, cfgExtensions->cfgbase,
                                config_table_rom);

    MemoryRegion *sram = g_new(MemoryRegion, 1);
    memory_region_init_ram(sram, NULL, "lpddr4.ram",
        machine->ram_size, &error_fatal);
    memory_region_add_subregion(address_space, 0x0, sram);

    MemoryRegion *vtcm = g_new(MemoryRegion, 1);
    vtcm_size = cfgTable->vtcm_size_kb * 1024;
    vtcm_addr = setup_vtcm(vtcm_size);

    memory_region_init_ram_ptr(vtcm, NULL, "vtcm.ram", vtcm_size, vtcm_addr);
    memory_region_add_subregion(address_space, cfgTable->vtcm_base, vtcm);

    /* Test region for cpz addresses above 32-bits */
    MemoryRegion *cpz = g_new(MemoryRegion, 1);
    memory_region_init_ram(cpz, NULL, "cpz.ram", 0x10000000, &error_fatal);
    memory_region_add_subregion(address_space, 0x910000000, cpz);

    /* Skip if the core doesn't allocate space for TCM */
    if (cfgExtensions->l2tcm_size) {
        MemoryRegion *tcm = g_new(MemoryRegion, 1);
        memory_region_init_ram(tcm, NULL, "tcm.ram", cfgExtensions->l2tcm_size,
            &error_fatal);
        memory_region_add_subregion(address_space, cfgTable->l2tcm_base, tcm);
    }


    HexagonCPU *cpu_0 = NULL;
    Error **errp = NULL;

    for (int i = 0; i < machine->smp.cpus; i++) {
        HexagonCPU *cpu = HEXAGON_CPU(object_new(machine->cpu_type));
        CPUHexagonState *env = &cpu->env;
        qemu_register_reset(do_cpu_reset, cpu);

        qdev_prop_set_uint32(DEVICE(cpu), "thread-count", machine->smp.cpus);
        qdev_prop_set_uint32(DEVICE(cpu), "config-table-addr",
            cfgExtensions->cfgbase);
        if (cpu->rev_reg == 0) {
            qdev_prop_set_uint32(DEVICE(cpu), "dsp-rev", rev);
        }
        qdev_prop_set_uint32(DEVICE(cpu), "l2vic-base-addr",
            cfgExtensions->l2vic_base);
        qdev_prop_set_uint32(DEVICE(cpu), "qtimer-base-addr",
            cfgExtensions->qtmr_rg0);
        object_property_set_link(OBJECT(cpu), "vtcm", OBJECT(vtcm),
                &error_fatal);
        qdev_prop_set_uint32(DEVICE(cpu), "l2line-size", cfgTable->l2line_size);

        /*
         * CPU #0 is the only CPU running at boot, others must be
         * explicitly enabled via start instruction.
         */
        qdev_prop_set_bit(DEVICE(cpu), "start-powered-off", (i != 0));

        HEX_DEBUG_LOG("%s: first cpu at 0x%p, env %p\n",
                __func__, cpu, env);

        env->vtcm_base = cfgTable->vtcm_base;
        env->vtcm_size = vtcm_size;
        env->memfd_fd = memfd_fd;
        if (i == 0) {
            hexagon_init_bootstrap(machine, cpu);
            cpu_0 = cpu;

            GString *argv = g_string_new(machine->kernel_filename);
            g_string_append(argv, " ");
            g_string_append(argv, machine->kernel_cmdline);
            env->cmdline = g_string_free(argv, false);
            env->dir_list = NULL;
        } else {
            if (cpu_0->usefs) {
                qdev_prop_set_string(DEVICE(cpu), "usefs", cpu_0->usefs);
            }
        }

        if (!qdev_realize_and_unref(DEVICE(cpu), NULL, errp)) {
            return;
        }
    }

    HexagonCPU *cpu = cpu_0;
    DeviceState *l2vic_dev;
    l2vic_dev = sysbus_create_varargs("l2vic", cfgExtensions->l2vic_base,
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
    sysbus_mmio_map(SYS_BUS_DEVICE(l2vic_dev), 1, cfgTable->fastl2vic_base);

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


    unsigned QTMR0_IRQ = syscfg_is_linux ? 2 : 3;
    sysbus_mmio_map(SYS_BUS_DEVICE(qtimer), 0,
                    0xfab20000);
    sysbus_mmio_map(SYS_BUS_DEVICE(qtimer), 1,
                    cfgExtensions->qtmr_rg0);
    sysbus_connect_irq(SYS_BUS_DEVICE(qtimer), 0,
                       qdev_get_gpio_in(l2vic_dev, QTMR0_IRQ));
    sysbus_connect_irq(SYS_BUS_DEVICE(qtimer), 1,
                       qdev_get_gpio_in(l2vic_dev, 4));

    hexagon_config_table *config_table = cfgTable;

    config_table->l2tcm_base = HEXAGON_CFG_ADDR_BASE(cfgTable->l2tcm_base);
    config_table->subsystem_base =
        HEXAGON_CFG_ADDR_BASE(cfgExtensions->csr_base);
    config_table->vtcm_base = HEXAGON_CFG_ADDR_BASE(cfgTable->vtcm_base);
    config_table->l2cfg_base = HEXAGON_CFG_ADDR_BASE(cfgTable->l2cfg_base);
    config_table->fastl2vic_base =
                             HEXAGON_CFG_ADDR_BASE(cfgTable->fastl2vic_base);

    rom_add_blob_fixed_as("config_table.rom", config_table,
        sizeof(*config_table), cfgExtensions->cfgbase, &address_space_memory);
}

/* ----------------------------------------------------------------- */
/* Core-specific configuration settings are defined below this line. */
/* Config table values defined in machine_configs.h.inc              */
/* ----------------------------------------------------------------- */

static void v66g_1024_config_init(MachineState *machine)
{
    hexagon_common_init(machine, v66_rev, &v66g_1024_cfgtable,
        &v66g_1024_extensions);
}

static void v66g_1024_init(ObjectClass *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);

    mc->desc = "Hexagon V66G_1024";
    mc->init = v66g_1024_config_init;
    mc->is_default = false;
    mc->block_default_type = IF_SCSI;
    mc->default_cpu_type = TYPE_HEXAGON_CPU_ANY;
    mc->default_cpus = 4;
    mc->max_cpus = THREADS_MAX;
    mc->default_ram_size = 4 * GiB;
}


static void v66g_1024_linux_init(MachineState *machine)
{
    syscfg_is_linux = true;

    v66g_1024_config_init(machine);
}

static void v66g_linux_init(ObjectClass *oc, void *data)
{
    v66g_1024_init(oc, data);

    MachineClass *mc = MACHINE_CLASS(oc);
    mc->init = v66g_1024_linux_init;
    mc->desc = "Hexagon Linux V66G_1024";
}

static void v68n_1024_config_init(MachineState *machine)

{
    hexagon_common_init(machine, v68_rev, &v68n_1024_cfgtable,
        &v68n_1024_extensions);
}

static void v68n_1024_init(ObjectClass *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);

    mc->desc = "Hexagon V68N_1024";
    mc->init = v68n_1024_config_init;
    mc->is_default = false;
    mc->block_default_type = IF_SCSI;
    mc->default_cpu_type = TYPE_HEXAGON_CPU_ANY;
    mc->default_cpus = 6;
    mc->max_cpus = THREADS_MAX;
    mc->default_ram_size = 4 * GiB;
}

static void v68g_1024_h2_init(MachineState *machine)
{
    syscfg_is_linux = true;
    v68n_1024_config_init(machine);
}

static void v68g_h2_init(ObjectClass *oc, void *data)
{
    v66g_1024_init(oc, data);

    MachineClass *mc = MACHINE_CLASS(oc);
    mc->init = v68g_1024_h2_init;
    mc->desc = "Hexagon H2 V68G_1024";

    mc->default_cpus = 4;
    mc->max_cpus = THREADS_MAX;
}


static void v69na_1024_config_init(MachineState *machine)
{
    hexagon_common_init(machine, v69_rev, &v69na_1024_cfgtable,
        &v69na_1024_extensions);
}

static void v69na_1024_init(ObjectClass *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);

    mc->desc = "Hexagon V69NA_1024";
    mc->init = v69na_1024_config_init;
    mc->is_default = false;
    mc->block_default_type = IF_SCSI;
    mc->default_cpu_type = TYPE_HEXAGON_CPU_ANY;
    mc->default_cpus = 6;
    mc->max_cpus = THREADS_MAX;
    mc->default_ram_size = 4 * GiB;
}

static void v73na_1024_config_init(MachineState *machine)
{
    hexagon_common_init(machine, v73_rev, &v73na_1024_cfgtable,
        &v73na_1024_extensions);
}

static void SA8775P_cdsp0_config_init(MachineState *machine)
{
    hexagon_common_init(machine, v73_rev, &SA8775P_cdsp0_cfgtable,
        &SA8775P_cdsp0_extensions);
}

static void SA8775P_cdsp0_init(ObjectClass *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);

    mc->desc = "SA8775P CDSP0";
    mc->init = SA8775P_cdsp0_config_init;
    mc->is_default = false;
    mc->block_default_type = IF_SCSI;
    mc->default_cpu_type = TYPE_HEXAGON_CPU_ANY;
    mc->default_cpus = 6;
    mc->max_cpus = 6;
    mc->default_ram_size = 4 * GiB;
}

static void SA8540P_cdsp0_config_init(MachineState *machine)
{
    hexagon_common_init(machine, v68_rev, &SA8540P_cdsp0_cfgtable,
        &SA8540P_cdsp0_extensions);
}

static void SA8540P_cdsp0_init(ObjectClass *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);

    mc->desc = "SA8540P CDSP0";
    mc->init = SA8540P_cdsp0_config_init;
    mc->is_default = false;
    mc->block_default_type = IF_SCSI;
    mc->default_cpu_type = TYPE_HEXAGON_CPU_ANY;
    mc->default_cpus = 6;
    mc->max_cpus = 6;
    mc->default_ram_size = 4 * GiB;
}

static void v73na_1024_linux_config_init(MachineState *machine)
{
    syscfg_is_linux = true;

    v73na_1024_config_init(machine);
}

static void v73na_1024_linux_init(ObjectClass *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);

    mc->desc = "Hexagon Linux V73NA_1024";
    mc->init = v73na_1024_linux_config_init;
    mc->is_default = false;
    mc->block_default_type = IF_SCSI;
    mc->default_cpu_type = TYPE_HEXAGON_CPU_ANY;
    mc->default_cpus = 6;
    mc->max_cpus = THREADS_MAX;
    mc->default_ram_size = 4 * GiB;
}

static void v73na_1024_init(ObjectClass *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);

    mc->desc = "Hexagon V73NA_1024";
    mc->init = v73na_1024_config_init;
    mc->is_default = false;
    mc->block_default_type = IF_SCSI;
    mc->default_cpu_type = TYPE_HEXAGON_CPU_ANY;
    mc->default_cpus = 6;
    mc->max_cpus = THREADS_MAX;
    mc->default_ram_size = 4 * GiB;
}

static void virt_init(ObjectClass *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);

    mc->desc = "Hexagon Virt";
    mc->init = v73na_1024_config_init;
    mc->is_default = true;
    mc->block_default_type = IF_SCSI;
    mc->default_cpu_type = TYPE_HEXAGON_CPU_ANY;
    mc->default_cpus = 6;
    mc->max_cpus = THREADS_MAX;
    mc->default_ram_size = 4 * GiB;
}

static const TypeInfo hexagon_machine_types[] = {
    {
        .name = MACHINE_TYPE_NAME("V66G_1024"),
        .parent = TYPE_MACHINE,
        .class_init = v66g_1024_init,
    }, {
        .name = MACHINE_TYPE_NAME("V68N_1024"),
        .parent = TYPE_MACHINE,
        .class_init = v68n_1024_init,
    }, {
        .name = MACHINE_TYPE_NAME("V69NA_1024"),
        .parent = TYPE_MACHINE,
        .class_init = v69na_1024_init,
    }, {
        .name = MACHINE_TYPE_NAME("V73NA_1024"),
        .parent = TYPE_MACHINE,
        .class_init = v73na_1024_init,
    }, {
        .name = MACHINE_TYPE_NAME("V73_Linux"),
        .parent = TYPE_MACHINE,
        .class_init = v73na_1024_linux_init,
    }, {
        .name = MACHINE_TYPE_NAME("V66_Linux"),
        .parent = TYPE_MACHINE,
        .class_init = v66g_linux_init,
    }, {
        .name = MACHINE_TYPE_NAME("V68_H2"),
        .parent = TYPE_MACHINE,
        .class_init = v68g_h2_init,
    }, {
        .name = MACHINE_TYPE_NAME("SA8540P_CDSP0"),
        .parent = TYPE_MACHINE,
        .class_init = SA8540P_cdsp0_init,
    }, {
        .name = MACHINE_TYPE_NAME("SA8775P_CDSP0"),
        .parent = TYPE_MACHINE,
        .class_init = SA8775P_cdsp0_init,
    }, {
        .name = MACHINE_TYPE_NAME("virt"),
        .parent = TYPE_MACHINE,
        .class_init = virt_init,
    },
};

DEFINE_TYPES(hexagon_machine_types)
