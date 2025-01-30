/*
 *  Copyright(c) 2020-2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 *
 * Qtimer Example
 *
 * This example initializes two timers that cause interrupts at different
 * intervals.  The thread 0 will sit in wait mode till the interrupt is
 * serviced then return to wait mode.
 *
 */

#include <assert.h>
#include <float.h>
#include <hexagon_sim_timer.h>
#include <stdio.h>
#include <stdlib.h>


#define COMPUTE_THREADS 3
#define STACK_SIZE 16384

volatile int exit_flag; /* required */
unsigned long long start_cycles, current_cycles;
static char stack[COMPUTE_THREADS][STACK_SIZE] __attribute__((__aligned__(8)));

void register_interrupt(int intno, void (*IRQ_handler)(int intno));
void thread_create(void (*pc)(void *), void(*sp), int threadno,
                   void(*data_struct));
void thread_join(int mask);
void add_translation(void(*va), void(*pa), int cacheability);

typedef unsigned long u32;
typedef unsigned long long u64;

typedef volatile unsigned long vu32; /* reqiured */
typedef volatile unsigned long long vu64; /* required */

#define QTMR_BASE ((CSR_BASE) + 0x20000)
#define QTMR_AC_CNTACR ((vu32 *)((QTMR_BASE) + 0x40))
#define QTMR_CNTPCT_LO ((vu32 *)((QTMR_BASE) + 0x1000))
#define QTMR_CNTPCT_HI ((vu32 *)((QTMR_BASE) + 0x1004))
#define QTMR_CNTP_CVAL_LO ((vu32 *)((QTMR_BASE) + 0x1020))
#define QTMR_CNTP_CVAL_HI ((vu32 *)((QTMR_BASE) + 0x1024))
#define QTMR_CNTP_TVAL ((vu32 *)((QTMR_BASE) + 0x1028))
#define QTMR_CNTP_CTL ((vu32 *)((QTMR_BASE) + 0x102c))

#define QTMR_CNTPCT2_LO ((vu32 *)((QTMR_BASE) + 0x2000))
#define QTMR_CNTPCT2_HI ((vu32 *)((QTMR_BASE) + 0x2004))
#define QTMR_CNTP2_CVAL_LO ((vu32 *)((QTMR_BASE) + 0x2020))
#define QTMR_CNTP2_CVAL_HI ((vu32 *)((QTMR_BASE) + 0x2024))
#define QTMR_CNTP2_TVAL ((vu32 *)((QTMR_BASE) + 0x2028))
#define QTMR_CNTP2_CTL ((vu32 *)((QTMR_BASE) + 0x202c))

#define L2VIC_BASE ((CSR_BASE) + 0x10000)
#define L2VIC_INT_ENABLE(n) ((vu32 *)((L2VIC_BASE) + 0x100 + 4 * (n / 32)))
#define L2VIC_INT_ENABLE_CLEAR(n) \
    ((vu32 *)((L2VIC_BASE) + 0x180 + 4 * (n / 32)))
#define L2VIC_INT_ENABLE_SET(n) ((vu32 *)((L2VIC_BASE) + 0x200 + 4 * (n / 32)))
#define L2VIC_INT_TYPE(n) ((vu32 *)((L2VIC_BASE) + 0x280 + 4 * (n / 32)))
#define L2VIC_INT_STATUS(n) ((vu32 *)((L2VIC_BASE) + 0x380 + 4 * (n / 32)))
#define L2VIC_INT_PENDING(n) ((vu32 *)((L2VIC_BASE) + 0x500 + 4 * (n / 32)))

#define ticks_per_qtimer1 ((QTMR_FREQ) / 7000)
#define ticks_per_qtimer2 ((QTMR_FREQ) / 700)

static volatile int qtimer1_cnt = 0, qtimer2_cnt = 0, loop = 0, /* required */
                    thread_cnt[4] = { 0, 0, 0, 0 }; /* required */
static u32 tval, tval2;

static void thread_resume(int thread_num)
{
    asm volatile(".align 32\n"
                 /* get thread number */
                 "r1 = %0\n"
                 /* put thread in wait mode */
                 "    resume(r1)\n"
                 :
                 : "r"(thread_num)
                 : "r1");
}

static void asm_wait()
{
    /* printf("All threads going into wait mode\n"); */
    asm volatile("r0 = #7\n"
                 "wait(r0)\n"
                 :
                 :
                 : "r0");
}

void update_qtimer1()
{
    u64 cval = (((u64) *QTMR_CNTP_CVAL_HI) << 32) |
        ((u64) *QTMR_CNTP_CVAL_LO);
    cval += ticks_per_qtimer1;
    *QTMR_CNTP_CVAL_LO = (u32)(cval & 0xffffffff);
    *QTMR_CNTP_CVAL_HI = (u32)(cval >> 32);
}

void update_qtimer2()
{
    u64 cval = (((u64) *QTMR_CNTP2_CVAL_HI) << 32) |
        ((u64) *QTMR_CNTP2_CVAL_LO);
    cval += ticks_per_qtimer2;
    *QTMR_CNTP2_CVAL_LO = (u32)(cval & 0xffffffff);
    *QTMR_CNTP2_CVAL_HI = (u32)(cval >> 32);
}

void init_qtimers(int ctr)
{
    /* enable read/write access to all timers */
    *QTMR_AC_CNTACR = 0x3f;
    if (ctr & 1) {
        /* set up timer 1 */
        *QTMR_CNTP_TVAL = ticks_per_qtimer1;
        *QTMR_CNTP_CTL = 1;
    }
    if (ctr & 2) {
        /* set up timer 2 */
        *QTMR_CNTP2_TVAL = ticks_per_qtimer2;
        *QTMR_CNTP2_CTL = 1;
    }
}

void update_l2vic(u32 irq)
{
    u32 irq_bit = 1 << (irq % 32);
    *L2VIC_INT_ENABLE_SET(irq) = irq_bit;
}

void init_l2vic()
{
    u32 irq1_bit, irq2_bit;

    irq1_bit = (1 << (IRQ1 % 32));
    irq2_bit = (1 << (IRQ2 % 32));

    if ((IRQ1 / 32) == (IRQ2 / 32)) {
        irq1_bit |= irq2_bit;

        *L2VIC_INT_ENABLE_CLEAR(IRQ1) = irq1_bit;
        *L2VIC_INT_TYPE(IRQ1) = 0;
        *L2VIC_INT_ENABLE_SET(IRQ1) = irq1_bit;
    } else {
        *L2VIC_INT_ENABLE_CLEAR(IRQ1) = irq1_bit;
        *L2VIC_INT_TYPE(IRQ1) = 0;
        *L2VIC_INT_ENABLE_SET(IRQ1) = irq1_bit;

        *L2VIC_INT_ENABLE_CLEAR(IRQ2) = irq2_bit;
        *L2VIC_INT_TYPE(IRQ2) = 0;
        *L2VIC_INT_ENABLE_SET(IRQ2) = irq2_bit;
    }
}

void intr_handler(int irq)
{
    u32 vid;

    float time_in_sec = ((float)(*QTMR_CNTPCT_LO)) / ((float)(QTMR_FREQ));

    __asm__ __volatile__("%0 = VID" : "=r"(vid));

    if (vid == IRQ1) {
        /* printf("IRQ1 start\n"); */
        qtimer1_cnt++;
        update_l2vic(vid);
        update_qtimer1();
        /* printf("IRQ1 end\n"); */
    } else if (vid == IRQ2) {
        int i = 0;
        int pending = *L2VIC_INT_PENDING(0);
        update_l2vic(vid);
        /*
         * printf("IRQ2 is waiting for next pending,
         * pending = 0x%x\n", pending);
         */
        while (1) {
            /* printf("tripcnt: %d\n", i++); */
            if (*L2VIC_INT_PENDING(IRQ1) == 0x8) {
                /*
                 *printf("Notice another IRQ1 event while running
                 * IRQ2 handler\n");
                 */
                break;
            }
        }
        qtimer2_cnt++;
        update_qtimer2();
        /* printf("IRQ2 is serviced\n"); */
    } else {
        printf("Other IRQ %lu\n", vid);
    }
}

/*
 *  Hexagon cores v65 and above use interrupt 2 instead of interrupt 32
 */
void enable_core_interrupt()
{
    int irq = 2;
    register_interrupt(irq, intr_handler);
}


int main()
{
    int i;
    exit_flag = 0;
    printf("\nCSR base=0x%x; L2VIC base=0x%x\n", CSR_BASE, L2VIC_BASE);
    printf("QTimer1 will go off 20 times (once every 1/%d sec).\n",
           (QTMR_FREQ) / (ticks_per_qtimer1));
    printf("QTimer2 will go off 2 times (once every 1/%d sec).\n\n",
           (QTMR_FREQ) / (ticks_per_qtimer2));

    add_translation((void *)CSR_BASE, (void *)CSR_BASE, 4);

    enable_core_interrupt();

    init_l2vic();
    /* initialize qtimers 1 and 2 */
    init_qtimers(3);
    int last = 1;

    while (qtimer2_cnt < 25) {
        /* Thread 0 waits for interrupts */
        if (last == qtimer2_cnt) {
            printf("tick: %d\n", qtimer2_cnt);
        }
        last = qtimer2_cnt + 1;
        asm_wait();
    }
    printf("PASS\n");
    return 0;
}
