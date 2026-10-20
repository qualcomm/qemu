/*
 * Hexagon Virtual Machine (HVM) monitor state
 *
 * The HVM interface lets a guest kernel run without a hypervisor binary:
 * QEMU itself plays the monitor.  This device holds the state that the
 * virtual CPUs share -- the virtual interrupt controller, the timeouts and
 * the set of started virtual CPUs.  The per-CPU half of the interface
 * (virtual instructions, translation, event delivery) is in
 * target/hexagon/hvm.c.
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef HW_HEXAGON_HVM_H
#define HW_HEXAGON_HVM_H

#include "hw/core/sysbus.h"
#include "qom/object.h"
#include "qemu/timer.h"

#define TYPE_HEXAGON_HVM "hexagon-hvm"
OBJECT_DECLARE_SIMPLE_TYPE(HexagonHVMState, HEXAGON_HVM)

#define HVM_MAX_VCPUS       8
/* Interrupts 0..15 belong to one virtual CPU, the rest are shared. */
#define HVM_PERCPU_INTS     16
#define HVM_SHARED_INTS     320
/* Device line n arrives as interrupt HVM_DEVICE_INT_BASE + n. */
#define HVM_DEVICE_INT_BASE 32
#define HVM_NUM_LINES       (HVM_SHARED_INTS - (HVM_DEVICE_INT_BASE - \
                                                HVM_PERCPU_INTS))
/* The per-CPU interrupt a vmtimerop timeout arrives on. */
#define HVM_TIMER_INT       12
/* VM index that is reported in virtual CPU ids (0 means "the caller"). */
#define HVM_VMIDX           2

/* vmintop operations */
enum {
    HVM_INTOP_NOP = 0,
    HVM_INTOP_GLOBEN,
    HVM_INTOP_GLOBDIS,
    HVM_INTOP_LOCEN,
    HVM_INTOP_LOCDIS,
    HVM_INTOP_AFFINITY,
    HVM_INTOP_GET,
    HVM_INTOP_PEEK,
    HVM_INTOP_STATUS,
    HVM_INTOP_POST,
    HVM_INTOP_CLEAR,
};

/* vmtimerop operations */
enum {
    HVM_TIMEROP_GETFREQ = 0,
    HVM_TIMEROP_GETRES,
    HVM_TIMEROP_GETTIME,
    HVM_TIMEROP_GETTIMEOUT,
    HVM_TIMEROP_SETTIMEOUT,
    HVM_TIMEROP_DELTATIMEOUT,
};

/* vmstop status values */
enum {
    HVM_STOP_NONE = 0,
    HVM_STOP_POWEROFF,
    HVM_STOP_HALT,
    HVM_STOP_RESTART,
    HVM_STOP_MACHINECHECK,
};

/* vmgetinfo codes (the H2 InfoType numbering) */
enum {
    HVM_INFO_BUILD_ID = 0,
    HVM_INFO_BOOT_FLAGS = 1,
    HVM_INFO_STLB = 2,
    HVM_INFO_SYSCFG = 3,
    HVM_INFO_LIVELOCK = 4,
    HVM_INFO_REV = 5,
    HVM_INFO_SSBASE = 6,
    HVM_INFO_TLB_FREE = 7,
    HVM_INFO_TLB_SIZE = 8,
    HVM_INFO_PHYSADDR = 9,
    HVM_INFO_TCM_BASE = 10,
    HVM_INFO_L2MEM_SIZE = 11,
    HVM_INFO_TCM_SIZE = 12,
    HVM_INFO_KERNEL_PGSIZE = 13,
    HVM_INFO_KERNEL_NPAGES = 14,
    HVM_INFO_L2VIC_BASE = 15,
    HVM_INFO_TIMER_BASE = 16,
    HVM_INFO_TIMER_INT = 17,
    HVM_INFO_ERROR = 18,
    HVM_INFO_HTHREADS = 19,
    HVM_INFO_L2TAG_SIZE = 20,
    HVM_INFO_L2CFG_BASE = 21,
    HVM_INFO_CLADE_BASE = 22,
    HVM_INFO_CFG_BASE = 23,
    HVM_INFO_HVX_VLENGTH = 24,
    HVM_INFO_COPROC_CONTEXTS = 25,
    HVM_INFO_HVX_SWITCH = 26,
    HVM_INFO_VTCM_BASE = 27,
    HVM_INFO_VTCM_SIZE = 28,
    HVM_INFO_ECC_BASE = 29,
    HVM_INFO_L2LINE_SIZE = 30,
    HVM_INFO_AUDIO_EXT = 31,
    HVM_INFO_VTCM_BANK_WIDTH = 32,
    HVM_INFO_L1D_SIZE = 33,
    HVM_INFO_MAX_CLUSTER_COPROC = 34,
    HVM_INFO_HMX_INSTANCES = 35,
    HVM_INFO_CORECFG_BASE = 36,
    HVM_INFO_HLX_CONTEXTS = 37,
    HVM_INFO_UNIT_START = 38,
    HVM_INFO_UNIT_ENTRY = 39,
    HVM_INFO_CORE_ID = 40,
    HVM_INFO_CORE_COUNT = 41,
    HVM_INFO_SHIFT = 42,
    HVM_INFO_TCM_OFFSET = 43,
    HVM_INFO_NOC_MBASE = 44,
    HVM_INFO_NOC_SBASE = 45,
    HVM_INFO_BOOT_R0 = 46,
    HVM_INFO_BOOT_R1 = 47,
    HVM_INFO_MAX = 48,
};

/* HVM_INFO_BOOT_FLAGS bits */
#define HVM_BOOT_FLAG_HAVE_HVX  (1u << 1)
#define HVM_BOOT_FLAG_EXT_OK    (1u << 3)

/* What vmgetinfo reports; filled in by the machine. */
#define HVM_NUM_INFO HVM_INFO_MAX

struct HexagonHVMState {
    SysBusDevice parent_obj;

    /* vmgetinfo answers, set by the machine. */
    uint32_t info[HVM_NUM_INFO];

    /* Virtual CPUs the guest has started (bit n is hardware thread n). */
    uint32_t started;
    /* Virtual CPUs that are in vmwait. */
    uint32_t waiting;
    uint32_t stop_reason;
    unsigned num_cpus;

    CPUState *cpus[HVM_MAX_VCPUS];

    /* Per-CPU interrupts */
    uint16_t cpuint_pending[HVM_MAX_VCPUS];
    uint16_t cpuint_enabled[HVM_MAX_VCPUS];

    /* Shared interrupts, indexed by interrupt number - HVM_PERCPU_INTS. */
    uint32_t pending[HVM_SHARED_INTS / 32];
    uint32_t enable[HVM_SHARED_INTS / 32];
    /* The device line is allowed to post: the guest has handled the last. */
    uint32_t armed[HVM_SHARED_INTS / 32];
    /* The level the device drives the line at. */
    uint32_t level[HVM_SHARED_INTS / 32];
    uint32_t percpu_mask[HVM_MAX_VCPUS][HVM_SHARED_INTS / 32];

    /* vmtimerop: absolute timeouts in timer ticks, 0 for none. */
    QEMUTimer *timer;
    uint64_t timeout[HVM_MAX_VCPUS];
    /* vmsettime state */
    int64_t cycle_offset;
    uint64_t pktcount;
};

/* All of these are called with the BQL held. */

/* Register the CPU that is hardware thread @idx. */
void hexagon_hvm_add_cpu(HexagonHVMState *s, CPUState *cs, unsigned idx);

/* vmintop.  @cpu is the calling virtual CPU's index. */
int32_t hexagon_hvm_intop(HexagonHVMState *s, unsigned cpu, uint32_t op,
                          uint32_t intno, uint32_t arg);

/* The interrupt vmintop(get) would take, or -1; peek does not take it. */
int32_t hexagon_hvm_int_get(HexagonHVMState *s, unsigned cpu);
int32_t hexagon_hvm_int_peek(HexagonHVMState *s, unsigned cpu);

/* Virtual CPU waits (vmwait) or stops waiting. */
void hexagon_hvm_set_waiting(HexagonHVMState *s, unsigned cpu, bool waiting);

/* vmtimerop; times cross the interface in nanoseconds of timer ticks. */
uint64_t hexagon_hvm_timerop(HexagonHVMState *s, unsigned cpu, uint32_t op,
                             uint64_t arg);
/* vmgettime / vmsettime */
uint64_t hexagon_hvm_gettime(HexagonHVMState *s, uint64_t *pktcount);
void hexagon_hvm_settime(HexagonHVMState *s, uint64_t time, uint64_t pktcount);

/*
 * A virtual CPU is created (vmstart): claim the lowest hardware thread that
 * has none, or return -1.  It goes away with hexagon_hvm_cpu_stopped().
 */
int hexagon_hvm_alloc_cpu(HexagonHVMState *s);
/* Returns true if this was the last running virtual CPU. */
bool hexagon_hvm_cpu_stopped(HexagonHVMState *s, unsigned cpu,
                             uint32_t status);

uint32_t hexagon_hvm_getinfo(HexagonHVMState *s, uint32_t code);

/* Notify a CPU that it has something to look at. */
void hexagon_hvm_kick(HexagonHVMState *s, unsigned cpu);

#endif /* HW_HEXAGON_HVM_H */
