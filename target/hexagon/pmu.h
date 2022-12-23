/*
 *
 *  Copyright(c) 2022 Qualcomm Innovation Center, Inc. All Rights Reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef HEXAGON_PMU_H
#define HEXAGON_PMU_H

#include "hex_regs.h"

#define HEX_PMU_EVENTS \
    DECL_PMU_EVENT(PMU_NO_EVENT, 0) \
    DECL_PMU_EVENT(COMMITTED_PKT_T0, 12) \
    DECL_PMU_EVENT(COMMITTED_PKT_T1, 13) \
    DECL_PMU_EVENT(COMMITTED_PKT_T2, 14) \
    DECL_PMU_EVENT(COMMITTED_PKT_T3, 15) \
    DECL_PMU_EVENT(COMMITTED_PKT_T4, 16) \
    DECL_PMU_EVENT(COMMITTED_PKT_T5, 17) \
    DECL_PMU_EVENT(COMMITTED_PKT_T6, 21) \
    DECL_PMU_EVENT(COMMITTED_PKT_T7, 22) \
    DECL_PMU_EVENT(HVX_PKT, 273)

#define DECL_PMU_EVENT(name, val) name = val,
enum {
    HEX_PMU_EVENTS
};
#undef DECL_PMU_EVENT

/* PMU sregs are defined in this order: 4, 5, 6, 7, 0, 1, 2, 3. */
#define IS_PMU_REG(REG) ((REG) >= HEX_SREG_PMUCNT4 && (REG) <= HEX_SREG_PMUCNT3)
static inline unsigned int pmu_index_from_sreg(int reg)
{
    g_assert(IS_PMU_REG(reg));
    if (reg >= HEX_SREG_PMUCNT0) {
        return reg - HEX_SREG_PMUCNT0;
    }
    return 4 + reg - HEX_SREG_PMUCNT4;
}

static inline int pmu_committed_pkt_thread(int event)
{
    /* Note that the COMMITTED_PKT_T* event numbers are not contiguous. */
    return event < COMMITTED_PKT_T6 ?
           event - COMMITTED_PKT_T0 :
           6 + event - COMMITTED_PKT_T6;
}

#define pmu_lock() \
    bool __needs_pmu_lock = !qemu_mutex_iothread_locked(); \
    do { \
        if (__needs_pmu_lock) { \
            qemu_mutex_lock_iothread(); \
        } \
    } while (0)

#define pmu_unlock() \
    do { \
        if (__needs_pmu_lock) { \
            qemu_mutex_unlock_iothread(); \
        } \
    } while (0)

#endif /* HEXAGON_PMU_H */
