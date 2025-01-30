/*
 *  Copyright(c) 2023-2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */


#include "thread_common.h"
#include <hexagon_standalone.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>


#define NUM_THREADS 2
#define STACK_SIZE 0x8000
char __attribute__((aligned(16))) stack[NUM_THREADS][STACK_SIZE];
int id[NUM_THREADS + 1];
volatile int
    done[NUM_THREADS + 1]; /* volatile because threads will change it */
volatile int
    contention[NUM_THREADS + 1]; /* volatile because threads will change it */

static const int LOOP_CNT = 10000;
volatile uint32_t tick32; /* volatile because we are testing atomics */
volatile uint64_t tick64; /* volatile because we are testing atomics */

#define DECLARE_ATOMIC_INC(SIZE, TYPE, SUFFIX)                                \
    /* Using volatile because we are testing atomics */                       \
    static inline TYPE atomic_inc##SIZE(volatile TYPE *x, TYPE increment)     \
    {                                                                         \
        TYPE old, dummy;                                                      \
        uint32_t iters = 0;                                                   \
        asm volatile(                                                         \
            "1: %0 = mem" SUFFIX "_locked(%3)\n\t"                            \
            "   %1 = add(%0, %4)\n\t"                                         \
            "   %2 = add(%2, #1)\n\t"                                         \
            /* This pause makes the thread yield in single-thread TCG. */     \
            "   pause(#0)\n\t"                                                \
            "   mem" SUFFIX "_locked(%3, p0) = %1\n\t"                        \
            "   if (!p0) jump 1b\n\t"                                         \
            : "=&r"(old), "=&r"(dummy), "+&r"(iters)                          \
            : "r"(x), "r"(increment)                                          \
            : "p0", "memory");                                                \
        return (TYPE)iters;                                                   \
    }

/*
 * This covers the case where a store-conditional targets a virtual address
 * other than the current load-locked address.
 */
#define DECLARE_ATOMIC_INC_MISMATCH(SIZE, TYPE, SUFFIX)             \
    /* Using volatile because we are testing atomics */             \
    static inline int atomic_inc##SIZE##_mismatch(volatile TYPE *x, \
                                                  volatile TYPE *y) \
    {                                                               \
        TYPE old, dummy = 0;                                        \
        int pred;                                                   \
        asm volatile("%0 = mem" SUFFIX "_locked(%3)\n\t"            \
                     "mem" SUFFIX "_locked(%4, p0) = %2\n\t"        \
                     "%1 = p0\n\t"                                  \
                     : "=&r"(old), "=&r"(pred)                      \
                     : "r"(dummy), "r"(x), "r"(y)                   \
                     : "p0", "memory");                             \
        return pred;                                                \
    }

#define DECLARE_SAT_ADD(SIZE, TYPE)                                   \
    static inline void sat_add##SIZE(TYPE *a, TYPE b)                 \
    {                                                                 \
        TYPE result = *a + b;                                         \
        *a = (result < *a || result < b) ? UINT##SIZE##_MAX : result; \
    }

#define TYPE(SIZE) uint##SIZE##_t

#define DECLARE_AUX_FUNCS(SIZE, SUFFIX)                   \
    DECLARE_ATOMIC_INC(SIZE, TYPE(SIZE), SUFFIX)          \
    DECLARE_ATOMIC_INC_MISMATCH(SIZE, TYPE(SIZE), SUFFIX) \
    DECLARE_SAT_ADD(SIZE, TYPE(SIZE))

DECLARE_AUX_FUNCS(32, "w")
DECLARE_AUX_FUNCS(64, "d")

int thread_body(int id)
{
    /*
     * These counters are to track how many times an atomic_incZZ()
     * had to iterate because the store-conditional returned 'false'.
     */
    uint32_t tick32_iters = 0;
    uint64_t tick64_iters = 0;
    static const int EXPECTED_IGNORED = 99;
    uint32_t ignored32 = EXPECTED_IGNORED;
    uint64_t ignored64 = EXPECTED_IGNORED;
    for (int i = 0; i < LOOP_CNT; i++) {
        uint32_t inc = id * (i % 5);

        sat_add32(&tick32_iters, atomic_inc32(&tick32, inc));
        sat_add64(&tick64_iters, atomic_inc64(&tick64, inc));

        /* For each iteration through the loop, we'll try to load-locked
         * 'ignored32/64' and store-conditional 'tick32/64'.  'ignored'
         * should never change, the predicate value from the store-cond
         * should never be true.  If tick32 is modified, it will be
         * detected by other assertions.
         */
        bool pred_true = atomic_inc32_mismatch(&tick32, &ignored32);
        assert(!pred_true);
        assert(ignored32 == EXPECTED_IGNORED);

        pred_true = atomic_inc64_mismatch(&tick64, &ignored64);
        assert(!pred_true);
        assert(ignored64 == EXPECTED_IGNORED);
    }

    return (tick32_iters - LOOP_CNT) + (tick64_iters - LOOP_CNT);
}

void my_thread(void *y)
{
    const int id = *(int *)y;
    contention[id] = thread_body(id);
    done[id] = 1;
}

static inline bool all_done(void)
{
    for (int i = 1; i <= NUM_THREADS; i++) {
        if (!done[i]) {
            return false;
        }
    }
    return true;
}

#define RETRY -1
int run_test(void)
{
    unsigned int thread_mask = 0;
    int err = 0;

    tick32 = 1;
    tick64 = 1;

    for (int i = 1; i <= NUM_THREADS; i++) {
        id[i] = i;
        done[i] = 0;
        /* thread-create takes a HW thread number */
        create_waiting_thread(my_thread, &stack[i][STACK_SIZE - 16], i, &id[i]);
        thread_mask |= (1 << i);
    }

    start_waiting_threads(thread_mask);

    uint32_t prev_tick32 = tick32;
    uint64_t prev_tick64 = tick64;
    while (!all_done()) {
        /* The tick values must be monotonically increasing. */
        assert(prev_tick32 <= tick32);
        assert(prev_tick64 <= tick64);

        prev_tick32 = tick32;
        prev_tick64 = tick64;

        asm volatile("pause(#0)");
    }

    thread_join(thread_mask);

    for (int i = 1; i <= NUM_THREADS; i++) {
        /*
         * All threads must encounter some contention otherwise
         * the test is not valid.
         */
        if (contention[i] == 0) {
            return RETRY;
        }
    }

    uint32_t expected_tick_count = 1;
    for (int tid = 1; tid <= NUM_THREADS; tid++) {
        for (int i = 0; i < LOOP_CNT; i++) {
            expected_tick_count += tid * (i % 5);
        }
    }

    if (tick32 != expected_tick_count) {
        printf("ERROR: tick32 %" PRIu32 " != %" PRIu32 "\n", tick32,
               expected_tick_count);
        err++;
    }
    if (tick64 != expected_tick_count) {
        printf("ERROR: tick64 %" PRIu64 " != %" PRIu32 "\n", tick64,
               expected_tick_count);
        err++;
    }
    return err;
}

/*
 * When there is no thread contention, the test is not valid, as it didn't
 * really checked the atomic increments implementation. This happens
 * occasionally (maybe 1/1000 times?). Therefore, we allow the test to repeat
 * itself a few times. Note that a *real* failure does *not* trigger a retry.
 * Also, the test is quite fast, so repetition is not an issue.
 */
#define NUM_RETRIES 4

int main()
{
    puts("Hexagon atomics test");
    int err;
    for (int i = 1; i <= NUM_RETRIES; i++) {
        printf("%d try\n", i);
        err = run_test();
        if (err != RETRY) {
            break;
        }
    }
    printf("%s\n", ((err) ? "FAIL" : "PASS"));
    return err;
}
