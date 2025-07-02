/*
 *  Copyright(c) 2023-2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "interrupts.h"
#include "util.h"
#include "thread_common.h"
#include <hexagon_standalone.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>


#define MAX_INT_NUM (8)
#define ALL_INTERRUPTS_MASK (0xff)

#define WAIT_THREAD_COUNT 3
#define TOTAL_THREAD_COUNT (1 + WAIT_THREAD_COUNT)
/* volatile because it tracks when interrupts have been processed */
volatile int ints_by_irq[MAX_INT_NUM]; /* volatile required here */
volatile int ints_by_tid[TOTAL_THREAD_COUNT]; /* volatile required here */

static bool all_ints_delivered(int n)
{
    bool all_delivered = true;
    for (int i = 0; i < MAX_INT_NUM; i++) {
        bool delivered = (ints_by_irq[i] == n);
        if (!delivered) {
            printf("ints_by_irq[%d] = %d, expected %d\n", i, ints_by_irq[i], n);
        }
        all_delivered = all_delivered && delivered;
    }
    return all_delivered;
}

uint32_t read_modectl(void)
{
    unsigned modectl;
    asm volatile("%0 = MODECTL\n\t" : "=r"(modectl));
    return modectl;
}

void wait_for_thread(int tid)
{
    unsigned tmode;
    do {
        unsigned modectl = read_modectl();
        tmode = modectl & (0x1 << (16 + tid));
    } while (tmode == 0);
}

static long long wait_stack_1[1024];
static long long wait_stack_2[1024];
static long long wait_stack_3[1024];
volatile bool tasks_enabled = true; /* multiple hw threads running */
volatile int times_woken[WAIT_THREAD_COUNT]; /* multiple hw threads running */
static void wait_thread(void *param)
{
    uint32_t thread_id = get_htid() - 1;
    set_task_prio(thread_id);
    while (tasks_enabled) {
        wait_for_interrupts();

        set_task_prio(get_task_prio() - 5);

        times_woken[thread_id]++;
    }
}

void wait_for_ints_delivered(int n)
{
    while (!all_ints_delivered(n)) {
        pcycle_pause(10000);
    }
    int times_woken_sum = 0;
    for (int i = 0; i < WAIT_THREAD_COUNT; i++) {
        times_woken_sum += times_woken[i];
    }
    /*
     * We can't use '==' because more than one interrupt may have been
     * processed in the same "wake window".
     */
    while (times_woken_sum < n) {
        pcycle_pause(10000);
    }
}

static void interrupt_handler(int intno)
{
    uint32_t thread_id = get_htid();
    asm volatile("k0lock\n");
    ints_by_irq[intno]++;
    ints_by_tid[thread_id]++;
    asm volatile("k0unlock\n");
}

void wait_for_wait(void)
{
    unsigned wait_mask, threads_mask = 0;
    for (int i = 1; i < TOTAL_THREAD_COUNT; i++) {
        threads_mask |= (1 << i);
    }
    do {
        wait_mask = (read_modectl() >> 16) & threads_mask;
        swi(0x1);
    } while (wait_mask != 0);
}


int main()
{
    for (int i = 0; i < MAX_INT_NUM; i++) {
        register_interrupt(i, interrupt_handler);
    }

    set_thread_imask(ALL_INTERRUPTS_MASK);

    thread_create_blocked(wait_thread,
            &wait_stack_1[ARRAY_SIZE(wait_stack_1) - 1], 1,
            NULL);
    thread_create_blocked(wait_thread,
            &wait_stack_2[ARRAY_SIZE(wait_stack_2) - 1], 2,
            NULL);
    thread_create_blocked(wait_thread,
            &wait_stack_3[ARRAY_SIZE(wait_stack_3) - 1], 3,
            NULL);
    /* make sure threads are up and in wait state before sending int */
    wait_for_thread(1);
    wait_for_thread(2);
    wait_for_thread(3);

    static int INT_MASK = ALL_INTERRUPTS_MASK;

    /* Test ordinary swi interrupts */
    swi(INT_MASK);
    printf("waiting for wake #1\n");
    wait_for_ints_delivered(1);

    /*
     * Test swi interrupts, triggered
     * while ints disabled.
     */
    wait_for_thread(1);
    wait_for_thread(2);
    wait_for_thread(3);
    global_int_disable();
    swi(INT_MASK);
    global_int_enable();
    printf("waiting for wake #2\n");
    wait_for_ints_delivered(2);

    /*
     * Test swi interrupts, triggered
     * while ints masked for all threads.
     */
    wait_for_thread(1);
    wait_for_thread(2);
    wait_for_thread(3);
    int INT_THREAD_MASK = 0x1f;
    for (int i = 0; i < MAX_INT_NUM; i++) {
        iassignw(i, INT_THREAD_MASK);
    }
    swi(INT_MASK);
    /* Now unmask: */
    INT_THREAD_MASK = ~(1 << 1 | 1 << 2 | 1 << 3);
    for (int i = 0; i < MAX_INT_NUM; i++) {
        iassignw(i, INT_THREAD_MASK);
    }
    printf("waiting for wake #3\n");
    wait_for_ints_delivered(3);


    int total_ints_tid = 0;
    for (int i = 0; i < TOTAL_THREAD_COUNT; i++) {
        printf("Total ints handled by tid %d: %d\n", i, ints_by_tid[i]);
        total_ints_tid += ints_by_tid[i];
    }
    assert(ints_by_tid[0] == 0);

    int total_ints_irq = 0;
    for (int i = 0; i < MAX_INT_NUM; i++) {
        printf("Total ints handled for IRQ %d: %d\n", i, ints_by_irq[i]);
        assert(ints_by_irq[i] == 3);
        total_ints_irq += ints_by_irq[i];
    }

    assert(total_ints_irq == (MAX_INT_NUM * 3));
    assert(total_ints_tid == (MAX_INT_NUM * 3));

    /* Teardown: */
    tasks_enabled = false;
    wait_for_wait();

    thread_join(1 << 1);
    thread_join(1 << 2);
    thread_join(1 << 3);

    printf("PASS\n");
    return 0;
}
