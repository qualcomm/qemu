/*
 *  Copyright(c) 2023-2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "thread_common.h"

#include <assert.h>
#include <hexagon_standalone.h>
#include <stdbool.h>
#include <stdlib.h>

volatile int thread_semaphore[32]; /* volatile: changed by multiple threads */

struct thread_work {
    void *param;
    void (*func)(void *);
};

static void thread_wrapper(void *p)
{
    struct thread_work *work = (struct thread_work *)p;
    wait_on_semaphore();
    work->func(work->param);
    free(work);
}

void create_waiting_thread(void (*func)(void *), void *sp, int tid, void *param)
{
    struct thread_work *work = malloc(sizeof(*work));
    assert(work);
    work->param = param;
    work->func = func;
    thread_semaphore[tid] = THREAD_SEMAPHORE_OFF;
    thread_create(thread_wrapper, sp, tid, work);
}

static void wait_semaphore_state(uint32_t mask, int state)
{
    /* Doesn't make sense to wait for myself */
    mask = remove_myself(mask);
    for (int tid = 0; tid < 32; tid++) {
        if (mask & (1 << tid)) {
            while (thread_semaphore[tid] != state) {
                asm volatile("pause(#1)\n");
            }
        }
    }
}

void set_semaphore_state(uint32_t mask, int state)
{
    for (int tid = 0; tid < 32; tid++) {
        if (mask & (1 << tid)) {
            thread_semaphore[tid] = state;
        }
    }
}

void start_waiting_threads(uint32_t mask)
{
    /* Check that the threads started */
    wait_semaphore_state(mask, THREAD_SEMAPHORE_ON_WAIT);

    /* Now let them run */
    set_semaphore_state(mask, THREAD_SEMAPHORE_GO);
}

void thread_create_blocked(void (*func)(void *), void *sp, int tid, void *param)
{
    create_waiting_thread(func, sp, tid, param);
    start_waiting_threads(1 << tid);
}

void thread_run_blocked(void (*func)(void *), void *sp, int tid, void *param)
{
    thread_create_blocked(func, sp, tid, param);
    thread_join(1 << tid);
}
