/*
 *  Copyright(c) 2023 Qualcomm Innovation Center, Inc. All Rights Reserved.
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

#include "cfgtable.h"
#include <hexagon_standalone.h>
#include <string.h>

static inline void infloop(void)
{
    while (1) {
        asm volatile("pause (#200)\n");
    }
}

#define NUM_THREADS 4
#define STACK_SIZE 0x8000
char __attribute__((aligned(16))) stack[NUM_THREADS][STACK_SIZE];
static void thread(void *y)
{
    int id = (int)y;
    printf("Starting thread %d\n", id);
    infloop();
}

#define THREAD_ENABLE_MASK 0x48

int main()
{
    assert(read_cfgtable_field(THREAD_ENABLE_MASK) & 0xf);
    printf("Starting thread 0\n");
    for (int i = 1; i < NUM_THREADS; i++) {
        thread_create(thread, (void *)&stack[i - 1][STACK_SIZE - 16], i,
                      (void *)i);
    }
    infloop();
    return 0;
}
