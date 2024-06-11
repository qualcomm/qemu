/*
 *  Copyright(c) 2019-2024 Qualcomm Innovation Center, Inc. All Rights Reserved.
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
#include "thread_common.h"

static inline void k0lock(void)
{
    asm volatile("k0lock\n");
}

static inline void k0unlock(void)
{
    asm volatile("k0unlock\n");
}
static inline uint32_t getsyscfg()
{
    uint32_t reg;
    asm volatile ("%0=syscfg"
                  : "=r"(reg));
    return reg;
}
static inline void putsyscfg(uint32_t val)
{
    asm volatile ("syscfg=%0;"
                  : : "r"(val) : "syscfg");
    asm volatile("isync\n");
    return;
}

#define COMPUTE_THREADS           2
#define STACK_SIZE            16384
char stack[COMPUTE_THREADS][STACK_SIZE] __attribute__((__aligned__(8)));

static void mod_syscfg(void *arg)
{
    uint32_t my_syscfg = getsyscfg();
    for (int i = 0; i < 1000; i++) {
        putsyscfg(my_syscfg);
        my_syscfg = getsyscfg();
    }
}
static void mod_k0(void *arg)
{
    for (int i = 0; i < 1000; i++) {
        k0lock();
        k0unlock();
    }
}

int main(int argc, char *argv[])
{
    create_waiting_thread(mod_syscfg, &stack[0][STACK_SIZE - 16], 1, 0);
    create_waiting_thread(mod_k0, &stack[1][STACK_SIZE - 16], 2, 0);
    start_waiting_threads(0x6);
    thread_join(1 << 1);
    thread_join(1 << 2);
    printf("PASS\n");
    return 0;
}
