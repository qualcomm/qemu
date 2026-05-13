/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef HEXAGON_PMU_H
#define HEXAGON_PMU_H

#include "hex_regs.h"

#define NUM_PMU_CTRS 8

/* PMU event definitions */
enum {
    PMU_NO_EVENT         = 0x00,
    COMMITTED_PKT_ANY    = 0x01,
    HVX_PKT              = 0x20,
    COMMITTED_PKT_T0     = 0x40,
    COMMITTED_PKT_T1     = 0x41,
    COMMITTED_PKT_T2     = 0x42,
    COMMITTED_PKT_T3     = 0x43,
    COMMITTED_PKT_T4     = 0x44,
    COMMITTED_PKT_T5     = 0x45,
    COMMITTED_PKT_T6     = 0x46,
    COMMITTED_PKT_T7     = 0x47,
};

/* Register classification macros */
static inline bool is_pmu_sreg(uint32_t reg)
{
    return (reg >= HEX_SREG_PMUCNT4 && reg <= HEX_SREG_PMUCNT3) ||
           reg == HEX_SREG_PMUEVTCFG || reg == HEX_SREG_PMUEVTCFG1 ||
           reg == HEX_SREG_PMUCFG || reg == HEX_SREG_PMUSTID0 ||
           reg == HEX_SREG_PMUSTID1;
}

static inline bool is_pmu_creg(uint32_t reg)
{
    return (reg >= HEX_REG_UPMUCNT0 && reg <= HEX_REG_UPMUCNT7);
}

/*
 * Convert system register number to PMU counter index (0-7).
 * PMUCNT4-7 are at sreg 44-47, PMUCNT0-3 are at sreg 48-51.
 */
static inline int pmu_index_from_sreg(uint32_t reg)
{
    if (reg >= HEX_SREG_PMUCNT0 && reg <= HEX_SREG_PMUCNT3) {
        return reg - HEX_SREG_PMUCNT0;
    }
    return (reg - HEX_SREG_PMUCNT4) + 4;
}

/*
 * Convert user register number to PMU counter index (0-7).
 * UPMUCNT0-7 are cregs 52-59.
 */
static inline int pmu_index_from_creg(uint32_t reg)
{
    return reg - HEX_REG_UPMUCNT0;
}

#ifndef CONFIG_USER_ONLY

static inline bool is_pmu_greg(uint32_t reg)
{
    return (reg >= HEX_GREG_GPMUCNT4 && reg <= HEX_GREG_GPMUCNT7) ||
           (reg >= HEX_GREG_GPMUCNT0 && reg <= HEX_GREG_GPMUCNT3);
}

/*
 * Convert global register number to PMU counter index (0-7).
 * GPMUCNT0-3 are gregs 26-29, GPMUCNT4-7 are gregs 16-19.
 */
static inline int pmu_index_from_greg(uint32_t reg)
{
    if (reg >= HEX_GREG_GPMUCNT0 && reg <= HEX_GREG_GPMUCNT3) {
        return reg - HEX_GREG_GPMUCNT0;
    }
    return (reg - HEX_GREG_GPMUCNT4) + 4;
}

#endif /* !CONFIG_USER_ONLY */

/* Return the event code for COMMITTED_PKT_Tn for a given thread */
static inline int pmu_committed_pkt_thread(int thread_id)
{
    return COMMITTED_PKT_T0 + thread_id;
}

/* BQL locking helpers for PMU access */
#define pmu_lock()   BQL_LOCK_GUARD()
#define pmu_unlock() /* BQL_LOCK_GUARD handles unlock */

#endif /* HEXAGON_PMU_H */
