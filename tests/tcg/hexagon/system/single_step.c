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

#include <stdio.h>
#include <hexagon_standalone.h>
#include "single_step.h"

static inline uint32_t get_ssr()
{
    uint32_t reg;
    asm volatile ("%0=ssr\n\t"
                  : "=r"(reg));
    return reg;
}
static inline void put_ssr(uint32_t val)
{
    asm volatile ("ssr=%0\n\t"
                  : : "r"(val));
    return;
}

int see_single_step;
int finish_single_step;

int check_point_1 = 3;
int check_point_2 = 5;
int check_point_3 = 8;
int check_point_4 = 13;

int witness_1;
int witness_2;
int witness_3;
int witness_4;

#define STACK_SIZE 0x8000
char __attribute__ ((aligned(16))) stack1[STACK_SIZE];

void thread(void *x)
{
    /* Enable Single Step Mode, this does nothing until we enter user mode */
    uint32_t ssr = get_ssr();
    ssr = ssr | (1 << 30);
    put_ssr(ssr);

    /* Will drop into user mode and start the test. */
    ss_test();
}

int main(void)
{
  /* Vector 12 is the debug exception handler */
  set_event_handler (HEXAGON_EVENT_12, single_step_ex);

  thread_create(thread, (void *)&stack1[STACK_SIZE - 16], 1, (void *)0);
  thread_join(1 << 1);

  int err = 0;
  if (witness_1 != check_point_1) {
    printf("ERROR: witness_1 fails, see: %d, expected %d\n",
             witness_1, check_point_1);
    err = 1;
  }

  if (witness_2 != check_point_2) {
    printf("ERROR: witness_2 fails, see: %d, expected %d\n",
             witness_2, check_point_2);
    err = 1;
  }

  if (witness_3 != check_point_3) {
    printf("ERROR: witness_3 fails, see: %d, expected %d\n",
             witness_3, check_point_3);
    err = 1;
  }

  if (witness_4 != check_point_4) {
    printf("ERROR: witness_4 fails, see: %d, expected %d\n",
             witness_4, check_point_4);
    err = 1;
  }

  if (err) {
    printf("FAIL:\n");
    return 1;
  }

  printf("witness_1=%d, witness_2=%d, witness_3=%d, witness_4=%d\n",
           witness_1, witness_2, witness_3, witness_4);

  printf("PASS:\n");
  return 0;

}

