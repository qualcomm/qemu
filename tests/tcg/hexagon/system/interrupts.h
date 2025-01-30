/*
 *  Copyright(c) 2022-2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef INTERRUPTS_H
#define INTERRUPTS_H

#include <stdint.h>

void global_int_disable()
{
    asm volatile("r0 = syscfg\n\t"
                 "r0 = clrbit(r0, #4)\n\t"
                 "syscfg = r0\n\t"
                 "isync\n\t"
                 :
                 :
                 : "r0");
}

void global_int_enable()
{
    asm volatile("r0 = syscfg\n\t"
                 "r0 = setbit(r0, #4)\n\t"
                 "syscfg = r0\n\t"
                 "isync\n\t"
                 :
                 :
                 : "r0");
}

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))
int vals[512];
int delay(int iters)
{
    for (int i = 0; i < iters; i++) {
        vals[i % ARRAY_SIZE(vals)] = i;
        asm volatile("pause(#1)\n\t");
        vals[i % ARRAY_SIZE(vals)]++;
    }
    return vals[0];
}


static inline void pause()
{
    asm volatile("pause(#1)\n\t");
}

static inline void swi(uint32_t mask)
{
    asm volatile("r0 = %0\n"
                 "swi(r0)\n"
                 :
                 : "r"(mask)
                 : "r0");
}

static inline void set_thread_imask(uint32_t mask)
{
    asm volatile("r0 = %0\n"
                 "imask = r0\n"
                 :
                 : "r"(mask)
                 : "r0");
}

static inline void wait_for_interrupts(void)
{
    asm volatile("wait(r0)\n"
                 :
                 :
                 :);
}

static inline void iassignw(int int_num, int thread_mask)
{
    int_num &= 0x01f;
    int_num = int_num << 16;
    asm volatile("r0 = and(%0, #0x0ff)\n\t"
                 "r0 = or(r0, %1)\n\t"
                 "iassignw(r0)\n"
                 :
                 : "r"(thread_mask), "r"(int_num)
                 :"r0");
}

static inline uint32_t get_task_prio()
{
    uint32_t stid;
    asm volatile("%0 = stid\n\t"
                 : "=r"(stid)
                 :
                 : );
    uint32_t prio = (stid >> 16) & 0xff;
    return prio;
}

static inline void set_task_prio(int prio)
{
    static uint32_t PRIO_CLR_MASK = 0xff00ffff;
    prio = prio & 0xff;
    asm volatile("r0 = stid\n\t"
                 "r0 = and(r0, %0)\n\t"
                 "r0 = or(r0, %1)\n\t"
                 "stid = r0\n\t"
                 "isync\n\t"
                 :
                 : "r"(PRIO_CLR_MASK), "r"(prio << 16)
                 : "r0");
}


#endif
