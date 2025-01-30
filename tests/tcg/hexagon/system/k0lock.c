/*
 *  Copyright(c) 2019-2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "hexagon_standalone.h"


static inline void k0lock(void)
{
    asm volatile("k0lock\n");
}

static inline void k0unlock(void)
{
    asm volatile("k0unlock\n");
}

#define COMPUTE_THREADS           3
#define STACK_SIZE            16384
char stack[COMPUTE_THREADS][STACK_SIZE] __attribute__((__aligned__(8)));

static void thread_func(void *arg)
{
    for (int i = 0; i < 3; i++) {
        k0lock();
        k0unlock();
        k0unlock();
    }
}

int main(int argc, char *argv[])
{
    const int work = COMPUTE_THREADS * 3;

    puts("Testing k0lock/k0unlock");
    for (int j = 0; j < work; ) {
        k0lock();
        for (int i = 0; i < COMPUTE_THREADS && j < work; i++, j++) {
            thread_create((void *)thread_func, &stack[i][STACK_SIZE], i + 1, 0);
        }
        k0unlock();
        thread_join(((1 << COMPUTE_THREADS) - 1) << 1);
    }
    printf("PASS\n");
    return 0;
}
