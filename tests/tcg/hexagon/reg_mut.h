/*
 *  Copyright(c) 2023-2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef REG_MUT_H
#define REG_MUT_H

/*
 * NOTE: all these macros restore the original value of the reg before
 * returning.
 */

#define READ_WRITE_REG_NOCLOBBER(cur_val, result, reg_name, new_val) \
    asm volatile("%0 = " reg_name "\n\t" \
                 reg_name " = %2\n\t" \
                 "%1 = " reg_name "\n\t" \
                 reg_name " = %0\n\t" \
                 : "=r"(cur_val), "=r"(result) \
                 : "r"(new_val) \
                 : )

#define READ_WRITE_REG_ENCODED(cur_val, result, reg_name, new_val, encoding) \
    asm volatile("%0 = " reg_name "\n\t" \
                 "r1:0 = %2\n\t" \
                 encoding "\n\t" \
                 "%1 = " reg_name "\n\t" \
                 "r1:0 = %0\n\t" \
                 encoding "\n\t" \
                 : "=r"(cur_val), "=r"(result) \
                 : "r"(new_val) \
                 : "r0", "r1")

#define DO_WRITE_REG_NOCLOBBER(result, reg_name, new_val, TMP_REG) \
    asm volatile(TMP_REG " = " reg_name "\n\t" \
                 reg_name " = %1\n\t" \
                 "%0 = " reg_name "\n\t" \
                 reg_name " = " TMP_REG "\n\t" \
                 : "=r"(result) \
                 : "r"(new_val) \
                 : "r0", "r1") /* We use r1 in the PAIR variant */

#define WRITE_REG_NOCLOBBER(result, reg_name, new_val) \
    DO_WRITE_REG_NOCLOBBER(result, reg_name, new_val, "r0")

#define WRITE_REG_PAIR_NOCLOBBER(result, reg_name, new_val) \
    DO_WRITE_REG_NOCLOBBER(result, reg_name, new_val, "r1:0")

#define WRITE_REG_ENCODED(result, reg_name, new_val, encoding) \
    asm volatile("r1 = " reg_name "\n\t" \
                 "r0 = %1\n\t" \
                 encoding "\n\t" \
                 "%0 = " reg_name "\n\t" \
                 "r0 = r1\n\t" \
                 encoding "\n\t" \
                 : "=r"(result) \
                 : "r"(new_val) \
                 : "r0", "r1")

#define WRITE_REG_PAIR_ENCODED(result, reg_name, new_val, encoding) \
    asm volatile("r3:2 = " reg_name "\n\t" \
                 "r1:0 = %1\n\t" \
                 encoding "\n\t" \
                 "%0 = " reg_name "\n\t" \
                 "r1:0 = r3:2\n\t" \
                 encoding "\n\t" \
                 : "=r"(result) \
                 : "r"(new_val) \
                 : "r0", "r1", "r2", "r3")

#endif
