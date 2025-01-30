/*
 *  Copyright(c) 2020-2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>

#define QTMR_BASE ((CSR_BASE) + 0x20000)
#define QTMR_CNTP1_CTL ((uint32_t *)((QTMR_BASE) + 0x102c))
#define QTMR_CNTP1_TVAL ((uint32_t *)((QTMR_BASE) + 0x1028))
#define QTMR_FREQ 19200000ULL


void timer_init()
{
    *QTMR_CNTP1_TVAL = QTMR_FREQ / 1000;
    *QTMR_CNTP1_CTL = 0x1; /* enable */
}
uint64_t utimer_read()
{
    uint32_t timer_low, timer_high;
    asm volatile("%1 = utimerhi\n\t"
                 "%0 = utimerlo\n\t"
                 : "=r"(timer_low), "=r"(timer_high));
    return ((uint64_t)timer_high << 32) | timer_low;
}
uint64_t utimer_read_pair()
{
    uint32_t timer_low, timer_high;
    asm volatile("r1:0 = utimer\n\t"
                 "%0 = r0\n\t"
                 "%1 = r1\n\t"
                 : "=r" (timer_low),
                   "=r" (timer_high));
    return ((uint64_t)timer_high << 32) | timer_low;
}
uint64_t timer_read()
{
    uint32_t timer_low, timer_high;
    asm volatile("%1 = s57\n\t"
                 "%0 = s56\n\t"
                 : "=r"(timer_low), "=r"(timer_high));
    return ((uint64_t)timer_high << 32) | timer_low;
}
uint64_t timer_read_pair()
{
    uint32_t timer_low, timer_high;
    asm volatile("r1:0 = s57:56\n\t"
                 "%0 = r0\n\t"
                 "%1 = r1\n\t"
                 : "=r"(timer_low), "=r"(timer_high));
    return ((uint64_t)timer_high << 32) | timer_low;
}

#endif
