/*
 *  Copyright(c) 2024 Qualcomm Innovation Center, Inc. All Rights Reserved.
 *  SPDX-License-Identifier: BSD-3-Clause
 */

    .text
.global _start
_start:
    r2 = #0
    p0 = !cmp.eq(r2,r2) // False
    p1 =  cmp.eq(r2,r2) // True

    /*
     * This instruction will use slot 0 and get cancelled. The cancellation
     * state should be cleaned for the next packet.
     */
    if (p0) memw(r2) = r2

    /*
     * This packet is crafted so that there are two conditional writes to
     * the same register and also an instruction that uses slot 0. The idea
     * is that, if the cancellation state is not properly cleaned from the
     * previous packet, we may end up considering this packet as cancelled as
     * well (due to r1 = #0) and fail to detect the multiple writes to r6.
     */
    {
        r1 = #0
        if  (p1) r6 = r2
        if (!p0) r6 = r2
    }

    jump pass
