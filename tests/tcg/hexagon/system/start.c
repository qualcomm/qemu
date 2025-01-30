/*
 *  Copyright(c) 2023-2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 * This program tests the Hexagon start instruction.
 * We test the instruction by intstalling our own reset handler.
 * Inside this handler,
 *     set a bit in reset_mask to indicate the thread was reset
 *     log the cause field in SSR register
 *     stop the thread
 *
 * The start instruction should reset all threads in the argument
 * except the current thread.  We check that the cause field in SSR
 * is preserved in the current thread and reset in the others in the
 * mask.
 */

#include "thread_common.h"
#include <hexagon_standalone.h>
#include <stdio.h>
#include <stdlib.h>

int err;
#include "../hex_test.h"


/* Mark volatile because it is modified by multiple threads */
static volatile uint32_t reset_mask;

void (*default_reset_handler)(void);

#define NUM_THREADS 4
#define DUMMY_CAUSE 0x13
/* Mark volatile because it is modified by multiple threads */
static volatile uint32_t ssr_cause_log[NUM_THREADS];

#define get_ssr_cause()                         \
    ({                                          \
        uint32_t ssr;                           \
        asm volatile("%0 = ssr\n" : "=r"(ssr)); \
        ssr & 0xff;                             \
    })

#define set_ssr_cause(CAUSE)                       \
    do {                                           \
        uint32_t ssr;                              \
        asm volatile("%0 = ssr\n\t" : "=r"(ssr));  \
        ssr &= ~0xff;                              \
        ssr |= ((CAUSE) & 0xff);                   \
        asm volatile("ssr = %0\n\t" : : "r"(ssr)); \
    } while (0)

/*
 * ATTENTION: this function must be kept as simple as possible and it CANNOT
 * call other non-inlined functions, as we are overriding the default reset
 * handler from crt0, so we do not get the basic setup step like stack
 * allocation.
 */
void my_reset_handler(void)
{
    uint32_t htid = get_htid();

    wait_on_semaphore();

    /* Log the SSR cause field */
    ssr_cause_log[htid] = get_ssr_cause();

    /* Atomically set the htid bit in the mask */
    asm volatile("k0lock\n");
    reset_mask |= (1 << htid);
    asm volatile("k0unlock\n");

    asm volatile("stop(r0)\n");
}

static void test_start(uint32_t mask)
{
    uint32_t htid = get_htid();

    reset_mask = 0;
    for (int i = 0; i < NUM_THREADS; i++) {
        ssr_cause_log[i] = DUMMY_CAUSE;
    }
    set_ssr_cause(0x15);

    set_event_handler(HEXAGON_EVENT_0, my_reset_handler);
    set_semaphore_state(mask, THREAD_SEMAPHORE_OFF);
    asm volatile("start(%0)\n\t" : : "r"(mask));
    start_waiting_threads(mask);
    thread_join(remove_myself(mask));
    set_event_handler(HEXAGON_EVENT_0, default_reset_handler);

    check32(get_ssr_cause(), 0x15);
    check32(reset_mask, remove_myself(mask));
    for (int i = 0; i < NUM_THREADS; i++) {
        if (i != htid) {
            if (mask & (1 << i)) {
                check32(ssr_cause_log[i], 0);
            } else {
                check32(ssr_cause_log[i], DUMMY_CAUSE);
            }
        }
    }
}

#define STACK_SIZE 0x8000
char __attribute__((aligned(16))) thread_stack[STACK_SIZE];

static void thread_func(void *x)
{
    test_start(0x2);
    test_start(0x6);
    test_start(0xe);
}

int main()
{
    puts("Hexagon start instruction test");

    default_reset_handler = (void (*)(void))get_event_handler(HEXAGON_EVENT_0);

    /* Run the tests on thread 0 */
    thread_func(NULL);

    /* Run the tests on other threads */
    for (int i = 1; i < NUM_THREADS; i++) {
        thread_run_blocked(thread_func, (void *)&thread_stack[STACK_SIZE - 16],
                           i, NULL);
    }
    printf("%s\n", ((err) ? "FAIL" : "PASS"));
    return err;
}
