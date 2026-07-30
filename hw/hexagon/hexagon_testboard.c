/*
 * Hexagon Baseboard System emulation.
 *
 * Copyright (c) 2020-2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "qemu/units.h"
#include "system/address-spaces.h"
#include "hw/core/hw-error.h"
#include "hw/core/boards.h"
#include "hw/core/qdev-properties.h"
#include "hw/hexagon/hexagon.h"
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
#include "hex_mmu.h"
#include "opcodes.h"
#include "include/migration/cpu.h"
#include "include/system/system.h"
#include "target/hexagon/internal.h"
#include "target/hexagon/macros.h"
#include "libgen.h"
#include "system/reset.h"
#include "system/qtest.h"
#include "semihosting/semihost.h"
#include "hw/misc/qcom-ipcc.h"
#include "qom/object.h"

#include "machine_configs.h.inc"
#include "qemu/qemu-print.h"
#include "coproc.h"
#include "hw/hexagon/cmd-db.h"
#include "hw/misc/tcsr.h"
#include "hw/misc/dspss-pub.h"
#include "qobject/qlist.h"
#include "hw/misc/wdog.h"
#include "hw/misc/unimp.h"

static bool syscfg_is_linux;

static struct hexagon_board_boot_info hexagon_binfo;

static hwaddr isdb_secure_flag;
static hwaddr isdb_trusted_flag;

static void *vtcm_addr;
static GString *shm_name;

#define SHM_INVALID -1
static int shm_fd = SHM_INVALID;

#ifdef _WIN32
static HANDLE file_mapping;
#endif

#define DEFAULT_SUBSYSTEM_ID 0

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

static Rev_t rev_from_rev_byte(int byte)
{
    switch (byte) {
    case 0x65:
        warn_report("binary arch revision is too old (v%02x), using v66 instead",
                    byte);
        /* fallthrough */
    case 0x66: return v66_rev;
    case 0x67: return v67_rev;
    case 0x68: return v68_rev;
    case 0x69: return v69_rev;
    case 0x71: return v71_rev;
    case 0x73: return v73_rev;
    case 0x75: return v75_rev;
    case 0x77: return v75_rev; /* v77 is identical to v75 */
    case 0x79: return v79_rev;
    case 0x81: return v81_rev;
    default: return unknown_rev;
    }
}

static void hexagon_load_kernel(HexagonCPU *cpu, Rev_t *rev)
{
    uint64_t pentry;
    long kernel_size;
    int elf_rev_byte, rev_byte;

    kernel_size = load_elf_ram_sym(hexagon_binfo.kernel_filename, NULL, NULL,
                      NULL, &pentry, NULL, NULL,
                      &hexagon_binfo.kernel_elf_flags, 0, EM_HEXAGON, 0, 0,
                      &address_space_memory, false, hex_symbol_callback);

    if (kernel_size <= 0) {
        error_report("no kernel file '%s'",
            hexagon_binfo.kernel_filename);
        exit(1);
    }

    rev_byte = *rev & 0xff;
    elf_rev_byte = hexagon_binfo.kernel_elf_flags & 0xff;

    if (*rev == unknown_rev) {
        *rev = rev_from_rev_byte(elf_rev_byte);
        if (*rev == unknown_rev) {
            error_report("could not identify binary revision: 0x%02x",
                         elf_rev_byte);
            exit(1);
        }
    } else if (rev_byte != elf_rev_byte) {
        warn_report("using v%02x cpu but binary is for v%02x",
                     rev_byte, elf_rev_byte);
    }

    qdev_prop_set_uint32(DEVICE(cpu->globalregs), "boot-evb", pentry);
}

static void hexagon_init_bootstrap(MachineState *machine, HexagonCPU *cpu,
                                   Rev_t *rev)
{
    if (machine->kernel_filename) {
        hexagon_load_kernel(cpu, rev);
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
    } else if (!cpu->vp_mode && !qtest_enabled()) {
        error_report("kernel image must be given with -kernel");
        exit(1);
    } else {
        *rev = glue(HEXAGON_LATEST_REV, _rev);
    }
}

/*
 * In QQVP mode num is the subsystem id (currently either 0 or 1)
 *   NSP0's vtcm would be /vtcm_0-###
 *   NSP1's vtcm would be /vtcm_1-###
 *
 * In standalone QEMU mode it is always 0.
 */
static void vtcm_exit_handler(void)
{
    if (vtcm_addr) {
        if (SHM_INVALID == shm_fd) {
            /* num_coproc_instance must have been 0 */
            g_free(vtcm_addr);
            return;
        }
#if defined(__unix__) || defined(__APPLE__)
        if (shm_name) {
            shm_unlink(shm_name->str);
            close(shm_fd);
            g_string_free(shm_name, TRUE);
        }
#elif _WIN32
        if (file_mapping) {
            UnmapViewOfFile(vtcm_addr);
            CloseHandle(file_mapping);
            g_string_free(shm_name, TRUE);
        }
#endif
    }
}

static void *malloc_shared(uint32_t vtcm_size_bytes, uint32_t subsystem_id)
{
    shm_name = g_string_new(NULL);

#if defined(__unix__) || defined(__APPLE__)
    g_string_printf(shm_name, "/vtcm_%d-%x", subsystem_id, getpid());

    shm_fd = shm_open(shm_name->str, O_CREAT | O_EXCL | O_RDWR,
                      S_IRUSR | S_IWUSR);
    if (SHM_INVALID == shm_fd) {
        hw_error("qemu: shm_open failed:%s:%s\n", strerror(errno),
                 shm_name->str);
        g_string_free(shm_name, TRUE);
        exit(1);
    }

    if (ftruncate(shm_fd, vtcm_size_bytes) == -1) {
        hw_error("qemu: ftruncate failed:%s:%s\n", strerror(errno),
                 shm_name->str);
        shm_unlink(shm_name->str);
        close(shm_fd);
        g_string_free(shm_name, TRUE);
        exit(1);
    }

    void *addr = (void *)mmap(0, vtcm_size_bytes, PROT_READ | PROT_WRITE,
                              MAP_SHARED, shm_fd, 0);
    if (addr == MAP_FAILED) {
        hw_error("qemu: mmap failed : %s:%s\n", strerror(errno), shm_name->str);
        shm_unlink(shm_name->str);
        close(shm_fd);
        g_string_free(shm_name, TRUE);
        exit(1);
    }
#elif _WIN32
    g_string_printf(shm_name, "Local\\vtcm_%d-%lx", subsystem_id,
                    GetCurrentProcessId());

    file_mapping = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL,
                                      PAGE_READWRITE, 0, vtcm_size_bytes,
                                      shm_name->str);
    if (NULL == file_mapping) {
        hw_error("qemu: CreateFileMapping failed: %lu\n", GetLastError());
        g_string_free(shm_name, TRUE);
        exit(1);
    }

    shm_fd = (int)(uintptr_t)file_mapping;

    void *addr = MapViewOfFile(file_mapping, FILE_MAP_ALL_ACCESS, 0, 0,
                               vtcm_size_bytes);
    if (NULL == addr) {
        hw_error("qemu: MapViewOfFile failed: %lu\n", GetLastError());
        CloseHandle(file_mapping);
        g_string_free(shm_name, TRUE);
        exit(1);
    }
#endif

    return addr;
}

/**
 * Setup vector tightly coupled memory (VTCM)
 *
 * Sets up VTCM with regular memory if no coproc is available and with shared
 * memory otherwise.
 *
 * @param[in] vtcm_size_bytes Size of vtcm memory to be allocated
 * @param[in] shared Use shared memory for VTCM
 */
static void *setup_vtcm(uint32_t vtcm_size_bytes, bool shared,
                        uint32_t subsystem_id)
{
    void *addr = NULL;

    if (!shared) {
        addr = g_malloc0(vtcm_size_bytes);
        shm_fd = SHM_INVALID;
    } else {
        addr = malloc_shared(vtcm_size_bytes, subsystem_id);
    }

    atexit(vtcm_exit_handler);

    return addr;
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

    qdev_prop_set_uint32(dev, "rsc-id-reset", 0x00020400);
    qdev_prop_set_uint32(dev, "solver-config-reset", 0x00010100);
    qdev_prop_set_uint32(dev, "rsc-config-reset", 0x01300214);
    qdev_prop_set_uint32(dev, "parentchild-config-reset", 0x8000000AU);
    qdev_prop_set_uint32(dev, "cmd-spacing", 0x14);
    qdev_prop_set_uint32(dev, "tcs-base-offset", 0x10);
    qdev_prop_set_uint32(dev, "cmd-base-in-tcs", 0x14);
    qdev_prop_set_uint32(dev, "tcs-timeout-base", 0x3D44);
    qdev_prop_set_uint32(dev, "timeout-clr-offset", 0x04);
    qdev_prop_set_uint32(dev, "timeout-status-offset", 0x08);
    qdev_prop_set_uint32(dev, "timeout-val-offset", 0x0C);
    qdev_prop_set_uint32(dev, "num-br-addr", 4);
    qdev_prop_set_uint32(dev, "num-timestamp-units", 6);

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
                                hexagon_machine_config *m_cfg)
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
                           sizeof(m_cfg->cfgtable), &error_fatal);
    memory_region_add_subregion(address_space, m_cfg->cfgbase,
                                config_table_rom);

    MemoryRegion *sram = g_new(MemoryRegion, 1);
    memory_region_init_ram(sram, NULL, "lpddr4.ram",
        machine->ram_size, &error_fatal);
    memory_region_add_subregion(address_space, 0x0, sram);

    uint32_t vtcm_size_bytes = m_cfg->cfgtable.vtcm_size_kb * 1024;
    if (vtcm_size_bytes > 0) {
        MemoryRegion *vtcm = g_new(MemoryRegion, 1);

        vtcm_addr = setup_vtcm(vtcm_size_bytes,
                               (m_cfg->cfgtable.coproc2_reg0) ? 1 : 0,
                               DEFAULT_SUBSYSTEM_ID);
        memory_region_init_ram_ptr(vtcm, NULL, "vtcm.ram", vtcm_size_bytes,
                                   vtcm_addr);
        memory_region_add_subregion(address_space,
                                    m_cfg->cfgtable.vtcm_base << 16,
                                    vtcm);
    }

#ifndef _WIN32
    /* Test region for cpz addresses above 32-bits */
    MemoryRegion *cpz = g_new(MemoryRegion, 1);
    memory_region_init_ram(cpz, NULL, "cpz.ram", 0x10000000, &error_fatal);
    memory_region_add_subregion(address_space, 0x910000000, cpz);
#endif

    /* Skip if the core doesn't allocate space for TCM */
    if (m_cfg->l2tcm_size) {
        MemoryRegion *tcm = g_new(MemoryRegion, 1);
        memory_region_init_ram(tcm, NULL, "tcm.ram", m_cfg->l2tcm_size,
                               &error_fatal);
        memory_region_add_subregion(address_space,
                                    m_cfg->cfgtable.l2tcm_base << 16,
                                    tcm);
    }

    HexagonCPU **cpus = g_malloc_n(machine->smp.cpus, sizeof(HexagonCPU *));
    Error **errp = NULL;

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

    sysbus_realize_and_unref(SYS_BUS_DEVICE(tlb_dev), errp);

    /* Now create CPUs. */
    for (int i = 0; i < machine->smp.cpus; i++) {
        HexagonCPU *cpu = HEXAGON_CPU(object_new(machine->cpu_type));
        cpus[i] = cpu;
        qemu_register_reset(do_cpu_reset, cpu);

        qdev_prop_set_uint32(DEVICE(cpu), "thread-count", machine->smp.cpus);
        qdev_prop_set_uint32(DEVICE(cpu), "vtcm-base-addr",
                             m_cfg->cfgtable.vtcm_base << 16);
        qdev_prop_set_uint32(DEVICE(cpu), "vtcm-size-kb",
                             m_cfg->cfgtable.vtcm_size_kb);
        qdev_prop_set_uint32(DEVICE(cpu), "l2line-size",
                             m_cfg->cfgtable.l2line_size);

        /*
         * CPU #0 is the only CPU running at boot, others must be
         * explicitly enabled via start instruction.
         */
        qdev_prop_set_bit(DEVICE(cpu), "start-powered-off", (i != 0));
        qdev_prop_set_uint32(DEVICE(cpu), "num-coproc-instance",
                             (m_cfg->cfgtable.coproc2_reg0) ? 1 : 0);
        qdev_prop_set_uint32(DEVICE(cpu), "hvx-contexts",
                             m_cfg->cfgtable.ext_contexts);
        qdev_prop_set_bit(DEVICE(cpu), "coproc2-bfloat",
                             (m_cfg->cfgtable.coproc2_fp16_acc_exp >> 0) & 1);
        qdev_prop_set_bit(DEVICE(cpu), "hvx-bfloat",
                             (m_cfg->cfgtable.coproc2_fp16_acc_exp >> 1) & 1);
        if (!object_property_set_link(OBJECT(cpu), "global-regs",
                                      OBJECT(glob_regs_dev), errp)) {
            error_report("Failed to link global system registers to CPU %d", i);
            goto out;
        }

        if (i == 0) {
            if (cpu->rev_reg) {
                rev = cpu->rev_reg;
            }
            hexagon_init_bootstrap(machine, cpu, &rev);
        } else {
            if (cpus[0]->usefs) {
                qdev_prop_set_string(DEVICE(cpu), "usefs", cpus[0]->usefs);
            }
        }

        qdev_prop_set_uint32(DEVICE(cpu), "dsp-rev", rev);
    }

    QCTQtimerState *qtimer = QCT_QTIMER(qdev_new(TYPE_QCT_QTIMER));

    object_property_set_uint(OBJECT(qtimer), "nr_frames", 3, &error_fatal);
    object_property_set_uint(OBJECT(qtimer), "nr_views", 1, &error_fatal);
    object_property_set_uint(OBJECT(qtimer), "cnttid_0", 0x111, &error_fatal);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(qtimer), &error_fatal);

    if (!object_property_set_link(OBJECT(glob_regs_dev), "qtimer",
                                  OBJECT(qtimer), errp)) {
        error_report("Failed to link qtimer interface to global registers");
        goto out;
    }

    DeviceState *l2vic_dev = qdev_new(TYPE_L2VIC);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(l2vic_dev), &error_fatal);
    sysbus_mmio_map(SYS_BUS_DEVICE(l2vic_dev), 0, m_cfg->l2vic_base);

    /* Link the L2VIC interface to globalreg */
    if (!object_property_set_link(OBJECT(glob_regs_dev), "l2vic",
                                  OBJECT(l2vic_dev), errp)) {
        error_report("Failed to link L2VIC interface to global registers");
        goto out;
    }

    qdev_prop_set_uint32(glob_regs_dev, "dsp-rev", rev);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(glob_regs_dev), errp);

    /*
     * Finally, link cpus to global registers, L2VIC interface, and do
     * realization
     */
    for (int i = 0; i < machine->smp.cpus; i++) {
        if (!object_property_set_link(OBJECT(cpus[i]), "tlb",
                                      OBJECT(tlb_dev), errp)) {
            error_report("Failed to link TLB to CPU %d", i);
            goto out;
        }
        if (!object_property_set_link(OBJECT(cpus[i]), "l2vic",
                                      OBJECT(l2vic_dev), errp)) {
            error_report("Failed to link L2VIC interface to CPU %d", i);
            goto out;
        }
        if (!qdev_realize_and_unref(DEVICE(cpus[i]), NULL, errp)) {
            error_report("Failed to realize CPU %d", i);
            goto out;
        }
    }

    /* Connect L2VIC IRQ outputs to CPU inputs after CPU realization */
    HexagonCPU *cpu = cpus[0];
    for (int i = 0; i < 8; i++) {
        sysbus_connect_irq(SYS_BUS_DEVICE(l2vic_dev), i,
                           qdev_get_gpio_in(DEVICE(cpu), i));
    }

    sysbus_mmio_map(SYS_BUS_DEVICE(l2vic_dev), 1,
                     m_cfg->cfgtable.fastl2vic_base << 16);

    /* for linux dts you must add 32 to these values */
    pl011_create(0x10000000, qdev_get_gpio_in(l2vic_dev, 15), serial_hd(0));

    unsigned QTMR0_IRQ = syscfg_is_linux ? 2 : 3;
    sysbus_mmio_map(SYS_BUS_DEVICE(qtimer), 1, m_cfg->qtmr_region);
    sysbus_connect_irq(SYS_BUS_DEVICE(qtimer), 0,
                       qdev_get_gpio_in(l2vic_dev, QTMR0_IRQ));
    sysbus_connect_irq(SYS_BUS_DEVICE(qtimer), 1,
                       qdev_get_gpio_in(l2vic_dev, 4));

    hexagon_config_table *config_table = &m_cfg->cfgtable;

    config_table->subsystem_base = HEXAGON_CFG_ADDR_BASE(m_cfg->csr_base);

    /* Convert to LE for guest memory */
    hexagon_config_table *guest_config_table = g_malloc(sizeof(*config_table));
    memcpy(guest_config_table, config_table, sizeof(*config_table));

    for (int i = 0; i < ARRAY_SIZE(guest_config_table->raw); i++) {
        guest_config_table->raw[i] = cpu_to_le32(guest_config_table->raw[i]);
    }

    rom_add_blob_fixed_as("config_table.rom", guest_config_table,
                          sizeof(*guest_config_table), m_cfg->cfgbase,
                          &address_space_memory);
    g_free(guest_config_table);
out:
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
    mc->max_cpus = THREADS_MAX;
    qemu_semihosting_enable();
}

static void machcfg_disable_coproc(hexagon_machine_config *cfg)
{
    cfg->cfgtable.coproc2_reg0 = 0;
    cfg->cfgtable.coproc2_reg1 = 0;
    cfg->cfgtable.coproc2_reg2 = 0;
    cfg->cfgtable.coproc2_reg3 = 0;
    cfg->cfgtable.coproc2_reg4 = 0;
    cfg->cfgtable.coproc2_reg5 = 0;
    cfg->cfgtable.coproc2_reg6 = 0;
    cfg->cfgtable.coproc2_reg7 = 0;
    cfg->cfgtable.coproc2_cvt_mpy_size = 0;
}

/* ----------------------------------------------------------------- */
/* Core-specific configuration settings are defined below this line. */
/* Config table values defined in machine_configs.h.inc              */
/* ----------------------------------------------------------------- */

static void v66g_1024_config_init(MachineState *machine)
{
    hexagon_common_init(machine, v66_rev, &v66g_1024);
}

static void v66g_1024_init(ObjectClass *oc, const void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);

    mc->desc = "Hexagon V66G_1024";
    mc->init = v66g_1024_config_init;
    init_mc(mc);
    mc->default_cpu_type = TYPE_HEXAGON_CPU_V66;
    mc->default_cpus = 4;
}


static void v66g_1024_linux_init(MachineState *machine)
{
    syscfg_is_linux = true;

    v66g_1024_config_init(machine);
}

static void v66g_linux_init(ObjectClass *oc, const void *data)
{
    v66g_1024_init(oc, data);

    MachineClass *mc = MACHINE_CLASS(oc);
    mc->init = v66g_1024_linux_init;
    mc->desc = "Hexagon Linux V66G_1024";
}

static void v68n_1024_config_init(MachineState *machine)

{
    hexagon_common_init(machine, v68_rev, &v68n_1024);
}

static void v68n_1024_init(ObjectClass *oc, const void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);

    mc->desc = "Hexagon V68N_1024";
    mc->init = v68n_1024_config_init;
    init_mc(mc);
    mc->default_cpu_type = TYPE_HEXAGON_CPU_V68;
    mc->default_cpus = 6;
}

static void v68g_1024_h2_init(MachineState *machine)
{
    syscfg_is_linux = true;
    v68n_1024_config_init(machine);
}

static void v68n_h2_init(ObjectClass *oc, const void *data)
{
    v68n_1024_init(oc, data);

    MachineClass *mc = MACHINE_CLASS(oc);
    mc->init = v68g_1024_h2_init;
    mc->desc = "Hexagon H2 V68G_1024";
    init_mc(mc);

    mc->default_cpus = 6;
}


static void v69na_1024_config_init(MachineState *machine)
{
    hexagon_common_init(machine, v69_rev, &v69na_1024);
}

static void v69na_1024_init(ObjectClass *oc, const void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);

    mc->desc = "Hexagon V69NA_1024";
    mc->init = v69na_1024_config_init;
    init_mc(mc);
    mc->default_cpu_type = TYPE_HEXAGON_CPU_V69;
    mc->default_cpus = 6;
}

static void v73na_1024_config_init(MachineState *machine)
{
    hexagon_common_init(machine, v73_rev, &v73na_1024);
}

static void v73m_config_init(MachineState *machine)
{
    hexagon_common_init(machine, v73m_rev, &v73m);
}

#include "smem_entries.inc"

static void SA8775P_cdsp0_config_init(MachineState *machine)
{
    hexagon_common_init(machine, v73_rev, &SA8775P_cdsp0);

    /* Create and map the TCSR device */
    DeviceState *tcsr = qdev_new(TYPE_TCSR);
    /* Set the first WONCE register to 0x90aff320 */
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
    object_property_add_child(OBJECT(machine), "dspss-pub", OBJECT(dspss_pub));
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dspss_pub), &error_fatal);
    sysbus_mmio_map(SYS_BUS_DEVICE(dspss_pub), 0, SA8775P_cdsp0.csr_base);

    /* Create and map the WDOG device at CSR base + 0x84000 */
    DeviceState *wdog = qdev_new(TYPE_WDOG);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(wdog), &error_fatal);
    sysbus_mmio_map(SYS_BUS_DEVICE(wdog), 0, SA8775P_cdsp0.csr_base + 0x84000);

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

static void SA8540P_cdsp0_config_init(MachineState *machine)
{
    hexagon_common_init(machine, v68_rev, &SA8540P_cdsp0);
}

static void SA8540P_cdsp0_init(ObjectClass *oc, const void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);

    mc->desc = "SA8540P CDSP0";
    mc->init = SA8540P_cdsp0_config_init;
    init_mc(mc);
    mc->default_cpu_type = TYPE_HEXAGON_CPU_V69;
    mc->default_cpus = 6;
    mc->max_cpus = 6;
}

static void SA8797P_nsp0_config_init(MachineState *machine)
{
    hexagon_common_init(machine, v81_rev, &SA8797P_nsp0);
}

static void SA8797P_nsp0_init(ObjectClass *oc, const void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);

    init_mc(mc);
    mc->desc = "SA8797P NSP0";
    mc->init = SA8797P_nsp0_config_init;
    mc->is_default = false;
    mc->default_cpu_type = TYPE_HEXAGON_CPU_V81;
    mc->default_cpus = 12;
    mc->max_cpus = 12;
}

static void v73na_1024_linux_config_init(MachineState *machine)
{
    syscfg_is_linux = true;

    v73na_1024_config_init(machine);
}

static void v73na_1024_linux_init(ObjectClass *oc, const void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);

    mc->desc = "Hexagon Linux V73NA_1024";
    mc->init = v73na_1024_linux_config_init;
    init_mc(mc);
    mc->default_cpu_type = TYPE_HEXAGON_CPU_V73;
    mc->default_cpus = 6;
}

static void v73na_1024_init(ObjectClass *oc, const void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);

    mc->desc = "Hexagon V73NA_1024";
    mc->init = v73na_1024_config_init;
    init_mc(mc);
    mc->default_cpu_type = TYPE_HEXAGON_CPU_V73;
    mc->default_cpus = 6;
}

static void v73m_init(ObjectClass *oc, const void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);

    mc->desc = "Hexagon V73M";
    mc->init = v73m_config_init;
    init_mc(mc);
    mc->default_cpu_type = TYPE_HEXAGON_CPU_V73;
    mc->default_cpus = 4;
}

static void v75na_1024_config_init(MachineState *machine)
{
    hexagon_common_init(machine, v75_rev, &v75na_1024);
}

static void sim_nocoproc_config_init(MachineState *machine)
{
    hexagon_machine_config v81dgb_1_nocoproc;
    memcpy(&v81dgb_1_nocoproc, &v81dgb_1, sizeof(v81dgb_1));
    machcfg_disable_coproc(&v81dgb_1_nocoproc);
    hexagon_common_init(machine, unknown_rev, &v81dgb_1_nocoproc);
}

static void sim_coproc_config_init(MachineState *machine)
{
    hexagon_common_init(machine, unknown_rev, &v75na_1024);
}

static void v75na_1024_linux_config_init(MachineState *machine)
{
    syscfg_is_linux = true;

    v75na_1024_config_init(machine);
}

static void v75na_1024_linux_init(ObjectClass *oc, const void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);

    mc->desc = "Hexagon Linux V75NA_1024";
    mc->init = v75na_1024_linux_config_init;
    init_mc(mc);
    mc->default_cpu_type = TYPE_HEXAGON_CPU_V75;
    mc->default_cpus = 6;
}

static void v75na_1024_init(ObjectClass *oc, const void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);

    mc->desc = "Hexagon V75NA_1024";
    mc->init = v75na_1024_config_init;
    init_mc(mc);
    mc->default_cpu_type = TYPE_HEXAGON_CPU_V75;
    mc->default_cpus = 6;
}

static void v79na_1_config_init(MachineState *machine)
{
    hexagon_common_init(machine, v79_rev, &v79na_1);
}

static void v79na_1_linux_config_init(MachineState *machine)
{
    syscfg_is_linux = true;

    v79na_1_config_init(machine);
}

static void v79m_1_config_init(MachineState *machine)
{
    hexagon_common_init(machine, v79m_1_rev, &v79m_1);
}

static void v79na_1_linux_init(ObjectClass *oc, const void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);

    mc->desc = "Hexagon Linux V79NA_1";
    mc->init = v79na_1_linux_config_init;
    init_mc(mc);
    mc->default_cpu_type = TYPE_HEXAGON_CPU_V79;
    mc->default_cpus = 8;
}

static void v79na_1_init(ObjectClass *oc, const void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);

    mc->desc = "Hexagon V79NA_1";
    mc->init = v79na_1_config_init;
    init_mc(mc);
    mc->default_cpu_type = TYPE_HEXAGON_CPU_V79;
    mc->default_cpus = 8;
}

static void v79m_1_init(ObjectClass *oc, const void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);

    mc->desc = "Hexagon V79M_1";
    mc->init = v79m_1_config_init;
    init_mc(mc);
    mc->default_cpu_type = TYPE_HEXAGON_CPU_V79;
    mc->default_cpus = 4;
}

static void v81qa_1_config_init(MachineState *machine)
{
    hexagon_common_init(machine, v81_rev, &v81qa_1);
}

static void v81qa_1_init(ObjectClass *oc, const void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);

    mc->desc = "Hexagon V81QA_1";
    mc->init = v81qa_1_config_init;
    mc->is_default = false;
    mc->block_default_type = IF_SCSI;
    mc->default_cpu_type = TYPE_HEXAGON_CPU_ANY;
    mc->default_cpus = 12;
    mc->max_cpus = THREADS_MAX;
    mc->default_ram_size = 4 * GiB;
}

static void v81na_2_config_init(MachineState *machine)
{
    hexagon_common_init(machine, v81na_2_rev, &v81na_2);
}

static void v81na_2_init(ObjectClass *oc, const void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);

    mc->desc = "Hexagon V81NA_2";
    mc->init = v81na_2_config_init;
    mc->is_default = false;
    mc->block_default_type = IF_SCSI;
    mc->default_cpu_type = TYPE_HEXAGON_CPU_ANY;
    mc->default_cpus = 12;
    mc->max_cpus = THREADS_MAX;
    mc->default_ram_size = 4 * GiB;
}

static void v81dgb_1_config_init(MachineState *machine)
{
    hexagon_common_init(machine, v81dgb_1_rev, &v81dgb_1);
}

static void v81dgb_1_init(ObjectClass *oc, const void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);

    mc->desc = "Hexagon V81DGB_1";
    mc->init = v81dgb_1_config_init;
    mc->is_default = false;
    mc->block_default_type = IF_SCSI;
    mc->default_cpu_type = TYPE_HEXAGON_CPU_ANY;
    mc->default_cpus = 12;
    mc->max_cpus = THREADS_MAX;
    mc->default_ram_size = 4 * GiB;
}

static void sim_init(ObjectClass *oc, const void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);

    mc->desc = "Hexagon Sim-like";
    mc->init = sim_nocoproc_config_init;
    init_mc(mc);
    mc->is_default = true;
    mc->default_cpu_type = glue(TYPE_HEXAGON_CPU_,
        HEXAGON_LATEST_REV_UPPER);
    mc->default_cpus = 6;
}

static void sim_coproc_init(ObjectClass *oc, const void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);

    mc->desc = "Hexagon Sim-like COPROC";
    mc->init = sim_coproc_config_init;
    init_mc(mc);
    mc->default_cpu_type = glue(TYPE_HEXAGON_CPU_,
        HEXAGON_LATEST_REV_UPPER);
    mc->default_cpus = 6;
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
        .name = MACHINE_TYPE_NAME("V73M"),
        .parent = TYPE_MACHINE,
        .class_init = v73m_init,
    }, {
        .name = MACHINE_TYPE_NAME("V73_Linux"),
        .parent = TYPE_MACHINE,
        .class_init = v73na_1024_linux_init,
    }, {
        .name = MACHINE_TYPE_NAME("V75NA_1024"),
        .parent = TYPE_MACHINE,
        .class_init = v75na_1024_init,
    }, {
        .name = MACHINE_TYPE_NAME("V75_Linux"),
        .parent = TYPE_MACHINE,
        .class_init = v75na_1024_linux_init,
    }, {
        .name = MACHINE_TYPE_NAME("V79NA_1"),
        .parent = TYPE_MACHINE,
        .class_init = v79na_1_init,
    }, {
        .name = MACHINE_TYPE_NAME("V79M_1"),
        .parent = TYPE_MACHINE,
        .class_init = v79m_1_init,
    }, {
        .name = MACHINE_TYPE_NAME("V79_Linux"),
        .parent = TYPE_MACHINE,
        .class_init = v79na_1_linux_init,
    }, {
        .name = MACHINE_TYPE_NAME("V81QA_1"),
        .parent = TYPE_MACHINE,
        .class_init = v81qa_1_init,
    }, {
        .name = MACHINE_TYPE_NAME("V81NA_2"),
        .parent = TYPE_MACHINE,
        .class_init = v81na_2_init,
    }, {
        .name = MACHINE_TYPE_NAME("V81DGB_1"),
        .parent = TYPE_MACHINE,
        .class_init = v81dgb_1_init,
    }, {
        .name = MACHINE_TYPE_NAME("V66_Linux"),
        .parent = TYPE_MACHINE,
        .class_init = v66g_linux_init,
    }, {
        .name = MACHINE_TYPE_NAME("V68_H2"),
        .parent = TYPE_MACHINE,
        .class_init = v68n_h2_init,
    }, {
        .name = MACHINE_TYPE_NAME("SA8540P_CDSP0"),
        .parent = TYPE_MACHINE,
        .class_init = SA8540P_cdsp0_init,
    }, {
        .name = MACHINE_TYPE_NAME("SA8775P_CDSP0"),
        .parent = TYPE_MACHINE,
        .class_init = SA8775P_cdsp0_init,
    }, {
        .name = MACHINE_TYPE_NAME("SA8797P_NSP0"),
        .parent = TYPE_MACHINE,
        .class_init = SA8797P_nsp0_init,
    }, {
        .name = MACHINE_TYPE_NAME("sim"),
        .parent = TYPE_MACHINE,
        .class_init = sim_init,
    }, {
        .name = MACHINE_TYPE_NAME("sim_coproc"),
        .parent = TYPE_MACHINE,
        .class_init = sim_coproc_init,
    },
};

DEFINE_TYPES(hexagon_machine_types)
