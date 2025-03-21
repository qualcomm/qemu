/*
 *  Copyright(c) 2023-2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>

int err;
#include "hex_test.h"

uint64_t get_upcycle(void)
{
    uint32_t upcyclelo, upcyclehi;
    uint64_t upcycle;

    /*
     * On linux-user mode, we assume SSR[CE] is always set.
     */
    asm volatile("%0 = upcycle\n\t"
                 "%1 = upcyclelo\n\t"
                 "%2 = upcyclehi\n\t"
                 : "=r"(upcycle), "=r"(upcyclelo), "=r"(upcyclehi));

    /* sanity checks */
    check32(upcycle, (((uint64_t)upcyclehi) << 32) | (uint64_t)upcyclelo);
    return upcycle;
}

int main()
{
    puts("Check that we can read upcycle counters in linux-user mode");

    uint64_t initial_cycles = get_upcycle();
    /* now spend some cycles */
    asm volatile(
        "   loop0(1f, #1000)\n"
        "1: { nop; }:endloop0\n"
        : : : "sa0", "lc0", "usr"
    );
    check32_range(get_upcycle() - initial_cycles, 1000, 1100);

    puts(err ? "FAIL" : "PASS");
    return err;
}
