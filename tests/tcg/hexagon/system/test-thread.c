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

#include <hexagon_standalone.h>
#include <string.h>


#define THCNT 8
static int thid[THCNT] = { 0 };
static int expect[THCNT] = { 0, 1, 2, 3, 4, 5, 6, 7 };

static int Mx;
volatile int thcnt = 0;
void thread(void *y)
{
    unsigned int id = thread_get_tnum();
    assert(id < (THCNT));
    while (1) {
        if (trylockMutex(&Mx))
            break;
        __asm__ volatile("pause (#200)\n");
    }
    thcnt++;
    thid[id] = id;
    unlockMutex(&Mx);
}

#define STACK_SIZE 0x8000
char __attribute__((aligned(16))) stack[THCNT - 1][STACK_SIZE];

int main()
{
    unsigned int id, thmask = 0;
    lockMutex(&Mx);
    for (int i = 1; i <= THCNT; i++) {
        thread_create(thread, &stack[i - 1][STACK_SIZE - 16], i, NULL);
        thmask |= (1 << i);
    }
    unlockMutex(&Mx);

    while (thcnt < (THCNT - 1)) {
        __asm__ volatile("pause (#200)\n");
    }

    thread_join(thmask);

    if (memcmp(thid, expect, sizeof(expect))) {
        printf("FAIL\n");
        for (int i = 0; i < THCNT; i++)
            printf("EXPECT: expect[%d] = %d, GOT: thid[%d] = %d\n", i,
                   expect[i], i, thid[i]);
        return 1;
    }
    printf("PASS\n");
    return 0;
}
