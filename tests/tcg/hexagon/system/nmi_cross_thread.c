/*
 *  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <stdint.h>
#include <stdio.h>
#include <hexagon_standalone.h>

volatile int thread1_running = 0;

#define STACK_SZ 4096
static long long thread1_stack[STACK_SZ / sizeof(long long)];

static void nmi_handler(void)
{
    thread_stop();
}

static void thread1_fn(void *arg)
{
    (void)arg;
    thread1_running = 1;
    /*
     * Memory clobber keeps the branch in the loop so the compiler
     * cannot unroll or eliminate it.
     */
    while (1) {
        asm volatile("" : : : "memory");
    }
}

int main(void)
{
    set_event_handler(HEXAGON_EVENT_1, nmi_handler);

    thread_create(thread1_fn,
                  &thread1_stack[STACK_SZ / sizeof(long long) - 2],
                  1, NULL);

    while (!thread1_running) {
        asm volatile("pause(#1)\n" : : : "memory");
    }

    uint32_t mask = 0x2;
    asm volatile("nmi(%0)\n" : : "r"(mask));

    thread_join(0x2);

    puts("PASS");
    return 0;
}
