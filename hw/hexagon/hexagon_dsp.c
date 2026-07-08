/*
 * Hexagon DSP Subsystem emulation.  This represents a generic DSP
 * subsystem with few peripherals, like the Compute DSP.
 *
 * Copyright (c) 2020-2024 Qualcomm Innovation Center, Inc. All Rights Reserved.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */


#include "qemu/osdep.h"
#include "qemu/units.h"
#include "system/address-spaces.h"
#include "hw/core/boards.h"
#include "hw/core/qdev-properties.h"
#include "hw/hexagon/hexagon.h"
#include "hw/hexagon/hexagon-angel-mbox.h"
#include "hw/hexagon/hexagon_globalreg.h"
#include "hw/hexagon/hexagon_tlb.h"
#include "hw/misc/qcom-hwkm-prng.h"
#include "hw/misc/qcom-turing-cdsp-pll.h"
#include "hw/misc/qcom-qdsp6-cc-regs.h"
#include "hw/misc/qcom-qdsp6-gdscr.h"
#include "hw/misc/qcom-qdsp6-cc-swi.h"
#include "hw/misc/qcom-turing-lmh.h"
#include "hw/misc/qcom-turing-rsc.h"
#include "hw/timer/qct-qtimer.h"
#include "hw/intc/l2vic.h"
#include "hw/char/pl011.h"
#include "hw/core/loader.h"
#include "qapi/error.h"
#include "qemu/error-report.h"
#include "qemu/log.h"
#include "elf.h"
#include "cpu.h"
#include "migration/cpu.h"
#include "system/system.h"
#include "target/hexagon/internal.h"
#include "system/reset.h"
#include "semihosting/semihost.h"
#include "qom/object.h"

#include "machine_cfg_v66g_1024.h.inc"
#include "machine_cfg_v68n_1024.h.inc"
#include "machine_cfg_sa8775_cdsp0.h.inc"
#include "machine_cfg_v81dgb_1.h.inc"
#include "machine_cfg_v81qa_1.h.inc"
#include "hw/hexagon/cmd-db.h"
#include "hw/misc/tcsr.h"
#include "hw/misc/dspss-pub.h"
#include "hw/misc/wdog.h"
#include "hw/misc/qcom-ipcc.h"
#include "hw/misc/unimp.h"
#include "qobject/qlist.h"

static hwaddr isdb_secure_flag;
static hwaddr isdb_trusted_flag;
static void hex_symbol_callback(const char *st_name, int st_info,
                                uint64_t st_value, uint64_t st_size)
{
    if (!g_strcmp0("isdb_secure_flag", st_name)) {
        isdb_secure_flag = st_value;
    }
    if (!g_strcmp0("isdb_trusted_flag", st_name)) {
        isdb_trusted_flag = st_value;
    }
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

static void hexagon_load_bios(HexagonCPU *cpu)
{
    uint64_t pentry;
    long bios_size;

    bios_size = load_elf_ram_sym(hexagon_binfo.bios_filename, NULL, NULL,
                      NULL, &pentry, NULL, NULL,
                      &hexagon_binfo.kernel_elf_flags, 0, EM_HEXAGON, 0, 0,
                      &address_space_memory, false, hex_symbol_callback);

    if (bios_size <= 0) {
        error_report("no bios file '%s'",
            hexagon_binfo.bios_filename);
        exit(1);
    }

    qdev_prop_set_uint32(DEVICE(cpu), "exec-start-addr", pentry);
}

static void hexagon_init_bootstrap(MachineState *machine, HexagonCPU *cpu)
{
    if (machine->firmware) {
        /*
         * -bios: load firmware as the boot image.  When both -bios and
         * -kernel are specified, -bios supplies the bootloader (e.g.
         * runelf.pbn) and -kernel provides the semihosting argv[0]
         * (e.g. the program for the bootloader to exec).
         */
        hexagon_load_bios(cpu);
    } else if (machine->kernel_filename) {
        hexagon_load_kernel(cpu);
    } else {
        return;
    }

    if (isdb_secure_flag || isdb_trusted_flag) {
        /* By convention these flags are at offsets 0x30 and 0x34 */
        uint32_t  mem;
        cpu_physical_memory_read(isdb_secure_flag, &mem, sizeof(mem));
        if (mem == 0x0) {
            mem = cpu_to_le32(1);
            cpu_physical_memory_write(isdb_secure_flag, &mem, sizeof(mem));
        }
        cpu_physical_memory_read(isdb_trusted_flag, &mem, sizeof(mem));
        if (mem == 0x0) {
            mem = cpu_to_le32(1);
            cpu_physical_memory_write(isdb_trusted_flag, &mem, sizeof(mem));
        }
    }
}

static void do_cpu_reset(void *opaque)
{
    HexagonCPU *cpu = opaque;
    CPUState *cs = CPU(cpu);
    cpu_reset(cs);
}

static void create_hwkm_prng(void)
{
    DeviceState *dev = qdev_new(TYPE_HWKM_PRNG);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, HWKM_PRNG_BASE);
}

static void create_turing_lmh(void)
{
    DeviceState *dev = qdev_new(TYPE_TURING_LMH);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, SA8775P_cdsp0.csr_base + TURING_LMH_OFFSET);
}

static void create_cdsp_pll(void)
{
    DeviceState *dev = qdev_new(TYPE_TURING_QDSP6SS_PLL);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);

    /* Map MMIO at absolute base 0x26340000 */
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, SA8775P_cdsp0.csr_base + 0x40000);

}

static void create_cdsp_core0_pll(void)
{
    DeviceState *dev = qdev_new(TYPE_TURING_QDSP6SS_PLL);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);

    /* Map MMIO at absolute base 0x26340000 */
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, 0x26000000);

}

static void create_cdsp_clkctl(void)
{
    DeviceState *dev = qdev_new(TYPE_QDSP6SS_CLKCTL);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);

    /* Map MMIO at absolute base 0x26348000 */
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, SA8775P_cdsp0.csr_base + 0x48000);

}

static void create_cdsp_gdscr(void)
{
    DeviceState *dev = qdev_new(TYPE_QCOM_GDSCR);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);

    /* Map MMIO at absolute base 0x151000 */
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, 0x151000);

}

static void create_cdsp_ccswi(void)
{
    DeviceState *dev = qdev_new(TYPE_CDSP0_CLKCTL);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);

    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, 0x26008000);

}

static void create_cdsp_turing_rsc(void)
{
    DeviceState *dev = qdev_new(TYPE_TURING_RSC);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);

    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, 0x260A4000);

    /* Connect IRQ outputs to L2VIC */
    Object *l2vic_obj = object_resolve_path_type("", TYPE_L2VIC, NULL);
    if (l2vic_obj) {
        DeviceState *l2vic = DEVICE(l2vic_obj);
        /* Connect Turing RSC error IRQ to L2VIC */
        sysbus_connect_irq(SYS_BUS_DEVICE(dev), 0,
                          qdev_get_gpio_in(l2vic, 41));
        /* Connect Turing RSC AMC Mode IRQ to L2VIC */
        sysbus_connect_irq(SYS_BUS_DEVICE(dev), 1,
                          qdev_get_gpio_in(l2vic, 61));
    }
}

static void hexagon_common_init(MachineState *machine, Rev_t rev,
                                hexagon_machine_config *m_cfg,
                                bool start_ticking, uint64_t periodic_ticks)
{
    memset(&hexagon_binfo, 0, sizeof(hexagon_binfo));
    hexagon_binfo.ram_size = machine->ram_size;
    if (machine->kernel_filename) {
        hexagon_binfo.kernel_filename = machine->kernel_filename;
    }
    if (machine->firmware) {
        hexagon_binfo.bios_filename = machine->firmware;
    }

    machine->enable_graphics = 0;

    MemoryRegion *address_space = get_system_memory();

    MemoryRegion *config_table_rom = g_new(MemoryRegion, 1);
    memory_region_init_rom(config_table_rom, NULL, "config_table.rom",
                           sizeof(m_cfg->cfgtable), &error_fatal);
    memory_region_add_subregion(address_space, m_cfg->cfgbase,
                                config_table_rom);

    MemoryRegion *sram = g_new(MemoryRegion, 1);
    memory_region_init_ram(sram, NULL, "ddr.ram",
        machine->ram_size, &error_fatal);
    memory_region_add_subregion(address_space, 0x0, sram);

    uint32_t vtcm_size_bytes = m_cfg->cfgtable.vtcm_size_kb * 1024;
    if (vtcm_size_bytes > 0) {
        MemoryRegion *vtcm = g_new(MemoryRegion, 1);
        memory_region_init_ram(vtcm, NULL, "vtcm.ram",
                               vtcm_size_bytes, &error_fatal);
        memory_region_add_subregion(address_space,
                                    m_cfg->cfgtable.vtcm_base << 16,
                                    vtcm);
    }

    DeviceState *glob_regs_dev = qdev_new(TYPE_HEXAGON_GLOBALREG);
    object_property_add_child(OBJECT(machine), "global-regs",
                              OBJECT(glob_regs_dev));
    qdev_prop_set_uint64(glob_regs_dev, "config-table-addr", m_cfg->cfgbase);

    /* Create TLB object */
    DeviceState *tlb_dev = qdev_new(TYPE_HEXAGON_TLB);
    object_property_add_child(OBJECT(machine), "hexagon-tlb", OBJECT(tlb_dev));
    qdev_prop_set_uint32(tlb_dev, "num-entries",
                         m_cfg->cfgtable.jtlb_size_entries);
    qdev_prop_set_uint32(tlb_dev, "dma-entries",
                         m_cfg->cfgtable.dma_jtlb_entries);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(tlb_dev), &error_fatal);

    HexagonCPU **cpus = g_new(HexagonCPU *, machine->smp.cpus);
    HexagonCPU *cpu0;
    for (int i = 0; i < machine->smp.cpus; i++) {
        HexagonCPU *cpu = HEXAGON_CPU(object_new(machine->cpu_type));
        cpus[i] = cpu;
        qemu_register_reset(do_cpu_reset, cpu);

        /*
         * CPU #0 is the only CPU running at boot, others must be
         * explicitly enabled via start instruction.
         */
        qdev_prop_set_bit(DEVICE(cpu), "start-powered-off", (i != 0));
        qdev_prop_set_uint32(DEVICE(cpu), "dsp-rev", rev);
        qdev_prop_set_uint32(DEVICE(cpu), "hvx-contexts",
                             m_cfg->cfgtable.ext_contexts);
        qdev_prop_set_uint32(DEVICE(cpu), "jtlb-entries",
                             m_cfg->cfgtable.jtlb_size_entries);
        qdev_prop_set_bit(DEVICE(cpu), "sched-limit", true);
        object_property_set_link(OBJECT(cpu), "global-regs",
                                 OBJECT(glob_regs_dev), &error_fatal);
        object_property_set_link(OBJECT(cpu), "tlb",
                                 OBJECT(tlb_dev), &error_fatal);


        if (i == 0) {
            cpu0 = cpu;
            hexagon_init_bootstrap(machine, cpu);
        } else {
            if (cpu0->usefs) {
                qdev_prop_set_string(DEVICE(cpu), "usefs", cpu0->usefs);
            }
        }
    }

    QCTQtimerState *qtimer = QCT_QTIMER(qdev_new(TYPE_QCT_QTIMER));
    object_property_set_uint(OBJECT(qtimer), "nr_frames",
                             3, &error_fatal);
    object_property_set_uint(OBJECT(qtimer), "nr_views",
                             1, &error_fatal);
    object_property_set_uint(OBJECT(qtimer), "cnttid",
                             0x111, &error_fatal);
    object_property_set_bool(OBJECT(qtimer), "start-ticking",
                             start_ticking, &error_fatal);
    object_property_set_uint(OBJECT(qtimer), "periodic-ticks",
                             periodic_ticks, &error_fatal);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(qtimer), &error_fatal);

    /* Link qtimer to globalreg for TIMERLO/TIMERHI reads */
    object_property_set_link(OBJECT(glob_regs_dev), "qtimer",
                             OBJECT(qtimer), &error_fatal);

    DeviceState *l2vic_dev = qdev_new(TYPE_L2VIC);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(l2vic_dev), &error_fatal);
    sysbus_mmio_map(SYS_BUS_DEVICE(l2vic_dev), 0, m_cfg->l2vic_base);

    /* Link the L2VIC interface to globalreg */
    object_property_set_link(OBJECT(glob_regs_dev), "l2vic",
                             OBJECT(l2vic_dev), &error_fatal);

    qdev_prop_set_uint32(glob_regs_dev, "dsp-rev", rev);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(glob_regs_dev), &error_fatal);

    /* Link the L2VIC interface to each CPU */
    for (int i = 0; i < machine->smp.cpus; i++) {
        object_property_set_link(OBJECT(cpus[i]), "l2vic",
                                 OBJECT(l2vic_dev), &error_fatal);
    }

    /*
     * Finally, realize the CPUs
     */
    for (int i = 0; i < machine->smp.cpus; i++) {
        qdev_realize_and_unref(DEVICE(cpus[i]), NULL, &error_fatal);
    }

    /* Connect L2VIC IRQ outputs to CPU inputs after CPU realization */
    HexagonCPU *cpu = cpus[0];
    for (int i = 0; i < 8; i++) {
        sysbus_connect_irq(SYS_BUS_DEVICE(l2vic_dev), i,
                           qdev_get_gpio_in(DEVICE(cpu), i));
    }

    sysbus_mmio_map(SYS_BUS_DEVICE(l2vic_dev), 1,
                     m_cfg->cfgtable.fastl2vic_base << 16);

    pl011_create(0x10000000, qdev_get_gpio_in(l2vic_dev, 15),
                 serial_hd(0));

    sysbus_mmio_map(SYS_BUS_DEVICE(qtimer), 1, m_cfg->qtmr_region);
    sysbus_connect_irq(SYS_BUS_DEVICE(qtimer), 0,
                       qdev_get_gpio_in(l2vic_dev, 3));
    sysbus_connect_irq(SYS_BUS_DEVICE(qtimer), 1,
                       qdev_get_gpio_in(l2vic_dev, 4));

    /* Convert to LE for guest memory */
    hexagon_config_table *guest_config_table = g_new(hexagon_config_table, 1);
    memcpy(guest_config_table, &m_cfg->cfgtable, sizeof(*guest_config_table));
    guest_config_table->subsystem_base =
        HEXAGON_CFG_ADDR_BASE(m_cfg->csr_base);

    for (int i = 0; i < ARRAY_SIZE(guest_config_table->raw); i++) {
        guest_config_table->raw[i] = cpu_to_le32(guest_config_table->raw[i]);
    }

    rom_add_blob_fixed_as("config_table.rom", guest_config_table,
                          sizeof(*guest_config_table), m_cfg->cfgbase,
                          &address_space_memory);
    g_free(guest_config_table);

    g_free(cpus);
}

static void init_mc(MachineClass *mc)
{
    mc->block_default_type = IF_SD;
    mc->default_ram_size = 4 * GiB;
    mc->no_parallel = 1;
    mc->no_floppy = 1;
    mc->no_cdrom = 1;
    mc->no_serial = 1;
    mc->is_default = false;
    mc->max_cpus = 8;
    qemu_semihosting_enable();
}

/* ----------------------------------------------------------------- */
/* Core-specific configuration settings are defined below this line. */
/* Config table values defined in machine_configs.h.inc              */
/* ----------------------------------------------------------------- */

static void v66g_1024_config_init(MachineState *machine)
{
    hexagon_common_init(machine, v66_rev, &v66g_1024, false, 0);
}

static void v66g_1024_init(ObjectClass *oc, const void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);

    mc->desc = "Hexagon V66G_1024";
    mc->init = v66g_1024_config_init;
    init_mc(mc);
    mc->is_default = true;
    mc->default_cpu_type = TYPE_HEXAGON_CPU_V66;
    mc->default_cpus = 4;
}


#include "smem_entries.inc"

static void SA8775P_cdsp0_config_init(MachineState *machine)
{
    hexagon_common_init(machine, v73_rev, &SA8775P_cdsp0, true, 192000);

    /* Create and map the TCSR device */
    DeviceState *tcsr = qdev_new(TYPE_TCSR);
    QList *wonce_init_list = qlist_new();
    qlist_append_int(wonce_init_list, 0x90aff320);
    qdev_prop_set_array(tcsr, "tz-wonce-init", wonce_init_list);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(tcsr), &error_fatal);
    sysbus_mmio_map(SYS_BUS_DEVICE(tcsr), 0, 0x01fc0000);
    uint32_t smem_addr = 0x90AFF320;
    cpu_physical_memory_write(0x01fc0000 + 0x14000, &smem_addr,
                              sizeof(uint32_t));

    /* Create and map the DSPSS-PUB device at CSR base */
    DeviceState *dspss_pub = qdev_new(TYPE_DSPSS_PUB);
    object_property_add_child(OBJECT(machine), "dspss-pub",
                              OBJECT(dspss_pub));
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dspss_pub), &error_fatal);
    sysbus_mmio_map(SYS_BUS_DEVICE(dspss_pub), 0, SA8775P_cdsp0.csr_base);

    /* Create and map the WDOG device at CSR base + 0x84000 */
    DeviceState *wdog = qdev_new(TYPE_WDOG);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(wdog), &error_fatal);
    sysbus_mmio_map(SYS_BUS_DEVICE(wdog), 0,
                    SA8775P_cdsp0.csr_base + 0x84000);

    hwaddr cmd_db_header_addr = 0x0C3F0000;
    hwaddr cmd_db_bin_addr = 0x80860000;
    hexagon_load_cmd_db(cmd_db_header_addr, cmd_db_bin_addr);

    cpu_physical_memory_write(0x90900000, sa8775p_smem_data,
                              sizeof(sa8775p_smem_data));

    create_unimplemented_device("cxstmtrace", 0x16000000, 0x1000);
    create_unimplemented_device("cxstmcfg", 0x10002000, 0x1000);
    create_unimplemented_device("cxetb", 0x11305000, 0x1000);
    create_unimplemented_device("tpdm-0", 0x11181000, 0x1000);
    create_unimplemented_device("tpdm-1", 0x11182000, 0x1000);
    create_unimplemented_device("tpdm-2", 0x11185000, 0x1000);
    create_unimplemented_device("tpdm-3", 0x11186000, 0x1000);
    create_unimplemented_device("funnel-0", 0x10041000, 0x1000);
    create_unimplemented_device("funnel-1", 0x11304000, 0x1000);
    create_unimplemented_device("tpda", 0x11188000, 0x1000);

    /* IPCC (Inter-Processor Communication Controller) */
    DeviceState *ipcc = qdev_new(TYPE_QCOM_IPCC);
    object_property_add_child(OBJECT(machine), "ipcc", OBJECT(ipcc));
    sysbus_realize_and_unref(SYS_BUS_DEVICE(ipcc), &error_fatal);
    sysbus_mmio_map(SYS_BUS_DEVICE(ipcc), 0, 0x00400000);

    Object *l2vic_obj = object_resolve_path_type("", TYPE_L2VIC, NULL);
    if (l2vic_obj) {
        DeviceState *l2vic = DEVICE(l2vic_obj);
        sysbus_connect_irq(SYS_BUS_DEVICE(ipcc), 6,
                           qdev_get_gpio_in(l2vic, 30));
    }

    create_hwkm_prng();
    create_cdsp_pll();
    create_cdsp_core0_pll();
    create_cdsp_clkctl();
    create_cdsp_gdscr();
    create_cdsp_ccswi();
    create_turing_lmh();
    create_cdsp_turing_rsc();
    /*
     * The OS running on the DSP expects the timer to be running and
     * pre-configured by the hypervisor.  Program an initial TVAL so the
     * first timer interrupt fires shortly after boot.  The periodic-ticks
     * property on the qtimer device handles auto-reload.
     * 19200 ticks = 1ms at 19.2 MHz.
     */
    uint32_t val;
    val = QCT_QTIMER_CNTP_CTL_ENABLE | QCT_QTIMER_CNTP_CTL_INTEN;
    cpu_physical_memory_write(SA8775P_cdsp0.qtmr_region +
                              QCT_QTIMER_CNTP_CTL, &val, sizeof(val));
    val = 19200;
    cpu_physical_memory_write(SA8775P_cdsp0.qtmr_region +
                              QCT_QTIMER_CNTP_TVAL, &val, sizeof(val));

    /* Set Default values for some Read-Only RPMH_PDC_COMPUTE registers. */
    uint32_t default_value = 0x20600;
    cpu_physical_memory_write(0xB2C1000, &default_value, sizeof(uint32_t));
    default_value = 0x5381;
    cpu_physical_memory_write(0xB2C1004, &default_value, sizeof(uint32_t));
    default_value = 0x180411;
    cpu_physical_memory_write(0xB2C1008, &default_value, sizeof(uint32_t));
    default_value = 0xa600a;
    cpu_physical_memory_write(0xB2C100c, &default_value, sizeof(uint32_t));
}

static void SA8775P_cdsp0_init(ObjectClass *oc, const void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);

    mc->desc = "SA8775P CDSP0";
    mc->init = SA8775P_cdsp0_config_init;
    init_mc(mc);
    mc->default_cpu_type = TYPE_HEXAGON_CPU_V73;
    mc->default_cpus = 6;
    mc->max_cpus = 6;
}

static void sim_config_init(MachineState *machine)
{
    hexagon_common_init(machine, v68_rev, &v68n_1024, false, 0);

    /*
     * Unblock H2 hypervisor "booter"'s angel semihosting mailbox poll
     * loop, which otherwise spins forever since QEMU does not implement
     * the mailbox protocol.  See docs/system/hexagon/booter.rst.
     *
     * booter's boot-time TLB setup (roms/hexagon-hypervisor's
     * kernel/init/boot/boot.ref.S) installs a 4K entry for ANGEL_VA
     * (0xffd00000) mapped to physical address 0, nested inside a
     * coarser 4M entry for Q6_SS_BASE_VA (0xffc00000) that maps to
     * csr_base.  On real hardware and hexagon-sim, the more specific
     * entry wins, but QEMU's TLB lookup (hexagon_tlb_find_match())
     * matches the first (coarser) entry instead, so accesses to
     * ANGEL_VA actually resolve to csr_base's offset within that 4M
     * window rather than physical address 0.  Map the mailbox device
     * there instead of at 0 so it is reached in practice.
     */
    DeviceState *angel_mbox_dev = qdev_new(TYPE_HEXAGON_ANGEL_MBOX);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(angel_mbox_dev), &error_fatal);
    sysbus_mmio_map_overlap(SYS_BUS_DEVICE(angel_mbox_dev), 0,
                            v68n_1024.csr_base, 1);
}

static void sim_init(ObjectClass *oc, const void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);

    mc->desc = "Hexagon Sim-like (v68)";
    mc->init = sim_config_init;
    init_mc(mc);
    mc->default_cpu_type = TYPE_HEXAGON_CPU_V68;
    mc->default_cpus = 6;
    mc->max_cpus = THREADS_MAX;
}

static void v81dgb_1_config_init(MachineState *machine)
{
    hexagon_common_init(machine, v81dgb_1_rev, &v81dgb_1, false, 0);
}

static void v81dgb_1_init(ObjectClass *oc, const void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);

    mc->desc = "Hexagon V81DGB_1";
    mc->init = v81dgb_1_config_init;
    mc->is_default = false;
    mc->default_cpu_type = TYPE_HEXAGON_CPU_ANY;
    mc->default_cpus = 12;
    mc->max_cpus = THREADS_MAX;
    mc->default_ram_size = 4 * GiB;
    qemu_semihosting_enable();
}

static void v81qa_1_config_init(MachineState *machine)
{
    hexagon_common_init(machine, v81_rev, &v81qa_1, false, 0);
}

static void v81qa_1_init(ObjectClass *oc, const void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);

    mc->desc = "Hexagon V81QA_1";
    mc->init = v81qa_1_config_init;
    mc->is_default = false;
    mc->default_cpu_type = TYPE_HEXAGON_CPU_ANY;
    mc->default_cpus = 12;
    mc->max_cpus = THREADS_MAX;
    mc->default_ram_size = 4 * GiB;
    qemu_semihosting_enable();
}

static const TypeInfo hexagon_machine_types[] = {
    {
        .name = MACHINE_TYPE_NAME("V66G_1024"),
        .parent = TYPE_MACHINE,
        .class_init = v66g_1024_init,
    },
    {
        .name = MACHINE_TYPE_NAME("SA8775P_CDSP0"),
        .parent = TYPE_MACHINE,
        .class_init = SA8775P_cdsp0_init,
    },
    {
        .name = MACHINE_TYPE_NAME("sim"),
        .parent = TYPE_MACHINE,
        .class_init = sim_init,
    },
    {
        .name = MACHINE_TYPE_NAME("V81DGB_1"),
        .parent = TYPE_MACHINE,
        .class_init = v81dgb_1_init,
    },
    {
        .name = MACHINE_TYPE_NAME("V81QA_1"),
        .parent = TYPE_MACHINE,
        .class_init = v81qa_1_init,
    },
};

DEFINE_TYPES(hexagon_machine_types)
