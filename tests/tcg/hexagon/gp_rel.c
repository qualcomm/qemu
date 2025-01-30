/*
 *  Copyright(c) 2019-2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <stdio.h>
#include <stdint.h>

/*
 * Make sure we ignore GP when the load is constant extended
 * Approach:
 *     Normally, in linux-user mode GP is always 0
 *     Change it to 4, then do a constant-extended load
 *     Check that the load returned the first element of the array
 */

static inline void *getgp()
{
  void *reg;
  asm volatile ("%0 = GP"
                : "=r"(reg));
  return reg;
}

static inline void putgp(void *x)
{
  asm volatile ("GP = %0"
                :: "r"(x));
}

static inline uint32_t loadgp()
{
  uint32_t reg;
  asm volatile ("%0 = memw(##array)"
                : "=r"(reg));
  return reg;
}

int err;
uint32_t array[2] = { 0xdead, 0xbeef };

#define check(N, EXPECT) \
  if (N != EXPECT) { \
    printf("ERROR: 0x%04x != 0x%04x\n", N, EXPECT); \
    err++; \
}

int main()
{
    void *old_gp = getgp();
    uint32_t res;

    putgp(old_gp + 4);
    res = loadgp();
    putgp(old_gp);
    check(res, 0xdead);

    puts(err ? "FAIL" : "PASS");
    return err;
}

