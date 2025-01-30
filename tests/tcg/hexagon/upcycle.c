/*
 *  Copyright(c) 2023-2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>

int err;

static inline void __check_range(uint32_t val, uint32_t min, uint32_t max, int line)
{
    if (val < min || val > max) {
        fprintf(stderr,
                "ERROR at line %d: %" PRIu32 " not in [%" PRIu32 ", %" PRIu32 "]\n",
                line, val, min, max);
        err++;
    }
}

#define check_range(V, MIN, MAX) __check_range(V, MIN, MAX, __LINE__)

static inline void __check(uint32_t val, uint32_t expect, int line)
{
    if (val != expect) {
        fprintf(stderr,
                "ERROR at line %d: %" PRIu32 " != %" PRIu32 "\n",
                line, val, expect);
        err++;
    }
}

#define check(V, E) __check(V, E, __LINE__)

int main()
{
    uint32_t upcyclelo, upcyclehi;
    uint64_t upcycle;

    puts("Check that we can read upcycle counters in linux-user mode");

    /*
     * On linux-user mode, we assume SSR[CE] is always set.
     */
    asm volatile("%0 = upcycle\n\t"
                 "%1 = upcyclelo\n\t"
                 "%2 = upcyclehi\n\t"
                 : "=r"(upcycle), "=r"(upcyclelo), "=r"(upcyclehi));

    check(upcyclehi, 0);
    check(upcyclelo, upcycle);
    check_range(upcycle, 3500, 6500);

    puts(err ? "FAIL" : "PASS");
    return err;
}
