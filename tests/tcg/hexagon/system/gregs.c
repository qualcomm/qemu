/*
 *  Copyright(c) 2022-2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <stdint.h>
#include <stdio.h>


#define set_and_check(greg, val, expect_val, type, print_fmt) \
do { \
    type in_reg = val, out_reg, expect = expect_val; \
    asm volatile(greg " = %1\n\t" \
                 "%0 = " greg "\n\t" \
                 : "=r"(out_reg) \
                 : "r"(in_reg) \
                 : greg); \
    if (out_reg != expect) { \
        printf("ERROR: \"%s\" 0x" #print_fmt " != 0x" #print_fmt \
               "at %s:%d\n", greg, out_reg, \
               expect, __FILE__, __LINE__); \
        err++; \
    } \
} while (0)

#define set_and_check_eq(reg) \
    set_and_check(reg, 0xbabebeef, 0xbabebeef, uint32_t, "%08lx")
#define set_and_check_zero(reg) \
    set_and_check(reg, 0xbabebeef, 0x00000000, uint32_t, "%08lx")

#define pair_set_and_check_eq(reg) \
    set_and_check(reg, 0xbabebeef0f0f0f0f, 0xbabebeef0f0f0f0f, uint64_t, "%016llx")
#define pair_set_and_check_zero(reg) \
    set_and_check(reg, 0xbabebeef0f0f0f0f, 0x000000000000000, uint64_t, "%016llx")

int main()
{
    int err = 0;
    set_and_check_eq("g0");
    set_and_check_eq("g1");
    set_and_check_eq("g2");
    set_and_check_eq("g3");
    set_and_check_zero("g4");
    set_and_check_zero("g5");
    set_and_check_zero("g6");
    set_and_check_zero("g7");
    set_and_check_zero("g8");
    set_and_check_zero("g9");
    set_and_check_zero("g10");
    set_and_check_zero("g11");
    set_and_check_zero("g12");
    set_and_check_zero("g12");
    set_and_check_zero("g14");
    set_and_check_zero("g15");
    set_and_check_zero("g16");
    set_and_check_zero("g17");
    set_and_check_zero("g18");
    set_and_check_zero("g19");
    set_and_check_zero("g20");
    set_and_check_zero("g21");
    set_and_check_zero("g22");
    set_and_check_zero("g23");
    set_and_check_zero("g24");
    set_and_check_zero("g25");
    set_and_check_zero("g26");
    set_and_check_zero("g27");
    set_and_check_zero("g28");
    set_and_check_zero("g29");
    set_and_check_zero("g30");
    set_and_check_zero("g31");

    pair_set_and_check_eq("g3:2");
    pair_set_and_check_zero("g5:4");

    printf("%s\n", ((err) ? "FAIL" : "PASS"));
    return err;
}
