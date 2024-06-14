/*
 *  Copyright(c) 2023 Qualcomm Innovation Center, Inc. All Rights Reserved.
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

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <inttypes.h>
#include <hexagon_standalone.h>
#include "thread_common.h"


#define TOLERANCE 0.1
#define ERR (1 + TOLERANCE)

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

static int err;

enum regtype {
    SREG,
    GREG,
    CREG,
};

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

#define check_range(regnum, regtype, lo, hi) \
    __check_val_range(get_counter(regnum, regtype), regnum, regtype, lo, hi, __LINE__)

#define check(regnum, regtype, exp) \
   __check_val(get_counter(regnum, regtype), regnum, regtype, exp, __LINE__)

#define check_val_range(val, regnum, regtype, lo, hi) \
    __check_val_range(val, regnum, regtype, lo, hi, __LINE__)

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
            : "r0", "r2", "r3" );
    } else {
        asm volatile(
            "r0 = pmuevtcfg1\n"
            "r2 = %0\n" /*off*/
            "r3 = #8\n" /*width*/
            "r0 = insert(%1, r3:2)\n"
            "pmuevtcfg1 = r0\n"
            :
            : "r"(off), "r"(event)
            : "r0", "r2", "r3" );
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

#define NUM_THREADS 8
#define STACK_SIZE 0x8000
char __attribute__((aligned(16))) stack[NUM_THREADS - 1][STACK_SIZE];

#define RUN_N_PACKETS(N) \
    asm volatile("   loop0(1f, %0)\n" \
                 "1: { nop }:endloop0\n" \
                 : : "r"(N))

#define BASE_WORK_COUNT 100
static void work(void *id)
{
    pmu_start();
    RUN_N_PACKETS(((int)id + 1) * BASE_WORK_COUNT);
    pmu_stop();
}

static void test_config_with_pmu_enabled(int start_offset)
{
    pmu_reset();

    pmu_set_counters(start_offset);

    pmu_start();
    for (int i = 0; i < 800; i++) {
        asm volatile("nop");
    }
    pmu_config(0, COMMITTED_PKT_T0);
    pmu_stop();

    check_range(0, SREG, start_offset, start_offset + 15);
}

static void test_threaded_pkt_count(enum regtype type, bool set_ssr_pe)
{
    pmu_reset();

    pmu_config(1, COMMITTED_PKT_T1);
    pmu_config(2, COMMITTED_PKT_T2);
    pmu_config(3, COMMITTED_PKT_T3);
    pmu_config(4, COMMITTED_PKT_T4);
    pmu_config(5, COMMITTED_PKT_T5);
    pmu_config(6, COMMITTED_PKT_T6);
    pmu_config(7, COMMITTED_PKT_T7);

    for (int i = 1; i < NUM_THREADS; i++) {
        thread_run_blocked(work, (void *)&stack[i - 1][STACK_SIZE - 16], i,
                           (void *)i);
    }
    pmu_config(0, COMMITTED_PKT_T0);
    work(0);

    toggle_ssr_pe(set_ssr_pe);
    for (int i = 0; i < NUM_PMU_CTRS; i++) {
        if (type != SREG && !set_ssr_pe) {
            check_range(i, type, 0, 0);
        } else {
            check_range(i, type, BASE_WORK_COUNT * (i + 1),
                                 BASE_WORK_COUNT * (i + 1) * ERR);
        }
    }
}

#define GET_PMU_BY_PAIRS(V, P1, P2, P3, P4) \
        asm volatile( \
            "r1:0 = " P1 "\n" \
            "r3:2 = " P2 "\n" \
            "r5:4 = " P3 "\n" \
            "r7:6 = " P4 "\n" \
            "%0 = r0\n" \
            "%1 = r1\n" \
            "%2 = r2\n" \
            "%3 = r3\n" \
            "%4 = r4\n" \
            "%5 = r5\n" \
            "%6 = r6\n" \
            "%7 = r7\n" \
            : "=r"(V[0]), "=r"(V[1]), "=r"(V[2]), "=r"(V[3]), "=r"(V[4]), \
              "=r"(V[5]), "=r"(V[6]), "=r"(V[7]) \
            : \
            : "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7")

static void test_paired_access(enum regtype type, bool set_ssr_pe)
{
    uint32_t v[NUM_PMU_CTRS];
    pmu_reset();

    pmu_config(1, COMMITTED_PKT_T1);
    pmu_config(2, COMMITTED_PKT_T2);
    pmu_config(3, COMMITTED_PKT_T3);
    pmu_config(4, COMMITTED_PKT_T4);
    pmu_config(5, COMMITTED_PKT_T5);
    pmu_config(6, COMMITTED_PKT_T6);
    pmu_config(7, COMMITTED_PKT_T7);

    /*
     * Set pmucnt0, 1, ... respectively to 0, 1000, 0, 1000, ...
     * Note: we don't want to condition the assignment on `type` as
     * guest writes should be ignored.
     */
    asm volatile(
        "r0 = #0\n"
        "r1 = #1000\n"
        "s49:48 = r1:0\n"
        "s51:50 = r1:0\n"
        "s45:44 = r1:0\n"
        "s47:46 = r1:0\n"
        : : : "r0", "r1");

    for (int i = 1; i < NUM_THREADS; i++) {
        thread_run_blocked(work, (void *)&stack[i - 1][STACK_SIZE - 16], i,
                           (void *)i);
    }
    pmu_config(0, COMMITTED_PKT_T0);
    work(0);

    if (type == GREG) {
        toggle_ssr_pe(set_ssr_pe);
        GET_PMU_BY_PAIRS(v, "g27:26", "g29:28", "g17:16", "g19:18");
    } else if (type == CREG) {
        toggle_ssr_pe(set_ssr_pe);
        GET_PMU_BY_PAIRS(v, "c21:20", "c23:22", "c25:24", "c27:26");
    } else {
        GET_PMU_BY_PAIRS(v, "s49:48", "s51:50", "s45:44", "s47:46");
    }

    for (int i = 0; i < NUM_PMU_CTRS; i++) {
        if (type != SREG && !set_ssr_pe) {
            check_range(i, type, 0, 0);
        } else {
            int off = i % 2 ? 1000 : 0;
            check_val_range(v[i], i, type,
                            off + BASE_WORK_COUNT * (i + 1),
                            off + BASE_WORK_COUNT * (i + 1) * ERR);
        }
    }
}

static void test_gpmucnt(void)
{
    /* gpmucnt should be 0 if SSR:PE is 0 */
    pmu_reset();
    test_threaded_pkt_count(GREG, false);
    pmu_reset();
    test_paired_access(GREG, false);

    /* gpmucnt should alias the sys pmucnts if SSR:PE is 1 */
    pmu_reset();
    test_threaded_pkt_count(GREG, true);
    pmu_reset();
    test_paired_access(GREG, true);

    /* gpmucnt writes should be ignored. */
    toggle_ssr_pe(1);
    pmu_reset();
    asm volatile(
        "r0 = #2\n"
        "gpmucnt0 = r0\n"
        "gpmucnt1 = r0\n"
        "gpmucnt2 = r0\n"
        "gpmucnt3 = r0\n"
        "gpmucnt4 = r0\n"
        "gpmucnt5 = r0\n"
        "gpmucnt6 = r0\n"
        "gpmucnt7 = r0\n"
        : : : "r0");
    for (int i = 0; i < NUM_PMU_CTRS; i++) {
        check_range(i, GREG, 0, 0);
    }
}

static void test_upmucnt(void)
{
    /* upmucnt should be 0 if SSR:PE is 0 */
    pmu_reset();
    test_threaded_pkt_count(CREG, false);
    pmu_reset();
    test_paired_access(CREG, false);

    /* gpmucnt should alias the sys pmucnts if SSR:PE is 1 */
    pmu_reset();
    test_threaded_pkt_count(CREG, true);
    pmu_reset();
    test_paired_access(CREG, true);

    /* gpmucnt writes should be ignored. */
    toggle_ssr_pe(1);
    pmu_reset();

    /* The compiler prevents this, so we write the word directly. */
    asm volatile(
        "r0 = #2\n"
        ".word 0x6220c014\n" /* c20 = r0 */
        ".word 0x6220c015\n" /* c21 = r0 */
        ".word 0x6220c016\n" /* c22 = r0 */
        ".word 0x6220c017\n" /* c23 = r0 */
        ".word 0x6220c018\n" /* c24 = r0 */
        ".word 0x6220c019\n" /* c25 = r0 */
        ".word 0x6220c01a\n" /* c26 = r0 */
        ".word 0x6220c01b\n" /* c27 = r0 */
        : : : "r0");
    for (int i = 0; i < NUM_PMU_CTRS; i++) {
        check_range(i, CREG, 0, 0);
    }
}

static void config_thread(void *_)
{
    pmu_set_counters(100);
    pmu_config(0, COMMITTED_PKT_T0);
    pmu_start();
}

static void test_config_from_another_thread(void)
{
    pmu_reset();
    thread_run_blocked(config_thread, (void *)&stack[0][STACK_SIZE - 16], 1,
                       NULL);
    pmu_stop();
    check_range(0, SREG, 100, 100000); /* We just want to check >= 100, really */
}

static void test_hvx_packets(void)
{
    pmu_reset();
    pmu_config(0, HVX_PKT);
    pmu_start();
    asm volatile(
        "nop\n"
        "nop\n"
        "{ v0 = vrmpyb(v0, v1); v2 = vrmpyb(v3, v4) }\n"
        "{ v0 = vrmpyb(v0, v1); nop; }\n"
        : : : "v0", "v2");
    check(0, SREG, 2);
    pmu_stop();

}

static void test_event_change(void)
{
    const int initial_offset = 500;
    uint32_t expect_count;

    pmu_reset();
    PMU_SET_COUNTER(0, initial_offset);
    expect_count = initial_offset;

    pmu_config(0, COMMITTED_PKT_T0);
    work(0);
    expect_count += BASE_WORK_COUNT;

    pmu_config(0, COMMITTED_PKT_T1);
    thread_run_blocked(work, (void *)&stack[0][STACK_SIZE - 16], 1, (void *)0);
    expect_count += BASE_WORK_COUNT;

    pmu_config(0, COMMITTED_PKT_T0);
    work(0);
    expect_count += BASE_WORK_COUNT;

    check_range(0, SREG, expect_count, expect_count * ERR);
}

static void test_committed_pkt_any(void)
{
    uint32_t expect_count;
    pmu_reset();
    pmu_config(0, COMMITTED_PKT_ANY);
    pmu_config(1, COMMITTED_PKT_T0);
    pmu_config(2, COMMITTED_PKT_T1);
    thread_run_blocked(work, (void *)&stack[0][STACK_SIZE - 16], 1, (void *)1);
    work(0);
    expect_count = get_pmu_counter(1) + get_pmu_counter(2);
    check_range(0, SREG, expect_count, expect_count * ERR);
}

int main()
{
    test_config_with_pmu_enabled(0);
    test_config_with_pmu_enabled(100);
    test_threaded_pkt_count(SREG, false);
    test_paired_access(SREG, false);
    test_gpmucnt();
    test_upmucnt();
    test_config_from_another_thread();
    test_hvx_packets();
    test_event_change();
    test_committed_pkt_any();

    printf("%s\n", ((err) ? "FAIL" : "PASS"));
    return err;
}
