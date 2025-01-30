/*
 *  Copyright(c) 2022-2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#define DEBUG 0

#include "hexagon_standalone.h"
#include "interrupts.h"
#include "util.h"
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


static const int MAX_INT_NUM = 8;
static const int ALL_INTERRUPTS_MASK = 0xff;

static inline void k0lock(void)
{
    asm volatile("k0lock\n");
}

static inline void wait(void)
{
    asm volatile("wait(r0)\n");
}

static inline void k0unlock(void)
{
    asm volatile("k0unlock\n");
}

static int buf[32 * 1024];
static int isr_buf[ARRAY_SIZE(buf)];
static void work()
{
    memcpy(buf, isr_buf, sizeof(buf));
}
static void isr_work()
{
    memcpy(isr_buf, buf, sizeof(isr_buf));
}


#define COMPUTE_THREADS           3
#define STACK_SIZE            16384
char stack[COMPUTE_THREADS][STACK_SIZE] __attribute__((__aligned__(8)));

static volatile int threads_started; /* used by different hw threads */
static const int ROUNDS = 10;

static volatile int thread_exit_flag; /* used by different hw threads */
static void thread_func(void *arg)
{
    int taskId = (int) arg;
    set_thread_imask(0);
    __atomic_fetch_add(&threads_started, 1, __ATOMIC_SEQ_CST);
    unsigned long long i = 0;
    while (thread_exit_flag == 0) {
        k0lock();
        pcycle_pause(100);
        k0unlock();
#if DEBUG == 1
        printf("thread[%d] round %llu\n", taskId, ++i);
#endif
    }
    set_thread_imask(ALL_INTERRUPTS_MASK);
    printf("thread exiting %d\n", taskId);
}

static volatile int ints_handled; /* used by different hw threads */
static void interrupt_handler(int intno)
{
    k0lock();
    isr_work();
    k0unlock();
    __atomic_fetch_add(&ints_handled, 1, __ATOMIC_SEQ_CST);
}

int main(int argc, char *argv[])
{
    for (int i = 0; i < MAX_INT_NUM; i++) {
        register_interrupt(i, interrupt_handler);
    }
    set_thread_imask(ALL_INTERRUPTS_MASK);
    memset(buf, 0xa5, sizeof(buf));

    puts("Testing pend/wake/wait");
    for (int i = 0; i < COMPUTE_THREADS; i++) {
        thread_create((void *)thread_func, &stack[i][ARRAY_SIZE(stack[i]) - 8],
                      i + 1, (void *)(i + 1));
    }

    while (threads_started < COMPUTE_THREADS) {
        printf("Threads started: %d\n", threads_started);
        pcycle_pause(10000);
    }
    printf("\tnow threads started: %d\n", threads_started);

    for (int i = 0; i < ROUNDS; i++) {
        int ints_expected = ints_handled + 1;
#if DEBUG == 1
        printf("Round %d/%d/%d, doing work for each int: expecting %d\n", i,
               ROUNDS, MAX_INT_NUM, ints_expected);
#endif
        for (int j = 0; j < MAX_INT_NUM; j++) {
            work();
            while (ints_handled < ints_expected) {
                swi(1 << j);
                pcycle_pause(10000);
            }
#if DEBUG == 1
            printf("done: Round %d: %d vs %d\n", i, ints_handled,
                   ints_expected);
#endif
        }
    }

    __atomic_fetch_add(&thread_exit_flag, 1, __ATOMIC_SEQ_CST);
    puts("waiting for threads...");
    for (int i = 0; i < ROUNDS; i++) {
        for (int j = 0; j < MAX_INT_NUM; j++) {
            pcycle_pause(10000);
            swi(j);
            swi(MAX_INT_NUM - j);
        }
    }

    puts("ints exhausted...");
    thread_join(((1 << COMPUTE_THREADS) - 1) << 1);

    printf("PASS\n");
    return 0;
}
