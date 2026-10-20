/*
 * Hexagon subsystem helpers shared between the machine models.
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "qapi/error.h"
#include "hw/hexagon/hex-subsys.h"
#include "hw/hexagon/hexagon_globalreg.h"
#include "hw/hexagon/hexagon_hvm.h"
#include "hw/hexagon/hexagon_hvx_context.h"
#include "hw/hexagon/hexagon_pmu.h"
#include "hw/hexagon/hexagon_tlb.h"
#include "hw/intc/hex-l2vic.h"
#include "hw/timer/qct-qtimer.h"
#include "hw/cpu/cluster.h"
#include "hw/core/loader.h"
#include "hw/core/qdev-properties.h"
#include "hw/core/qdev.h"
#include "hw/core/sysbus.h"
#include "system/address-spaces.h"

#define HEX_L2VIC_CPU_IRQS 8

/* Number of QTimer frames instantiated for every Hexagon machine. */
#define HEX_QTIMER_NR_FRAMES 3

#define HEX_QTIMER_L2VIC_IRQ_BASE 2

static DeviceState *l2vic_create(HexagonCommonMachineState *hms,
                                 const struct hexagon_machine_config *m_cfg)
{
    DeviceState *l2vic = qdev_new(TYPE_HEX_L2VIC);

    object_property_add_child(OBJECT(hms), "l2vic", OBJECT(l2vic));
    sysbus_realize_and_unref(SYS_BUS_DEVICE(l2vic), &error_fatal);
    sysbus_mmio_map(SYS_BUS_DEVICE(l2vic), 0, m_cfg->l2vic_base);
    sysbus_mmio_map(SYS_BUS_DEVICE(l2vic), 1,
                    m_cfg->cfgtable.fastl2vic_base << 16);

    return l2vic;
}

static void l2vic_connect_cpu(DeviceState *l2vic, DeviceState *cpu)
{
    int i;

    for (i = 0; i < HEX_L2VIC_CPU_IRQS; i++) {
        sysbus_connect_irq(SYS_BUS_DEVICE(l2vic), i, qdev_get_gpio_in(cpu, i));
    }
}

static DeviceState *qtimer_create(HexagonCommonMachineState *hms,
                                  const struct hexagon_machine_config *m_cfg)
{
    DeviceState *qtimer = qdev_new(TYPE_QCT_QTIMER);

    object_property_add_child(OBJECT(hms), "qtimer", OBJECT(qtimer));
    qdev_prop_set_uint32(qtimer, "nr_frames", HEX_QTIMER_NR_FRAMES);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(qtimer), &error_fatal);
    sysbus_mmio_map(SYS_BUS_DEVICE(qtimer), 0, m_cfg->csr_base);
    sysbus_mmio_map(SYS_BUS_DEVICE(qtimer), 1, m_cfg->qtmr_region);
    for (unsigned int i = 0; i < HEX_QTIMER_NR_FRAMES; i++) {
        sysbus_connect_irq(SYS_BUS_DEVICE(qtimer), i,
                           qdev_get_gpio_in(hms->l2vic,
                                            HEX_QTIMER_L2VIC_IRQ_BASE + i));
    }

    return qtimer;
}

static DeviceState *globalreg_create(HexagonCommonMachineState *hms,
                                     const struct hexagon_machine_config *m_cfg,
                                     Rev_t rev)
{
    DeviceState *glob_regs = qdev_new(TYPE_HEXAGON_GLOBALREG);

    object_property_add_child(OBJECT(hms), "global-regs", OBJECT(glob_regs));
    qdev_prop_set_uint64(glob_regs, "config-table-addr", m_cfg->cfgbase);
    qdev_prop_set_uint32(glob_regs, "dsp-rev", rev);
    object_property_set_link(OBJECT(glob_regs), "l2vic", OBJECT(hms->l2vic),
                             &error_fatal);
    object_property_set_link(OBJECT(glob_regs), "qtimer", OBJECT(hms->qtimer),
                             &error_fatal);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(glob_regs), &error_fatal);

    return glob_regs;
}

static DeviceState *tlb_create(HexagonCommonMachineState *hms,
                               const struct hexagon_machine_config *m_cfg)
{
    DeviceState *tlb = qdev_new(TYPE_HEXAGON_TLB);

    object_property_add_child(OBJECT(hms), "tlb", OBJECT(tlb));
    qdev_prop_set_uint32(tlb, "num-entries", m_cfg->cfgtable.jtlb_size_entries);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(tlb), &error_fatal);

    return tlb;
}

/*
 * Create the core's HVX extension contexts.  There are fewer of them than
 * there are hardware threads, and SSR:XA picks which one a thread uses, so
 * they belong to the subsystem rather than to any one CPU.
 */
static void hvx_contexts_create(HexagonCommonMachineState *hms,
                                const struct hexagon_machine_config *m_cfg)
{
    unsigned n = m_cfg->cfgtable.ext_contexts;
    assert(n <= HVX_CONTEXTS_MAX);
    for (unsigned i = 0; i < n; i++) {
        DeviceState *ctx = qdev_new(TYPE_HEXAGON_HVX_CONTEXT);
        g_autofree char *name = g_strdup_printf("hvx-context[%u]", i);

        object_property_add_child(OBJECT(hms), name, OBJECT(ctx));
        qdev_prop_set_uint32(ctx, "index", i);
        sysbus_realize_and_unref(SYS_BUS_DEVICE(ctx), &error_fatal);
    }
    hms->num_hvx_ctx = n;
}

/*
 * The PMU is opt-in (`-machine <type>,pmu=on`): most runs don't need it, and
 * every access to it is NULL-checked, so simply not creating one when the
 * machine didn't ask for it is what makes it free when unused.
 */
static DeviceState *pmu_create(HexagonCommonMachineState *hms)
{
    DeviceState *pmu = qdev_new(TYPE_HEXAGON_PMU);

    /*
     * Named "pmu-device", not "pmu": the machine's own "pmu" bool property
     * (enabling/disabling this device) lives on this same object, and QOM
     * property names must be unique per object.
     */
    object_property_add_child(OBJECT(hms), "pmu-device", OBJECT(pmu));
    sysbus_realize_and_unref(SYS_BUS_DEVICE(pmu), &error_fatal);

    return pmu;
}

/*
 * What the guest of a Hexagon Virtual Machine can ask about the machine with
 * vmgetinfo.  The registers of the core itself (SYSCFG) are read when asked.
 */
static void hvm_fill_info(HexagonHVMState *s, MachineState *machine,
                          const struct hexagon_machine_config *m_cfg,
                          Rev_t rev)
{
    const union hexagon_config_table *t = &m_cfg->cfgtable;
    bool hvx = t->ext_contexts > 0;

    s->info[HVM_INFO_BOOT_FLAGS] = (hvx ? HVM_BOOT_FLAG_HAVE_HVX |
                                          HVM_BOOT_FLAG_EXT_OK : 0);
    s->info[HVM_INFO_REV] = rev;
    s->info[HVM_INFO_SSBASE] = t->subsystem_base << 16;
    s->info[HVM_INFO_TLB_SIZE] = t->jtlb_size_entries;
    s->info[HVM_INFO_TLB_FREE] = t->jtlb_size_entries;
    s->info[HVM_INFO_TCM_BASE] = t->l2tcm_base << 16;
    s->info[HVM_INFO_TCM_SIZE] = m_cfg->l2tcm_size;
    s->info[HVM_INFO_L2MEM_SIZE] = m_cfg->l2tcm_size;
    /* The guest is told it is on a core with the monitor's own 1M page. */
    s->info[HVM_INFO_KERNEL_PGSIZE] = 0x100000;
    s->info[HVM_INFO_KERNEL_NPAGES] = 1;
    s->info[HVM_INFO_L2VIC_BASE] = m_cfg->l2vic_base;
    s->info[HVM_INFO_TIMER_BASE] = m_cfg->qtmr_region;
    s->info[HVM_INFO_TIMER_INT] = HVM_DEVICE_INT_BASE +
                                  HEX_QTIMER_L2VIC_IRQ_BASE;
    s->info[HVM_INFO_HTHREADS] = MAKE_64BIT_MASK(0, machine->smp.cpus);
    s->info[HVM_INFO_L2CFG_BASE] = t->l2cfg_base << 16;
    s->info[HVM_INFO_CFG_BASE] = m_cfg->cfgbase >> 16;
    if (hvx) {
        s->info[HVM_INFO_HVX_VLENGTH] = 1u << t->hvx_vec_log_length;
    }
    s->info[HVM_INFO_COPROC_CONTEXTS] = t->ext_contexts;
    s->info[HVM_INFO_MAX_CLUSTER_COPROC] = t->ext_contexts;
    s->info[HVM_INFO_VTCM_BASE] = t->vtcm_base << 16;
    s->info[HVM_INFO_VTCM_SIZE] = t->vtcm_size_kb;
    s->info[HVM_INFO_L2LINE_SIZE] = 64;
    s->info[HVM_INFO_VTCM_BANK_WIDTH] = t->vtcm_bank_width;
    s->info[HVM_INFO_L1D_SIZE] = t->l1d_size_kb * 1024;
    s->info[HVM_INFO_HMX_INSTANCES] = !!(t->coproc_present & (1u << 2));
    s->info[HVM_INFO_HLX_CONTEXTS] = ctpop32(t->coproc2_reg2);
    s->info[HVM_INFO_CORE_COUNT] = 1;
}

static DeviceState *hvm_create(HexagonCommonMachineState *hms,
                               const struct hexagon_machine_config *m_cfg,
                               Rev_t rev)
{
    DeviceState *hvm = qdev_new(TYPE_HEXAGON_HVM);

    object_property_add_child(OBJECT(hms), "hvm-monitor", OBJECT(hvm));
    hvm_fill_info(HEXAGON_HVM(hvm), MACHINE(hms), m_cfg, rev);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(hvm), &error_fatal);

    return hvm;
}

static DeviceState *cluster_create(HexagonCommonMachineState *hms)
{
    DeviceState *cluster = qdev_new(TYPE_CPU_CLUSTER);

    object_property_add_child(OBJECT(hms), "cluster", OBJECT(cluster));
    qdev_prop_set_uint32(cluster, "cluster-id", 0);

    return cluster;
}

void hex_subsys_create(HexagonCommonMachineState *hms,
                       const struct hexagon_machine_config *m_cfg, Rev_t rev)
{
    MachineState *machine = MACHINE(hms);
    MemoryRegion *sysmem = get_system_memory();

    /* Main DDR. */
    memory_region_init_ram(&hms->ram, NULL, "ddr.ram", machine->ram_size,
                           &error_fatal);
    memory_region_add_subregion(sysmem, m_cfg->ddr_base, &hms->ram);

    /* Config-table ROM and the blob that backs it. */
    memory_region_init_rom(&hms->cfgtable_rom, NULL, "config_table.rom",
                           sizeof(m_cfg->cfgtable), &error_fatal);
    memory_region_add_subregion(sysmem, m_cfg->cfgbase, &hms->cfgtable_rom);
    rom_add_blob_fixed_as("config_table.rom", &m_cfg->cfgtable,
                          sizeof(m_cfg->cfgtable), m_cfg->cfgbase,
                          &address_space_memory);

    if (m_cfg->cfgtable.vtcm_size_kb > 0) {
        memory_region_init_ram(&hms->vtcm, NULL, "vtcm.ram",
                               m_cfg->cfgtable.vtcm_size_kb * 1024,
                               &error_fatal);
        memory_region_add_subregion(sysmem, m_cfg->cfgtable.vtcm_base << 16,
                                    &hms->vtcm);
    }

    hms->cluster = cluster_create(hms);
    if (hms->hvm_enabled) {
        hms->hvm = hvm_create(hms, m_cfg, rev);
    }
    hms->l2vic = l2vic_create(hms, m_cfg);
    hms->qtimer = qtimer_create(hms, m_cfg);
    hms->glob_regs = globalreg_create(hms, m_cfg, rev);
    hms->tlb = tlb_create(hms, m_cfg);
    hvx_contexts_create(hms, m_cfg);
    if (hms->pmu_enabled) {
        hms->pmu = pmu_create(hms);
    }
}

void hex_subsys_add_cpu(HexagonCommonMachineState *hms, DeviceState *cpu)
{
    object_property_add_child(OBJECT(hms->cluster), "cpu[*]", OBJECT(cpu));
    object_property_set_link(OBJECT(cpu), "global-regs",
                             OBJECT(hms->glob_regs), &error_fatal);
    object_property_set_link(OBJECT(cpu), "tlb", OBJECT(hms->tlb),
                             &error_fatal);
    object_property_set_link(OBJECT(cpu), "l2vic", OBJECT(hms->l2vic),
                             &error_fatal);
    if (hms->pmu) {
        object_property_set_link(OBJECT(cpu), "pmu", OBJECT(hms->pmu),
                                 &error_fatal);
    }
    if (hms->hvm) {
        object_property_set_link(OBJECT(cpu), "hvm", OBJECT(hms->hvm),
                                 &error_fatal);
    }
    for (unsigned i = 0; i < hms->num_hvx_ctx; i++) {
        g_autofree char *name = g_strdup_printf("hvx-context[%u]", i);
        Object *ctx = object_resolve_path_component(OBJECT(hms), name);

        object_property_set_link(OBJECT(cpu), name, ctx, &error_fatal);
    }
}

void hex_subsys_realize_cluster(HexagonCommonMachineState *hms)
{
    /*
     * The cluster must be realized after its CPUs have been parented into it
     * (see hex_subsys_add_cpu()) but before any CPU is itself realized, since
     * qdev_realize_and_unref() on a CPU latches cluster_index into the TCG
     * cflags at that point.
     */
    qdev_realize_and_unref(hms->cluster, NULL, &error_fatal);
}

void hex_subsys_realize_cpu(HexagonCommonMachineState *hms, DeviceState *cpu,
                            bool boot_cpu)
{
    unsigned htid = HEXAGON_CPU(cpu)->htid;

    qdev_realize_and_unref(cpu, NULL, &error_fatal);

    if (hms->hvm) {
        hexagon_hvm_add_cpu(HEXAGON_HVM(hms->hvm), CPU(cpu), htid);
    }
    if (boot_cpu) {
        l2vic_connect_cpu(hms->l2vic, cpu);
    }
}
