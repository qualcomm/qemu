/*
 *  Copyright(c) 2019-2023 Qualcomm Innovation Center, Inc. All Rights Reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "hexagon_standalone.h"


static inline void tlblock(void)
{
    asm volatile("tlblock\n");
}

static inline void tlbunlock(void)
{
    asm volatile("tlbunlock\n");
}

static inline void pause(void)
{
    asm volatile("pause(#3)\n");
}

#define COMPUTE_THREADS           3
#define STACK_SIZE            16384
char stack[COMPUTE_THREADS][STACK_SIZE] __attribute__((__aligned__(8)));

static void thread_func(void *arg)
{
    for (int i = 0; i < 5; i++) {
        tlblock();
        tlbunlock();
        tlbunlock();
    }
}

int main(int argc, char *argv[])
{
    const int work = COMPUTE_THREADS * 3;

    puts("Testing tlblock/tlbunlock");
    for (int j = 0; j < work; ) {
        tlblock();
        for (int i = 0; i < COMPUTE_THREADS && j < work; i++, j++) {
            thread_create((void *)thread_func, &stack[i][STACK_SIZE], i + 1, 0);
        }
        tlbunlock();
        thread_join(((1 << COMPUTE_THREADS) - 1) << 1);
    }
    printf("PASS\n");
    return 0;
}
