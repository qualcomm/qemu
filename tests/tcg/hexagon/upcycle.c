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
