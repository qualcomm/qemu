/*
 *  Copyright(c) 2023-2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <hexagon_standalone.h>
#include <stdio.h>


volatile int thcnt = 0;

void thread1(void *y)
{
    __asm__("r0 = ##0x001200ff; stid = r0; isync" ::: "r0");
    printf("CT: thread id=%d, will wait, thread1\n", thread_get_tnum());
    thcnt++;
    __asm__ volatile("r0 = #0; wait(r0);" ::: "r0");
    printf("\nTID %d Resumed\n", thread_get_tnum());
    if (thcnt != 4) {
        printf("\nTID %d Expected ISR didn't run\n", thread_get_tnum());
    } else
        thcnt = 5;

    return;
}

void handler(int intno)
{
    int bits = 1 << intno;
    // printf("TID:%d %s: int=%d\n", thread_get_tnum(), __func__, intno);
    thcnt = 4;
    __asm__("ciad(%0)\n\t" ::"r"(bits));
}

void pause()
{
    __asm__ __volatile__("pause(#1)");
}
void die(int intno)
{
    printf("%s: int=%d, tid=%d\n", __func__, intno, thread_get_tnum());
    printf("thread dead\n");
    while (1) {
        pause();
    }
}

#define STACK_SIZE 0x8000
char __attribute__((aligned(16))) stack1[STACK_SIZE];

int main()
{
    thcnt = 1;
    unsigned int id;
    printf("MT: thread id=%d\n", id = thread_get_tnum());
    __asm__("r0 = ##0x1ff; bestwait = r0; isync" ::: "r0");
    __asm__("r0 = #0x3f;  imask = r0; isync" ::: "r0");

    __asm__("r0 = ##0x104; schedcfg = r0; isync" ::: "r0");
    register_interrupt(4, handler);
    // register_interrupt(0, die);
    register_interrupt(1, die);
    register_interrupt(2, die);
    register_interrupt(3, die);
    register_interrupt(5, die);
    register_interrupt(6, die);
    register_interrupt(7, die);

    thread_create(thread1, (void *)&stack1[STACK_SIZE - 16], 1, (void *)&thcnt);

    while (thcnt < 2) {
        pause();
    }
    printf("\nTID 1 should be waiting, thcnt = %d\n", thcnt);

    printf("TID 0 sets bestwait to wake up TID 1\n");

    // Verify wait stopped the thread.
    if (thcnt == 5) {
        printf("FAIL\n");
        return 1;
    }

    // This should restart thread 1:
    // Trigger "handler" then thread 1 resumes
    __asm__("r0 = #0x11\n\t"
            "{ bestwait = r0\n\t"
            "  jump skipit }\n\t"
            "stop(r0)\n\t"
            "skipit:\n\t"
            "isync" ::
                : "r0");

    thread_join(1 << 0x1);

    if (thcnt == 5)
        printf("PASS\n");
    else
        printf("FAIL\n");

    return (thcnt == 5) ? 0 : 1;
}
