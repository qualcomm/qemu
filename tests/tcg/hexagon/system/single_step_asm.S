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

.global single_step_ex
single_step_ex:

/* Tell everyone we have hit the Single Step exception */
    r0 = memw(##see_single_step)
    r0 = add(r0, #1)
    memw(##see_single_step) = r0

/* Return in monitor-mode when the test is over */
    r0 = memw(##finish_single_step)
    {
           p0 = cmp.gt(r0, #0)
           if (!p0.new) jump:nt .Lrte
    }
    r0 = ssr
    r0 = clrbit(r0, #16)
    ssr = r0
.Lrte:
    rte
.size single_step_ex, . - single_step_ex

.global put_ssr_um
put_ssr_um:
    r0 = ssr
    r0 = setbit(r0, #16)
    ssr = r0                           /* Now in single stepping in usermode */
    nop                                /* 0 */
    jumpr lr                           /* 1 */
.size put_ssr_um, . - put_ssr_um

.global ss_func
ss_func:
    r1 = memw(##see_single_step)       /* 3   * checked */
    memw(##witness_1) = r1;            /* 4 */
    r1 = memw(##see_single_step)       /* 5   * checked */
    memw(##witness_2) = r1;            /* 6 */
    jump 1f                            /* 7 */
    stop(r0)
    stop(r0)
    stop(r0)
1:
    r1 = memw(##see_single_step)       /* 8   * checked */
    memw(##witness_3)  = r1;           /* 9 */
    {
        nop
        nop
        nop
        nop
    }                                  /* 10 */
    nop                                /* 11 */
    nop                                /* 12 */
    r1 = memw(##see_single_step)       /* 13   * checked */
    memw(##witness_4)  = r1;           /* 14 */

/* The ISR, single_step_ex will look at this and bring the thread out of
 * user-mode stoppping the test.
 */

    memw(##finish_single_step) = r1;
    jumpr r31
.size ss_func, . - ss_func



.global ss_test
ss_test:
    allocframe(#0x0)
    call #put_ssr_um
    call #ss_func                      /* 2 */
    dealloc_return
.size ss_test, . - ss_test
