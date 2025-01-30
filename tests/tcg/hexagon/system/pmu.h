/*
 *  Copyright(c) 2024-2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef PMU_H
#define PMU_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>

#define NUM_PMU_CTRS 8

#define _DECLARE_GET_PMU_CASE(ID, BASE) \
    case ID: asm volatile("%0 = " #BASE #ID "\n" : "=r"(counter)); break

#define DECLARE_GET_PMU(NAME, BASE) \
    static uint32_t NAME(int index) \
    { \
        uint32_t counter; \
        switch (index) { \
        _DECLARE_GET_PMU_CASE(0, BASE); \
        _DECLARE_GET_PMU_CASE(1, BASE); \
        _DECLARE_GET_PMU_CASE(2, BASE); \
        _DECLARE_GET_PMU_CASE(3, BASE); \
        _DECLARE_GET_PMU_CASE(4, BASE); \
        _DECLARE_GET_PMU_CASE(5, BASE); \
        _DECLARE_GET_PMU_CASE(6, BASE); \
        _DECLARE_GET_PMU_CASE(7, BASE); \
        default: \
            printf("ERROR at line %d: invalid counter index %d\n", __LINE__, index); \
            abort(); \
        } \
        return counter; \
    }

DECLARE_GET_PMU(get_pmu_counter, pmucnt)
DECLARE_GET_PMU(get_gpmu_counter, gpmucnt)
/*
 * TODO: ideally, we would want to use upmucnt0, upmucnt1, ..., but the
 * compiler doesn't know these names yet, so we use c20, c21, ...
 */
DECLARE_GET_PMU(get_upmu_counter, c2)

enum regtype {
    SREG,
    GREG,
    CREG,
};

static inline uint32_t get_counter(int regnum, enum regtype type)
{
    switch (type) {
    case SREG: return get_pmu_counter(regnum);
    case GREG: return get_gpmu_counter(regnum);
    case CREG: return get_upmu_counter(regnum);
    }
    printf("unknown reg type %d\n", type);
    abort();
}


static void pmu_config(uint32_t counter_id, uint32_t event)
{
    uint32_t off = (counter_id % 4) * 8;
    /* First the 8 LSBs */
    if (counter_id < 4) {
        asm volatile(
            "r0 = pmuevtcfg\n"
            "r2 = %0\n" /*off*/
            "r3 = #8\n" /*width*/
            "r0 = insert(%1, r3:2)\n"
            "pmuevtcfg = r0\n"
            :
            : "r"(off), "r"(event)
            : "r0", "r2", "r3");
    } else {
        asm volatile(
            "r0 = pmuevtcfg1\n"
            "r2 = %0\n" /*off*/
            "r3 = #8\n" /*width*/
            "r0 = insert(%1, r3:2)\n"
            "pmuevtcfg1 = r0\n"
            :
            : "r"(off), "r"(event)
            : "r0", "r2", "r3");
    }
    /* Now the 2 MSBs */
    off = counter_id * 2;
    event >>= 8;
    asm volatile(
        "r0 = pmucfg\n"
        "r2 = %0\n" /*off*/
        "r3 = #2\n" /*width*/
        "r0 = insert(%1, r3:2)\n"
        "pmucfg = r0\n"
        :
        : "r"(off), "r"(event)
        : "r0", "r2", "r3");
}

static void pmu_set_counters(uint32_t val)
{
    asm volatile(
        "pmucnt0 = %0\n"
        "pmucnt1 = %0\n"
        "pmucnt2 = %0\n"
        "pmucnt3 = %0\n"
        "pmucnt4 = %0\n"
        "pmucnt5 = %0\n"
        "pmucnt6 = %0\n"
        "pmucnt7 = %0\n"
        : : "r"(val));
}

#define PMU_SET_COUNTER(IDX, VAL) do { \
        uint32_t reg = (VAL); \
        asm volatile("pmucnt" #IDX " = %0\n" : : "r"(reg)); \
    } while (0)

#define PM_SYSCFG_BIT 9
static inline void pmu_start(void)
{
    asm volatile(
        "r1 = syscfg\n"
        "r1 = setbit(r1, #%0)\n"
        "syscfg = r1\n"
        "isync\n"
        : : "i"(PM_SYSCFG_BIT) : "r1");
}

static inline void pmu_stop(void)
{
    asm volatile(
        "r1 = syscfg\n"
        "r1 = clrbit(r1, #%0)\n"
        "syscfg = r1\n"
        "isync\n"
        : : "i"(PM_SYSCFG_BIT) : "r1");
}

#define PE_SSR_BIT 24
static void toggle_ssr_pe(bool enable)
{
    if (enable) {
        asm volatile(
                "r0 = ssr\n"
                "r0 = setbit(r0, #%0)\n"
                "ssr = r0\n"
                : : "i"(PE_SSR_BIT) : "r0");
    } else {
        asm volatile(
                "r0 = ssr\n"
                "r0 = clrbit(r0, #%0)\n"
                "ssr = r0\n"
                : : "i"(PE_SSR_BIT) : "r0");
    }
}

static void pmu_reset(void)
{
    pmu_stop();
    pmu_set_counters(0);
    for (int i = 0; i < NUM_PMU_CTRS; i++) {
        pmu_config(i, 0);
    }
}


const char *regtype_to_str(enum regtype type)
{
    switch (type) {
    case SREG: return "sys";
    case GREG: return "greg";
    case CREG: return "upmucnt";
    }
    printf("unknown reg type %d\n", type);
    abort();
}

static inline void __check_val_range(uint32_t val,
                                     int regnum, enum regtype type,
                                     uint32_t lo, uint32_t hi,
                                     int line)
{
    if (val < lo || val > hi) {

        printf("ERROR at line %d: %s counter %u outside"
               " [%"PRIu32", %"PRIu32"] range (%"PRIu32")\n",
               line, regtype_to_str(type), regnum, lo, hi, val);
        err = 1;
    }
}

static inline void __check_val(uint32_t val, int regnum, enum regtype type,
                               uint32_t exp, int line)
{
    if (val != exp) {
        printf("ERROR at line %d: %s counter %u has value %"PRIu32", "
               "expected %"PRIu32"\n",
               line, regtype_to_str(type), regnum, val, exp);
        err = 1;
    }
}

#define check_range(regnum, regtype, lo, hi) \
    __check_val_range(get_counter(regnum, regtype), regnum, regtype, lo, hi, __LINE__)

#define check(regnum, regtype, exp) \
   __check_val(get_counter(regnum, regtype), regnum, regtype, exp, __LINE__)

#define check_val_range(val, regnum, regtype, lo, hi) \
    __check_val_range(val, regnum, regtype, lo, hi, __LINE__)

#define COMMITTED_PKT_ANY 3
#define COMMITTED_PKT_T0 12
#define COMMITTED_PKT_T1 13
#define COMMITTED_PKT_T2 14
#define COMMITTED_PKT_T3 15
#define COMMITTED_PKT_T4 16
#define COMMITTED_PKT_T5 17
#define COMMITTED_PKT_T6 21
#define COMMITTED_PKT_T7 22
#define HVX_PKT 273

#endif /* PMU_H */
