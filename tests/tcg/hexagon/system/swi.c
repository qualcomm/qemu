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


#include <errno.h>
#include <fcntl.h>
#include <hexagon_standalone.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>


#define MAX_THREADS 4

uint32_t read_ssr(void)
{
    unsigned ssr;
    asm volatile("%0 = ssr\n\t" : "=r"(ssr));
    return ssr;
}

uint32_t read_modectl(void)
{
    unsigned modectl;
    asm volatile("%0 = MODECTL\n\t" : "=r"(modectl));
    return modectl;
}

void do_wait(void)
{
    asm volatile("wait(r0)\n\t" : : : "r0");
}

void do_resume(uint32_t mask)
{
    asm volatile("resume(%0)\n\t" : : "r"(mask));
}

void send_swi(uint32_t mask)
{
    asm volatile("swi(%0)\n\t" : : "r"(mask));
}

/* read the thread's imask */
uint32_t getimask(int thread)
{
    uint32_t imask;
    asm volatile("isync\n\t"
                 "%0 = getimask(%1)\n\t"
                 "isync\n"
                 : "=r"(imask)
                 : "r"(thread));
    return imask;
}

/* enables ints for multiple threads */
void setimask(unsigned int tid, unsigned int imask_irq)
{
    asm volatile("r0 = %0\n\t"
                 "p0 = %1\n\t"
                 "setimask(p0, r0)\n\t"
                 "isync\n\t"
                 :
                 : "r"(imask_irq), "r"(tid)
                 : "r0", "p0");
}

typedef void (*ThreadT)(void *);
long long t_stack[MAX_THREADS][1024];

/* volatile because it tracks the interrupts taken */
volatile int intcnt[8];

void int_hdl(int intno)
{
    intcnt[intno]++;
}

void my_thread_function(void *y)
{
    /* accept only our interrupt */
    int tid = *(int *)y;
    unsigned read_mask;
    unsigned ssr = read_ssr();
    printf("app:%s: tid %d, ssr 0x%x, imask 0x%lx\n", __func__, tid, ssr,
           getimask(tid));

    register_interrupt(tid, int_hdl);
    printf(
        "app:%s: before wait: tid %d: thread init complete, modectl = 0x%lx\n",
        __func__, tid, read_modectl());

    /* let creating thread know we are done with initialization */
    do_wait();

    /* thread 0 sends to thread 1, thread 1 sends to thread 2, etc. */
    static unsigned int send_int_mask[4] = { 0x0, 0x4, 0x2, 0x0 };
    printf("app:%s: tid %u: sending swi with mask 0x%x, modectl = 0x%lx\n",
           __func__, tid, send_int_mask[tid], read_modectl());
    send_swi(send_int_mask[tid]);

    while (intcnt[tid] == 0) {
        ;
    }
    printf("app:%s: intcnt[tid=%d] %d, imask 1:0x%lx, imask 2:0x%lx\n",
           __func__, tid, intcnt[tid], getimask(1), getimask(2));
}

void wait_for_threads(void)
{
    unsigned t1mode, t2mode;
    do {
        unsigned modectl = read_modectl();
        t1mode = modectl & (0x1 << (16 + 1)); /* thread 1 wait bit */
        t2mode = modectl & (0x1 << (16 + 2)); /* thread 2 wait bit */
    } while (t1mode == 0 || t2mode == 0);
}

int main()
{
    unsigned join_mask = 0;
    int tid = 0;
    int id[MAX_THREADS] = { 0, 1, 2, 3 };

    unsigned ssr = read_ssr();
    printf("app:%s: tid %d, ssr 0x%x, imask 0x%lx\n", __func__, tid, ssr,
           getimask(tid));

    /* kick off the two threads and let them do their init */
    unsigned first_modectl = read_modectl();
    tid = 1;
    join_mask |= 0x1 << tid;
    thread_create(my_thread_function, &t_stack[tid][1023], tid,
                  (void *)&id[tid]);
    tid = 2;
    join_mask |= 0x1 << tid;
    thread_create(my_thread_function, &t_stack[tid][1023], tid,
                  (void *)&id[tid]);

    /* wait for both threads to finish their init and then restart them */
    wait_for_threads();

    tid = 0;
    setimask(tid, 0xFF);
    tid = 1;
    setimask(tid, (~(0x1 << tid)) & 0xFF);
    tid = 2;
    setimask(tid, (~(0x1 << tid)) & 0xFF);
    printf("app:%s: after wait: "
           "imask 0:0x%lx, imask 1:0x%lx, imask 2:0x%lx, imask 3:0x%lx\n",
           __func__, getimask(0), getimask(1), getimask(2), getimask(3));

    printf("app:%s: threads done with init: "
           "join mask 0x%x, first 0x%x, modectl 0x%lx\n",
           __func__, join_mask, first_modectl, read_modectl());
    do_resume(join_mask);

    /* wait for both to finish */
    thread_join(1 << 1);
    thread_join(1 << 2);

    printf("app:%s: printing intcnt array: modectl 0x%lx\n", __func__,
           read_modectl());
    printf("all: ");
    for (int i = 0; i < sizeof(intcnt) / sizeof(*intcnt); ++i) {
        if ((i % 4) == 0) {
            printf("\napp: ");
            printf("intcnt[%2u] = %u ", i, intcnt[i]);
        }
    }

    printf("PASS\n");
    return 0;
}
