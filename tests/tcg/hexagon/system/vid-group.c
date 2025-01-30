/*
 *  Copyright(c) 2023-2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <stdlib.h>
#include <stdio.h>
#include <hexagon_standalone.h>
#include "thread_common.h"

#include "cfgtable.h"

/* This and other volatiles are for dealing with mmio registers */
typedef volatile unsigned int vu32;
static uint32_t HWTID[6];

void pause()
{
    __asm__ __volatile__("pause(#1)");
}

vu32 g_l2vic_base;
vu32 *FAST_INTF_VA = (vu32 *)0x888e0000;
#define FIRST_IRQ (36)
#define STACK_SIZE (16384)
#define ARCH_REV __HEXAGON_ARCH__

static int IRQ[4] = { FIRST_IRQ, FIRST_IRQ + 1, FIRST_IRQ + 2, FIRST_IRQ + 3 };

/* Using volatile for MMIO writes */
#define L2VIC_INT_ENABLE(n) \
    ((vu32 *) ((g_l2vic_base) + 0x100 + 4 * (n / 32))) /* device memory */
#define L2VIC_INT_ENABLE_CLEAR(n) \
   ((vu32 *)((g_l2vic_base) + 0x180 + 4 * (n / 32)))
#define L2VIC_INT_ENABLE_SET(n) \
   ((vu32 *) ((g_l2vic_base) + 0x200 + 4 * (n / 32))) /* device memory */
#define L2VIC_INT_CLEAR(n) \
   ((vu32 *)((g_l2vic_base) + 0x400 + 4 * (n / 32)))
#define L2VIC_SOFT_INT_SET(n) \
   ((vu32 *) ((g_l2vic_base) + 0x480 + 4 * (n / 32))) /* device memory */
#define L2VIC_INT_TYPE(n) \
   ((vu32 *) ((g_l2vic_base) + 0x280 + 4 * (n / 32))) /* device memory */
#define L2VIC_INT_GRP(n) ((vu32 *)((g_l2vic_base) + 0x600 + 4 * n))

static int count[4] = { 0, 0, 0, 0 };

#define asm_wait() __asm__ __volatile__("    wait(r0)\n" : : : "r0")

#define asm_resume()                                                           \
    __asm__ __volatile__("    r0 = #7\n"                                       \
             "    resume(r0)\n"                                    \
             :                                                     \
             :                                                     \
             : "r0");

unsigned long long my_read_pcycles(void)
{
    uint32_t lo, hi;
    uint64_t pcyc;

    __asm__ __volatile__("%0 = pcyclelo\n"
             "%1 = pcyclehi\n"
             : "=r"(lo), "=r"(hi));
    pcyc = (((uint64_t)hi) << 32) + lo;
    return pcyc;
}

static uint64_t t1, t2;

void update_l2vic(uint32_t irq)
{
    *FAST_INTF_VA = (0 << 16) + irq; /* fast re-enable */
}

void init_l2vic()
{
    for (int i = 0; i < 4; i++) {
        uint32_t irq = IRQ[i];
        uint32_t irq_bit = (1 << (irq % 32));

        *L2VIC_INT_ENABLE_CLEAR(irq) = irq_bit;
        *L2VIC_INT_TYPE(irq) |= irq_bit;
        *L2VIC_INT_ENABLE_SET(irq) = irq_bit;
    }
}

void intr_handler(int irq)
{
    uint32_t pcyclo, htid, j, vid;
    static uint32_t count[4] = { 0, 0, 0, 0 };

    __asm__ __volatile__("%0 = pcyclelo\n"
             "%1 = htid\n"
             : "=r"(pcyclo), "=r"(htid));
    HWTID[htid]++;


    vid = *((uint32_t *)(g_l2vic_base + (irq - 2) * 4));
    if ((vid - FIRST_IRQ) > 3) {
        printf("FAIL\n");
        printf(" unexpected L2IRQ number %lu\n", vid);
        fflush(stdout);
        exit(-1);
    }
    count[vid - FIRST_IRQ]++;
    j = 0;
    for (int i = 0; i <= 3; i++) {
        if (count[i] >= 4) {
            j++;
        }
    }
    update_l2vic(vid);
}

int int1;
int int2;
int int3;
int int4;

void do_int1(int irq)
{
    int tnum = thread_get_tnum();
    assert(tnum == 1);
    intr_handler(irq);
    int1 = 1;
}
void do_int2(int irq)
{
    int tnum = thread_get_tnum();
    assert(tnum == 2);
    intr_handler(irq);
    int2 = 1;
}
void do_int3(int irq)
{
    int tnum = thread_get_tnum();
    assert(tnum == 3);
    intr_handler(irq);
    int3 = 1;
}
void do_int4(int irq)
{
    int tnum = thread_get_tnum();
    assert(tnum == 4);
    intr_handler(irq);
    int4 = 1;
}

/* assign l2irq to specific L1INT */
void assign_l2irq_to_l1int(int *irq_array, int how_many)
{
    int irq, word_id, set_id;

    for (int i = 0; i < how_many; i++) {
        irq = irq_array[i];
        word_id = irq / 32;
        set_id = irq % 32;
        *L2VIC_INT_GRP(word_id) |= (8 + i) << (set_id * 4);
    }
}

void enable_core_interrupt()
{
    uint32_t set;

    register_interrupt(2, do_int1); /* T1 */
    register_interrupt(3, do_int2); /* T2 */
    register_interrupt(4, do_int3); /* T3 */
    register_interrupt(5, do_int4); /* T4 */

    set = (2 << 16) + 0xfd; /* only T1 to serve INT#2 */
    __asm__ __volatile__("r0 = %0\n"
             "iassignw(r0)\n"
             :
             : "r"(set)
             : "r0");
    set = (3 << 16) + 0xfb; /* only T2 to serve INT#3 */
    __asm__ __volatile__("r0 = %0\n"
             "iassignw(r0)\n"
             :
             : "r"(set)
             : "r0");
    set = (4 << 16) + 0xf7; /* T3 serves INT#4 and INT#5 */
    __asm__ __volatile__("r0 = %0\n"
             "iassignw(r0)\n"
             :
             : "r"(set)
             : "r0");
    set = (5 << 16) + 0xef; /* T4 serves INT#4 and INT#5 */
    __asm__ __volatile__("r0 = %0\n"
             "iassignw(r0)\n"
             :
             : "r"(set)
             : "r0");

    assign_l2irq_to_l1int(IRQ, 4);
}

char __attribute__((aligned(16))) stack1[STACK_SIZE];
char __attribute__((aligned(16))) stack2[STACK_SIZE];
char __attribute__((aligned(16))) stack3[STACK_SIZE];
char __attribute__((aligned(16))) stack4[STACK_SIZE];

void thread(void *data)
{
    uint32_t pcyclo;
    volatile int *int_seen = (int *)data; /* volatile: changed in int handler */
    while (*int_seen == 0) {
        __asm__ __volatile__("%0 = pcyclelo" : "=r"(pcyclo));
        pause();
    }
    return;
}

int main()
{
    unsigned int base;
    unsigned int va;
    unsigned long long int pa;

    thread_create_blocked(thread, &stack4[STACK_SIZE - 16], 4, (void *)&int4);
    thread_create_blocked(thread, &stack3[STACK_SIZE - 16], 3, (void *)&int3);
    thread_create_blocked(thread, &stack2[STACK_SIZE - 16], 2, (void *)&int2);
    thread_create_blocked(thread, &stack1[STACK_SIZE - 16], 1, (void *)&int1);

    /* setup the fastl2vic interface and setup an indirect mapping */
    add_translation_extended(3, (void *)FAST_INTF_VA, GET_FASTL2VIC_BASE(), 16, 7, 4, 0, 0, 3);

    g_l2vic_base = GET_SUBSYSTEM_BASE() + 0x10000;


    enable_core_interrupt();
    init_l2vic();

    for (int i = 0; i < 6; i++) { /* disable(1)-enable(4)-disable(1) */
        printf(" iteration S0 %d @ 0x%llx\n", i, my_read_pcycles());
        fflush(stdout);

        /* clear off all pending interrupts */
        for (int j = 0; j < 4; j++) {
            unsigned int bit = 1 << (IRQ[j] % 32);
            *L2VIC_INT_CLEAR(IRQ[j]) = bit;
        }

        /* set up */
        printf(" iteration S1 %d @ 0x%llx\n", i, my_read_pcycles());
        fflush(stdout);
        if ((i == 0) || (i == 5)) {
            for (int j = 0; j < 4; j++) {
                *FAST_INTF_VA = (1 << 16) + IRQ[j]; /* disable them */
            }
        }
        if (i == 1) {
            for (int j = 0; j < 4; j++) {
                *FAST_INTF_VA = (0 << 16) + IRQ[j]; /* enable them */
            }
        }

        /* action */
        printf(" iteration S2 %d @ 0x%llx\n", i, my_read_pcycles());
        fflush(stdout);

        for (int j = 0; j < 4; j++) {
            *FAST_INTF_VA = (2 << 16) + IRQ[j];
        }

        /*
         * This takes the place of the "waste_some_time" function.
         * With MTTCG we can reach points where we always clear pendings
         * at the top of this loop such that the lower priority ints never
         * get a chance to run.
         * Wait until the lowest priority int is seen before moving
         * forward.  NOTE: Changing int4 to int1 would expose the periodic
         * failures when mttcg is enabled.
         */
        while (!int4) {
                pause();
        }
    }

    /*
     * Wait until each thread has exited.
     * This could hang if a thread never gets an interrupt.
     */
    for (int i = 1; i < 5; i++) {
        thread_join(1 << i);
    }

    printf("PASS\n");
    return 0;
}
