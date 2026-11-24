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
 * Memory map (as seen by each cpu):
 *   0x10000000 - 0x100000ff: UART (16550, shared by all cpus)
 *   0x20000000 - 0x2000ffff: shared SRAM (e.g. for mailboxes)
 *   0x40000000 - ...       : private RAM (half of -m), cpu reset address
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
};

static const char *const heterogeneous_targets[] = { "aarch64", "riscv64" };

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

    /*
     * Realize cluster before its cpu, so cpu->cluster_index is already set
     * when the vcpu thread is created (it's used to compute TB cflags).
     */
    qdev_realize(DEVICE(cluster), NULL, &error_fatal);
    qdev_realize(DEVICE(cpu), NULL, &error_fatal);
    qemu_register_reset(heterogeneous_cpu_reset, cpu);

    return CPU(cpu);
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

    /* devices */
    serial_mm_init(sysmem, HETEROGENEOUS_UART_BASE, 0, NULL,
                   HETEROGENEOUS_UART_BAUDBASE, serial_hd(0),
                   DEVICE_LITTLE_ENDIAN);

    /* cpus: index 0 is aarch64, index 1 is riscv64 */
    heterogeneous_create_cpu(s, HETEROGENEOUS_CLUSTER_ARM,
                             HETEROGENEOUS_ARM_CPU_TYPE, "rvbar");
    heterogeneous_create_cpu(s, HETEROGENEOUS_CLUSTER_RISCV,
                             HETEROGENEOUS_RISCV_CPU_TYPE, "resetvec");
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
