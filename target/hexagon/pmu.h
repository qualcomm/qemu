/*
 * Hexagon PMU event definitions
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef HEXAGON_TARGET_PMU_H
#define HEXAGON_TARGET_PMU_H

#include "hex_regs.h"

/*
 * PMU event codes, from the Hexagon V81 Programmer's Reference Manual's
 * PMU/HVX-PMU event tables. QEMU is a functional simulator: it has no
 * cache, branch-predictor, bus or timing model, so only events derivable
 * from packet decode/commit are implemented here. Every other event code
 * a guest selects via PMUEVTCFG/PMUEVTCFG1/PMUCFG is accepted (the counter
 * keeps counting nothing beyond its rebased offset) but logged once via
 * LOG_UNIMP rather than silently claiming support.
 *
 * HEX_PMU_IMPLEMENTED_EVENTS is the subset hexagon_pmu_event_stats() (in
 * cpu_helper.c) actually counts -- hexagon_pmu_event_implemented() checks
 * membership in *that* list, so a named-but-not-counted event (e.g. the
 * CYCLES_*_HVX_CONTEXTS_RUNNING family below) still logs as unimplemented
 * rather than silently reading 0 forever.
 */
enum {
    PMU_NO_EVENT = 0x000,
    COMMITTED_PKT_ANY = 0x003,
    COMMITTED_PKT_T0 = 0x00c,
    COMMITTED_PKT_T1 = 0x00d,
    COMMITTED_PKT_T2 = 0x00e,
    COMMITTED_PKT_T3 = 0x00f,
    COMMITTED_PKT_T4 = 0x010,
    COMMITTED_PKT_T5 = 0x011,
    COMMITTED_PKT_T6 = 0x015,
    COMMITTED_PKT_T7 = 0x016,
    COMMITTED_LD = 0x030,
    COMMITTED_ST = 0x031,
    COMMITTED_MEMOP = 0x032,
    HVX_PKT = 0x111,
    HVX_PKT_THREAD = 0x112,
    HVXPIPE_ALU = 0x128,
    HVXPIPE_MPY = 0x129,
    HVXPIPE_SHIFT = 0x12a,
    HVXPIPE_PERM = 0x12b,
    CYCLES_1_HVX_CONTEXTS_RUNNING = 0x115,
    CYCLES_2_HVX_CONTEXTS_RUNNING = 0x116,
    CYCLES_3_HVX_CONTEXTS_RUNNING = 0x117,
    CYCLES_4_HVX_CONTEXTS_RUNNING = 0x12c,
    CYCLES_5_HVX_CONTEXTS_RUNNING = 0x191,
    CYCLES_6_HVX_CONTEXTS_RUNNING = 0x192,
};

/*
 * The CYCLES_*_HVX_CONTEXTS_RUNNING family is deliberately excluded: an
 * accurate count needs a cycle-level model of concurrently-running HVX
 * contexts, which an unsynchronized MTTCG implementation can't give a
 * meaningful answer for. Left as future work rather than shipped as an
 * approximation.
 */
#define HEX_PMU_IMPLEMENTED_EVENTS \
    DECL_PMU_EVENT(PMU_NO_EVENT,       0x000) \
    DECL_PMU_EVENT(COMMITTED_PKT_ANY,  0x003) \
    DECL_PMU_EVENT(COMMITTED_PKT_T0,   0x00c) \
    DECL_PMU_EVENT(COMMITTED_PKT_T1,   0x00d) \
    DECL_PMU_EVENT(COMMITTED_PKT_T2,   0x00e) \
    DECL_PMU_EVENT(COMMITTED_PKT_T3,   0x00f) \
    DECL_PMU_EVENT(COMMITTED_PKT_T4,   0x010) \
    DECL_PMU_EVENT(COMMITTED_PKT_T5,   0x011) \
    DECL_PMU_EVENT(COMMITTED_PKT_T6,   0x015) \
    DECL_PMU_EVENT(COMMITTED_PKT_T7,   0x016) \
    DECL_PMU_EVENT(COMMITTED_LD,       0x030) \
    DECL_PMU_EVENT(COMMITTED_ST,       0x031) \
    DECL_PMU_EVENT(COMMITTED_MEMOP,    0x032) \
    DECL_PMU_EVENT(HVX_PKT,            0x111) \
    DECL_PMU_EVENT(HVX_PKT_THREAD,     0x112) \
    DECL_PMU_EVENT(HVXPIPE_ALU,        0x128) \
    DECL_PMU_EVENT(HVXPIPE_MPY,        0x129) \
    DECL_PMU_EVENT(HVXPIPE_SHIFT,      0x12a) \
    DECL_PMU_EVENT(HVXPIPE_PERM,       0x12b)

#define NUM_PMU_CTRS 8

/*
 * PMU sregs are defined in this order: 4, 5, 6, 7, 0, 1, 2, 3.
 * The gregs are ordered the same way.
 */
#define IS_PMU_CNT_SREG(REG) \
    ((REG) >= HEX_SREG_PMUCNT4 && (REG) <= HEX_SREG_PMUCNT3)
#define IS_PMU_CFG_SREG(REG) \
    ((REG) == HEX_SREG_PMUEVTCFG || (REG) == HEX_SREG_PMUEVTCFG1 || \
     (REG) == HEX_SREG_PMUCFG || (REG) == HEX_SREG_PMUSTID0 || \
     (REG) == HEX_SREG_PMUSTID1)
#define IS_PMU_SREG(REG) (IS_PMU_CNT_SREG(REG) || IS_PMU_CFG_SREG(REG))

#ifndef CONFIG_USER_ONLY
#define IS_PMU_GREG(REG) \
    (((REG) >= HEX_GREG_GPMUCNT4 && (REG) <= HEX_GREG_GPMUCNT7) || \
     ((REG) >= HEX_GREG_GPMUCNT0 && (REG) <= HEX_GREG_GPMUCNT3))

static inline unsigned int pmu_index_from_sreg(int reg)
{
    g_assert(IS_PMU_CNT_SREG(reg));
    if (reg >= HEX_SREG_PMUCNT0) {
        return reg - HEX_SREG_PMUCNT0;
    }
    return 4 + reg - HEX_SREG_PMUCNT4;
}

static inline unsigned int pmu_index_from_greg(int reg)
{
    g_assert(IS_PMU_GREG(reg));
    if (reg >= HEX_GREG_GPMUCNT0) {
        return reg - HEX_GREG_GPMUCNT0;
    }
    return 4 + reg - HEX_GREG_GPMUCNT4;
}

static inline int pmu_committed_pkt_thread(int event)
{
    /* Note that the COMMITTED_PKT_T* event numbers are not contiguous. */
    return event < COMMITTED_PKT_T6 ?
           event - COMMITTED_PKT_T0 :
           6 + event - COMMITTED_PKT_T6;
}
#endif /* !CONFIG_USER_ONLY */

#endif /* HEXAGON_TARGET_PMU_H */
