/*
 *  Copyright(c) 2023-2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */


#include <errno.h>
#include <fcntl.h>
#include <hexagon_standalone.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include "thread_common.h"


#define MAX_THREADS 12 /* including main thread */

uint32_t read_ssr(void)
{
    unsigned ssr;
    asm volatile("%0 = ssr\n\t" : "=r"(ssr));
    return ssr;
}

uint32_t read_modectl(void)
{
    unsigned modectl;
    asm volatile("%0 = MODECTL\n\t" : "=r"(modectl));
    return modectl;
}

void do_wait(void)
{
    asm volatile("wait(r0)\n\t" : : : "r0");
}

void do_resume(uint32_t mask)
{
    asm volatile("resume(%0)\n\t" : : "r"(mask));
}

void send_swi(uint32_t mask)
{
    asm volatile("swi(%0)\n\t" : : "r"(mask));
}

/* read the thread's imask */
uint32_t getimask(int thread)
{
    uint32_t imask;
    asm volatile("isync\n\t"
                 "%0 = getimask(%1)\n\t"
                 "isync\n"
                 : "=r"(imask)
                 : "r"(thread));
    return imask;
}

/* enables ints for multiple threads */
void setimask(unsigned int tid, unsigned int imask_irq)
{
    asm volatile("r0 = %0\n\t"
                 "p0 = %1\n\t"
                 "setimask(p0, r0)\n\t"
                 "isync\n\t"
                 :
                 : "r"(imask_irq), "r"(tid)
                 : "r0", "p0");
}

typedef void (*ThreadT)(void *);
long long t_stack[MAX_THREADS][1024];

volatile int intcnt[32]; /* volatile because it tracks the interrupts taken */

void int_hdl(int intno)
{
    printf("  tid [%lu] received int %d\n", get_htid(), intno);
    intcnt[intno]++;
}

void my_thread_function(void *y)
{
    /* accept only our interrupt */
    int tid = *(int *)y;
    unsigned read_mask;
    unsigned ssr = read_ssr();
    printf("app:%s: tid %d, ssr 0x%x, imask 0x%lx\n", __func__, tid, ssr,
           getimask(tid));

    register_interrupt(tid, int_hdl);
    printf(
        "app:%s: before wait: tid %d: thread init complete, modectl = 0x%lx\n",
        __func__, tid, read_modectl());

    /* let creating thread know we are done with initialization */
    do_wait();

    /* thread 1 sends to thread 2, 2 to 3, ..., (MAX_THREADS - 1) to 1 */
    unsigned int send_int_mask = tid == (MAX_THREADS - 1) ? 2 : 1 << (tid + 1);
    printf("app:%s: tid %u: sending swi with mask 0x%x, modectl = 0x%lx\n",
           __func__, tid, send_int_mask, read_modectl());
    send_swi(send_int_mask);

    while (intcnt[tid] == 0) {
        ;
    }
}

void wait_for_threads(void)
{
    unsigned wait_mask, threads_mask = 0;
    for (int i = 1; i < MAX_THREADS; i++) {
        threads_mask |= (1 << i);
    }
    do {
        wait_mask = (read_modectl() >> 16) & threads_mask;
    } while (wait_mask != threads_mask);
}

int main()
{
    unsigned join_mask = 0;
    int id[MAX_THREADS];

    unsigned ssr = read_ssr();
    printf("app:%s: tid 0, ssr 0x%x, imask 0x%lx\n", __func__, ssr, getimask(0));

    /* kick off the threads and let them do their init */
    unsigned first_modectl = read_modectl();
    for (int tid = 1; tid < MAX_THREADS; tid++) {
        join_mask |= 0x1 << tid;
        id[tid] = tid;
        thread_create(my_thread_function, &t_stack[tid][1023], tid,
                      (void *)&id[tid]);
    }

    /* wait for both threads to finish their init and then restart them */
    wait_for_threads();
    printf("app:%s: after wait:\n", __func__);

    setimask(0, 0xffffffff);
    printf("  imask 0:0x%lx\n", getimask(0));
    for (int tid = 1; tid < MAX_THREADS; tid++) {
        setimask(tid, (~(0x1 << tid)) & 0xffffffff);
        printf("  imask %d:0x%lx\n", tid, getimask(tid));
    }

    printf("app:%s: threads done with init: "
           "join mask 0x%x, first 0x%x, modectl 0x%lx\n",
           __func__, join_mask, first_modectl, read_modectl());
    do_resume(join_mask);

    /* wait for threads to finish */
    printf("waiting threads with mask 0x%x\n", join_mask);
    thread_join(join_mask);

    printf("app:%s: printing intcnt array: modectl 0x%lx\n", __func__,
           read_modectl());
    printf("all: ");
    for (int i = 0; i < sizeof(intcnt) / sizeof(*intcnt); ++i) {
        if ((i % 4) == 0) {
            printf("\napp: ");
            printf("intcnt[%2u] = %u ", i, intcnt[i]);
        }
    }

    printf("\nPASS\n");
    return 0;
}
