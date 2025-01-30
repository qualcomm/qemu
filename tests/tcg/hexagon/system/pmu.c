/*
 *  Copyright(c) 2023-2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <inttypes.h>
#include <hexagon_standalone.h>
#include "thread_common.h"

static int err;
#include "pmu.h"

#define TOLERANCE 0.1
#define ERR (1 + TOLERANCE)

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
