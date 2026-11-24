/*
 * Heterogeneous machine: one aarch64 cpu and one riscv64 cpu sharing the same
 * memory and devices.
 *
 * This machine is meant to exercise heterogeneous emulation, and requires
 * QEMU to run with several targets enabled:
 *   qemu-system -target aarch64,riscv64 -M heterogeneous
 *
 * Each cpu has its own view of memory. Devices and SRAM are shared, while
 * RAM is private: it is split in two halves, one per cpu, both mapped at the
 * same address.
 *
 * Each cpu has its own interrupt controller, only visible in its view of
 * memory.
 *
 * Memory map (as seen by each cpu):
 *   0x02000000 - 0x02003fff: riscv64 only: ACLINT MSWI
 *   0x02004000 - 0x0200bfff: riscv64 only: ACLINT MTIMER (10 MHz)
 *   0x08000000 - 0x0800ffff: aarch64 only: GICv3 distributor
 *   0x080a0000 - 0x080bffff: aarch64 only: GICv3 redistributor
 *   0x0c000000 - 0x0c5fffff: riscv64 only: PLIC
 *   0x10000000 - 0x100000ff: UART (16550, shared by all cpus)
 *   0x20000000 - 0x2000ffff: shared SRAM (e.g. for mailboxes)
 *   0x40000000 - ...       : private RAM (half of -m), cpu reset address
 *
 * Interrupts:
 *   aarch64 cpu starts at EL1 (no EL2/EL3), GICv3 has no security extensions.
 *   Generic timer is wired on PPIs (NS EL1 physical timer is INTID 30).
 *   riscv64 cpu has hart ID 1. ACLINT drives MSIP/MTIP, PLIC (contexts: 0 is
 *   M-mode, 1 is S-mode) drives MEIP/SEIP.
 *   UART interrupt is routed to both: GIC SPI 1 (INTID 33), PLIC source 10.
 *
 * Each cpu lives in its own cluster, so translated code is never shared
 * between them. This is mandatory, since the same physical address holds
 * different content for each cpu.
 *
 * Images must be loaded in the address space of their cpu, e.g. with
 *   -device loader,file=<elf>,cpu-num=<0 for aarch64, 1 for riscv64>
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "qemu/units.h"
#include "qemu/target-info.h"
#include "qemu/target-info-qom.h"
#include "qapi/error.h"
#include "qemu/error-report.h"
#include "hw/core/qdev-properties.h"
#include "hw/core/boards.h"
#include "hw/core/cpu.h"
#include "hw/cpu/cluster.h"
#include "hw/char/serial-mm.h"
#include "hw/core/split-irq.h"
#include "hw/core/sysbus.h"
#include "hw/arm/bsa.h"
#include "hw/intc/arm_gicv3.h"
#include "hw/intc/riscv_aclint.h"
#include "hw/intc/sifive_plic.h"
#include "target/arm/cpu-qom.h"
#include "target/arm/gtimer.h"
#include "qobject/qlist.h"
#include "system/address-spaces.h"
#include "system/memory.h"
#include "system/reset.h"
#include "system/system.h"

#define TYPE_HETEROGENEOUS_MACHINE MACHINE_TYPE_NAME("heterogeneous")
OBJECT_DECLARE_SIMPLE_TYPE(HeterogeneousMachineState, HETEROGENEOUS_MACHINE)

/* avoid including target headers, this file is target agnostic */
#define HETEROGENEOUS_ARM_CPU_TYPE      "cortex-a57-arm-cpu"
#define HETEROGENEOUS_RISCV_CPU_TYPE    "rv64-riscv-cpu"

#define HETEROGENEOUS_UART_BASE         0x10000000
#define HETEROGENEOUS_UART_BAUDBASE     115200
#define HETEROGENEOUS_SRAM_BASE         0x20000000
#define HETEROGENEOUS_SRAM_SIZE         (64 * KiB)
#define HETEROGENEOUS_RAM_BASE          0x40000000
#define HETEROGENEOUS_RESET_ADDR        HETEROGENEOUS_RAM_BASE
#define HETEROGENEOUS_RAM_MIN_SIZE      (256 * MiB)

/* aarch64 interrupt controller */
#define HETEROGENEOUS_GIC_DIST_BASE     0x08000000
#define HETEROGENEOUS_GIC_REDIST_BASE   0x080a0000
#define HETEROGENEOUS_GIC_NUM_SPIS      32
#define HETEROGENEOUS_GIC_UART_SPI      1

/* riscv64 interrupt controllers */
#define HETEROGENEOUS_RISCV_HARTID      1
#define HETEROGENEOUS_ACLINT_BASE       0x02000000
#define HETEROGENEOUS_ACLINT_MTIMER_BASE \
    (HETEROGENEOUS_ACLINT_BASE + RISCV_ACLINT_SWI_SIZE)
#define HETEROGENEOUS_ACLINT_TIMEBASE   10000000
#define HETEROGENEOUS_PLIC_BASE         0x0c000000
#define HETEROGENEOUS_PLIC_SIZE         0x600000
#define HETEROGENEOUS_PLIC_NUM_SOURCES  32
#define HETEROGENEOUS_PLIC_UART_SOURCE  10

/*
 * riscv interrupt numbers (mip/mie bits), used as riscv cpu gpio inputs.
 * They are architectural, but target/riscv/cpu_bits.h can't be included
 * from target agnostic code.
 */
#define HETEROGENEOUS_RISCV_IRQ_M_SOFT  3
#define HETEROGENEOUS_RISCV_IRQ_M_TIMER 7
#define HETEROGENEOUS_RISCV_IRQ_S_EXT   9
#define HETEROGENEOUS_RISCV_IRQ_M_EXT   11

enum {
    HETEROGENEOUS_CLUSTER_ARM,
    HETEROGENEOUS_CLUSTER_RISCV,
    HETEROGENEOUS_NUM_CLUSTERS,
};

struct HeterogeneousMachineState {
    MachineState parent_obj;

    CPUClusterState clusters[HETEROGENEOUS_NUM_CLUSTERS];
    MemoryRegion sram;
    /* per cpu view of memory */
    MemoryRegion cpu_mem[HETEROGENEOUS_NUM_CLUSTERS];
    /* private RAM of each cpu (alias of a part of machine RAM) */
    MemoryRegion cpu_ram[HETEROGENEOUS_NUM_CLUSTERS];
    /* shared devices and memory (alias of system memory) */
    MemoryRegion cpu_shared[HETEROGENEOUS_NUM_CLUSTERS];

    CPUState *cpus[HETEROGENEOUS_NUM_CLUSTERS];
    DeviceState *gic;
    DeviceState *plic;
    DeviceState *uart_irq_split;
};

static const char *const heterogeneous_targets[] = { "aarch64", "riscv64" };

/* types of riscv interrupt controllers, compiled for riscv targets only */
static const char *const heterogeneous_riscv_types[] = {
    TYPE_RISCV_ACLINT_SWI, TYPE_RISCV_ACLINT_MTIMER, TYPE_SIFIVE_PLIC,
};

/*
 * Create view of memory for cpu of cluster @cluster_id:
 * - private RAM: part @cluster_id of machine RAM
 * - everything else: shared, through an alias of system memory
 */
static MemoryRegion *heterogeneous_create_cpu_mem(HeterogeneousMachineState *s,
                                                  unsigned cluster_id)
{
    MachineState *machine = MACHINE(s);
    uint64_t ram_size = machine->ram_size / HETEROGENEOUS_NUM_CLUSTERS;
    g_autofree char *mem_name = g_strdup_printf("cpu-memory%u", cluster_id);
    g_autofree char *ram_name = g_strdup_printf("cpu-ram%u", cluster_id);
    g_autofree char *shared_name = g_strdup_printf("cpu-shared%u", cluster_id);
    MemoryRegion *mem = &s->cpu_mem[cluster_id];

    memory_region_init(mem, OBJECT(s), mem_name, UINT64_MAX);

    memory_region_init_alias(&s->cpu_ram[cluster_id], OBJECT(s), ram_name,
                             machine->ram, cluster_id * ram_size, ram_size);
    memory_region_add_subregion(mem, HETEROGENEOUS_RAM_BASE,
                                &s->cpu_ram[cluster_id]);

    /* lower priority, so private RAM always takes precedence */
    memory_region_init_alias(&s->cpu_shared[cluster_id], OBJECT(s),
                             shared_name, get_system_memory(), 0, UINT64_MAX);
    memory_region_add_subregion_overlap(mem, 0, &s->cpu_shared[cluster_id],
                                        -1);

    return mem;
}

/* cpus are not reset automatically on system reset */
static void heterogeneous_cpu_reset(void *opaque)
{
    cpu_reset(CPU(opaque));
}

static CPUState *heterogeneous_create_cpu(HeterogeneousMachineState *s,
                                          unsigned cluster_id,
                                          const char *cpu_type,
                                          const char *reset_prop)
{
    CPUClusterState *cluster = &s->clusters[cluster_id];
    g_autofree char *cluster_name = g_strdup_printf("cluster%u", cluster_id);
    MemoryRegion *mem = heterogeneous_create_cpu_mem(s, cluster_id);
    Object *cpu;

    object_initialize_child(OBJECT(s), cluster_name, cluster,
                            TYPE_CPU_CLUSTER);
    qdev_prop_set_uint32(DEVICE(cluster), "cluster-id", cluster_id);

    cpu = object_new(cpu_type);
    object_property_add_child(OBJECT(cluster), "cpu", cpu);
    object_unref(cpu);
    object_property_set_uint(cpu, reset_prop, HETEROGENEOUS_RESET_ADDR,
                             &error_fatal);
    object_property_set_link(cpu, "memory", OBJECT(mem), &error_fatal);
    if (cluster_id == HETEROGENEOUS_CLUSTER_ARM) {
        /* start at EL1, so GIC doesn't need security extensions */
        object_property_set_bool(cpu, "has_el3", false, &error_fatal);
        object_property_set_bool(cpu, "has_el2", false, &error_fatal);
    } else {
        /*
         * riscv interrupt controllers identify harts either with their arch
         * ID (cpu_by_arch_id()) or their cpu index (qemu_get_cpu()). Make both
         * identical, and different from arch ID of arm cpu (MPIDR affinity,
         * 0 here).
         */
        object_property_set_uint(cpu, "mhartid", HETEROGENEOUS_RISCV_HARTID,
                                 &error_fatal);
    }

    /*
     * Realize cluster before its cpu, so cpu->cluster_index is already set
     * when the vcpu thread is created (it's used to compute TB cflags).
     */
    qdev_realize(DEVICE(cluster), NULL, &error_fatal);
    qdev_realize(DEVICE(cpu), NULL, &error_fatal);
    qemu_register_reset(heterogeneous_cpu_reset, cpu);

    s->cpus[cluster_id] = CPU(cpu);
    return CPU(cpu);
}

/* map a device region in private view of memory of a cpu */
static void heterogeneous_map_private(HeterogeneousMachineState *s,
                                      unsigned cluster_id, hwaddr addr,
                                      DeviceState *dev, int n)
{
    memory_region_add_subregion(&s->cpu_mem[cluster_id], addr,
                                sysbus_mmio_get_region(SYS_BUS_DEVICE(dev), n));
}

static void heterogeneous_create_arm_intc(HeterogeneousMachineState *s)
{
    DeviceState *cpudev = DEVICE(s->cpus[HETEROGENEOUS_CLUSTER_ARM]);
    SysBusDevice *gicbusdev;
    QList *redist_region_count;
    /* GIC gpio inputs: SPIs first, then PPIs of each cpu */
    int ppibase = HETEROGENEOUS_GIC_NUM_SPIS;
    const int timer_irq[] = {
        [GTIMER_PHYS] = ARCH_TIMER_NS_EL1_IRQ,
        [GTIMER_VIRT] = ARCH_TIMER_VIRT_IRQ,
        [GTIMER_HYP]  = ARCH_TIMER_NS_EL2_IRQ,
        [GTIMER_SEC]  = ARCH_TIMER_S_EL1_IRQ,
    };

    s->gic = qdev_new(TYPE_ARM_GICV3);
    qdev_prop_set_uint32(s->gic, "revision", 3);
    qdev_prop_set_uint32(s->gic, "num-cpu", 1);
    qdev_prop_set_uint32(s->gic, "first-cpu-index",
                         s->cpus[HETEROGENEOUS_CLUSTER_ARM]->cpu_index);
    /* num-irq counts the 32 internal interrupts (SGIs and PPIs) */
    qdev_prop_set_uint32(s->gic, "num-irq",
                         HETEROGENEOUS_GIC_NUM_SPIS + GIC_INTERNAL);
    redist_region_count = qlist_new();
    qlist_append_int(redist_region_count, 1);
    qdev_prop_set_array(s->gic, "redist-region-count", redist_region_count);

    gicbusdev = SYS_BUS_DEVICE(s->gic);
    sysbus_realize_and_unref(gicbusdev, &error_fatal);
    heterogeneous_map_private(s, HETEROGENEOUS_CLUSTER_ARM,
                              HETEROGENEOUS_GIC_DIST_BASE, s->gic, 0);
    heterogeneous_map_private(s, HETEROGENEOUS_CLUSTER_ARM,
                              HETEROGENEOUS_GIC_REDIST_BASE, s->gic, 1);

    for (unsigned irq = 0; irq < ARRAY_SIZE(timer_irq); irq++) {
        qdev_connect_gpio_out(cpudev, irq,
                              qdev_get_gpio_in(s->gic,
                                               ppibase + timer_irq[irq]));
    }

    /* GIC outputs, for our single cpu: IRQ, FIQ, VIRQ, VFIQ */
    sysbus_connect_irq(gicbusdev, 0, qdev_get_gpio_in(cpudev, ARM_CPU_IRQ));
    sysbus_connect_irq(gicbusdev, 1, qdev_get_gpio_in(cpudev, ARM_CPU_FIQ));
    sysbus_connect_irq(gicbusdev, 2, qdev_get_gpio_in(cpudev, ARM_CPU_VIRQ));
    sysbus_connect_irq(gicbusdev, 3, qdev_get_gpio_in(cpudev, ARM_CPU_VFIQ));
}

/*
 * riscv interrupt controllers are created through QOM only: helpers to create
 * them are compiled for riscv targets only, while this machine is compiled
 * for aarch64.
 */
static void heterogeneous_create_riscv_intc(HeterogeneousMachineState *s)
{
    DeviceState *cpudev = DEVICE(s->cpus[HETEROGENEOUS_CLUSTER_RISCV]);
    DeviceState *swi, *mtimer;

    for (size_t i = 0; i < ARRAY_SIZE(heterogeneous_riscv_types); i++) {
        if (!object_class_by_name(heterogeneous_riscv_types[i])) {
            error_report("device %s is not available",
                         heterogeneous_riscv_types[i]);
            exit(1);
        }
    }

    /* ACLINT: software interrupts (M-mode) */
    swi = qdev_new(TYPE_RISCV_ACLINT_SWI);
    qdev_prop_set_uint32(swi, "hartid-base", HETEROGENEOUS_RISCV_HARTID);
    qdev_prop_set_uint32(swi, "num-harts", 1);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(swi), &error_fatal);
    heterogeneous_map_private(s, HETEROGENEOUS_CLUSTER_RISCV,
                              HETEROGENEOUS_ACLINT_BASE, swi, 0);
    qdev_connect_gpio_out(swi, 0, qdev_get_gpio_in(cpudev,
                                           HETEROGENEOUS_RISCV_IRQ_M_SOFT));

    /* ACLINT: machine timer */
    mtimer = qdev_new(TYPE_RISCV_ACLINT_MTIMER);
    qdev_prop_set_uint32(mtimer, "hartid-base", HETEROGENEOUS_RISCV_HARTID);
    qdev_prop_set_uint32(mtimer, "num-harts", 1);
    qdev_prop_set_uint32(mtimer, "timecmp-base", 0);
    qdev_prop_set_uint32(mtimer, "time-base", RISCV_ACLINT_DEFAULT_MTIME);
    qdev_prop_set_uint32(mtimer, "aperture-size",
                         RISCV_ACLINT_DEFAULT_MTIMER_SIZE);
    qdev_prop_set_uint32(mtimer, "timebase-freq",
                         HETEROGENEOUS_ACLINT_TIMEBASE);
    qdev_prop_set_bit(mtimer, "provide-rdtime", true);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(mtimer), &error_fatal);
    heterogeneous_map_private(s, HETEROGENEOUS_CLUSTER_RISCV,
                              HETEROGENEOUS_ACLINT_MTIMER_BASE, mtimer, 0);
    qdev_connect_gpio_out(mtimer, 0, qdev_get_gpio_in(cpudev,
                                           HETEROGENEOUS_RISCV_IRQ_M_TIMER));

    /* PLIC: external interrupts, M-mode and S-mode contexts */
    s->plic = qdev_new(TYPE_SIFIVE_PLIC);
    qdev_prop_set_string(s->plic, "hart-config", "MS");
    qdev_prop_set_uint32(s->plic, "hartid-base", HETEROGENEOUS_RISCV_HARTID);
    qdev_prop_set_uint32(s->plic, "num-sources",
                         HETEROGENEOUS_PLIC_NUM_SOURCES);
    qdev_prop_set_uint32(s->plic, "num-priorities", 7);
    qdev_prop_set_uint32(s->plic, "priority-base", 0x00);
    qdev_prop_set_uint32(s->plic, "pending-base", 0x1000);
    qdev_prop_set_uint32(s->plic, "enable-base", 0x2000);
    qdev_prop_set_uint32(s->plic, "enable-stride", 0x80);
    qdev_prop_set_uint32(s->plic, "context-base", 0x200000);
    qdev_prop_set_uint32(s->plic, "context-stride", 0x1000);
    qdev_prop_set_uint32(s->plic, "aperture-size", HETEROGENEOUS_PLIC_SIZE);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(s->plic), &error_fatal);
    heterogeneous_map_private(s, HETEROGENEOUS_CLUSTER_RISCV,
                              HETEROGENEOUS_PLIC_BASE, s->plic, 0);
    /* PLIC outputs: S-mode of each hart, then M-mode of each hart */
    qdev_connect_gpio_out(s->plic, 0, qdev_get_gpio_in(cpudev,
                                           HETEROGENEOUS_RISCV_IRQ_S_EXT));
    qdev_connect_gpio_out(s->plic, 1, qdev_get_gpio_in(cpudev,
                                           HETEROGENEOUS_RISCV_IRQ_M_EXT));
}

/* shared devices interrupts are routed to interrupt controllers of all cpus */
static qemu_irq heterogeneous_shared_irq(HeterogeneousMachineState *s,
                                         int gic_spi, int plic_source)
{
    DeviceState *split = qdev_new(TYPE_SPLIT_IRQ);

    qdev_prop_set_uint32(split, "num-lines", 2);
    qdev_realize_and_unref(split, NULL, &error_fatal);
    qdev_connect_gpio_out(split, 0, qdev_get_gpio_in(s->gic, gic_spi));
    qdev_connect_gpio_out(split, 1, qdev_get_gpio_in(s->plic, plic_source));

    return qdev_get_gpio_in(split, 0);
}

static void heterogeneous_init(MachineState *machine)
{
    HeterogeneousMachineState *s = HETEROGENEOUS_MACHINE(machine);
    MemoryRegion *sysmem = get_system_memory();

    for (size_t i = 0; i < ARRAY_SIZE(heterogeneous_targets); i++) {
        if (!target_info_qom_find_target(heterogeneous_targets[i])) {
            error_report("machine %s requires target %s to be enabled "
                         "(use -target aarch64,riscv64)",
                         MACHINE_GET_CLASS(machine)->name,
                         heterogeneous_targets[i]);
            exit(1);
        }
    }

    if (machine->ram_size < HETEROGENEOUS_RAM_MIN_SIZE) {
        error_report("machine %s requires at least %" PRIu64 " MiB of RAM",
                     MACHINE_GET_CLASS(machine)->name,
                     (uint64_t)HETEROGENEOUS_RAM_MIN_SIZE / MiB);
        exit(1);
    }

    /*
     * Shared memory and devices are mapped in system memory, and each cpu
     * sees them through an alias. RAM is not mapped in system memory, and
     * each cpu only sees its own part.
     */
    memory_region_init_ram(&s->sram, NULL, "heterogeneous.sram",
                           HETEROGENEOUS_SRAM_SIZE, &error_fatal);
    memory_region_add_subregion(sysmem, HETEROGENEOUS_SRAM_BASE, &s->sram);

    /* cpus: index 0 is aarch64, index 1 is riscv64 */
    heterogeneous_create_cpu(s, HETEROGENEOUS_CLUSTER_ARM,
                             HETEROGENEOUS_ARM_CPU_TYPE, "rvbar");
    heterogeneous_create_cpu(s, HETEROGENEOUS_CLUSTER_RISCV,
                             HETEROGENEOUS_RISCV_CPU_TYPE, "resetvec");
    g_assert(s->cpus[HETEROGENEOUS_CLUSTER_RISCV]->cpu_index ==
             HETEROGENEOUS_RISCV_HARTID);

    /* interrupt controllers, private to each cpu */
    heterogeneous_create_arm_intc(s);
    heterogeneous_create_riscv_intc(s);

    /* devices */
    serial_mm_init(sysmem, HETEROGENEOUS_UART_BASE, 0,
                   heterogeneous_shared_irq(s, HETEROGENEOUS_GIC_UART_SPI,
                                            HETEROGENEOUS_PLIC_UART_SOURCE),
                   HETEROGENEOUS_UART_BAUDBASE, serial_hd(0),
                   DEVICE_LITTLE_ENDIAN);
}

static void heterogeneous_machine_class_init(ObjectClass *oc,
                                             const void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);

    mc->desc = "Heterogeneous machine with an aarch64 and a riscv64 cpu";
    mc->init = heterogeneous_init;
    mc->min_cpus = 2;
    mc->max_cpus = 2;
    mc->default_cpus = 2;
    mc->default_ram_size = HETEROGENEOUS_RAM_MIN_SIZE;
    mc->default_ram_id = "heterogeneous.ram";
    mc->minimum_page_bits = 12;
    mc->no_parallel = 1;
    mc->no_floppy = 1;
    mc->no_cdrom = 1;
}

static bool heterogeneous_machine_is_available(void)
{
    /* machines are only checked against primary target */
    return target_heterogeneous();
}

static const TypeInfo heterogeneous_machine_type = {
    .name = TYPE_HETEROGENEOUS_MACHINE,
    .parent = TYPE_MACHINE,
    .instance_size = sizeof(HeterogeneousMachineState),
    .class_init = heterogeneous_machine_class_init,
    .is_available = heterogeneous_machine_is_available,
};

static void heterogeneous_machine_register_types(void)
{
    type_register_static(&heterogeneous_machine_type);
}

type_init(heterogeneous_machine_register_types)
