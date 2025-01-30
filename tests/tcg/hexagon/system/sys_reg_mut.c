/*
 *  Copyright(c) 2023-2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <stdint.h>
#include <stdio.h>

int err;
#include "../reg_mut.h"
#include "../hex_test.h"


static inline uint32_t ehw(uint64_t val)
{
    return (val & 0xffffffff00000000) >> 32;
}

static inline uint32_t elw(uint64_t val)
{
    return val & 0x00000000ffffffff;
}

static inline uint32_t mask(uint32_t new_val, uint32_t cur_val,
                            uint32_t reg_mask)
{
    return (new_val & ~reg_mask) | (cur_val & reg_mask);
}

static inline uint64_t mask_pair(uint64_t new_val, uint64_t cur_val,
                                 uint32_t reg_mask_high, uint32_t reg_mask_low)
{
    return ((uint64_t) mask(ehw(new_val), ehw(cur_val), reg_mask_high) << 32) |
           mask(elw(new_val), elw(cur_val), reg_mask_low);
}

static inline void write_thread_local_system_control_registers(void)
{
    uint32_t result = 0;
    uint32_t cur_val = 0;
    uint32_t new_val = 0xffffffff;

    READ_WRITE_REG_NOCLOBBER(cur_val, result, "stid", new_val);
    check32(result, mask(new_val, cur_val, 0xff00ff00));

    READ_WRITE_REG_NOCLOBBER(cur_val, result, "elr", new_val);
    check32(result, mask(new_val, cur_val, 0x00000003));

    READ_WRITE_REG_NOCLOBBER(cur_val, result, "ccr", new_val);
    check32(result, mask(new_val, cur_val, 0x10e0ff24));

    READ_WRITE_REG_NOCLOBBER(cur_val, result, "imask", new_val);
    check32(result, mask(new_val, cur_val, 0x00000000));

    READ_WRITE_REG_NOCLOBBER(cur_val, result, "gevb", new_val);
    check32(result, mask(new_val, cur_val, 0x000000ff));
}

static inline void write_global_system_control_registers(void)
{
    uint32_t result = 0;
    uint32_t cur_val = 0;
    uint32_t new_val = 0xffffffff;

    READ_WRITE_REG_NOCLOBBER(cur_val, result, "syscfg", new_val);
    check32(result, mask(new_val, cur_val, 0x80001c00));

    READ_WRITE_REG_NOCLOBBER(cur_val, result, "vid", new_val);
    check32(result, mask(new_val, cur_val, 0xfc00fc00));

    READ_WRITE_REG_NOCLOBBER(cur_val, result, "bestwait", new_val);
    check32(result, mask(new_val, cur_val, 0xfffffe00));

    READ_WRITE_REG_NOCLOBBER(cur_val, result, "schedcfg", new_val);
    check32(result, mask(new_val, cur_val, 0xfffffee0));

    READ_WRITE_REG_NOCLOBBER(cur_val, result, "isdbcfg0", new_val);
    check32(result, mask(new_val, cur_val, 0xe0000000));

    READ_WRITE_REG_NOCLOBBER(cur_val, result, "brkptpc0", new_val);
    check32(result, mask(new_val, cur_val, 0x00000003));

    READ_WRITE_REG_NOCLOBBER(cur_val, result, "brkptcfg0", new_val);
    check32(result, mask(new_val, cur_val, 0xfc007000));

    READ_WRITE_REG_NOCLOBBER(cur_val, result, "brkptpc1", new_val);
    check32(result, mask(new_val, cur_val, 0x00000003));

    READ_WRITE_REG_NOCLOBBER(cur_val, result, "brkptcfg1", new_val);
    check32(result, mask(new_val, cur_val, 0xfc007000));

    READ_WRITE_REG_NOCLOBBER(cur_val, result, "isdben", new_val);
    check32(result, mask(new_val, cur_val, 0xfffffffe));
}

static inline void write_thread_local_system_control_register_pairs(void)
{
    uint64_t result = 0;
    uint64_t cur_val = 0;
    uint64_t new_val = 0xffffffffffffffff;

    READ_WRITE_REG_NOCLOBBER(cur_val, result, "s3:2", new_val);
    check64(result, mask_pair(new_val, cur_val, 0x00000003, 0xff00ff00));

    READ_WRITE_REG_NOCLOBBER(cur_val, result, "s11:10", new_val);
    check64(result, mask_pair(new_val, cur_val, 0x000000ff, 0x00000000));
}

/*
 * Instruction { sX:Y = r1:0 }
 *
 * This instruction is barred by the assembler.
 *
 * 01101101000 00000 11 0000000 yyyyyyy
 * Where y's are the Y's bits.
 *
 * Generated with this python function:
 * lambda n : hex(int('0110110100000000110000000' + format(n, '#09b')[2:], 2))
 */
#define S21_20_EQ_R1_0 ".word 0x6d00c014"
#define S27_26_EQ_R1_0 ".word 0x6d00c01a"
#define S29_28_EQ_R1_0 ".word 0x6d00c01c"
#define S33_32_EQ_R1_0 ".word 0x6d00c020"
#define S41_40_EQ_R1_0 ".word 0x6d00c028"
#define S57_56_EQ_R1_0 ".word 0x6d00c038"

static inline void write_global_system_control_register_pairs(void)
{
    uint64_t result = 0;
    uint64_t cur_val = 0;
    uint64_t new_val = 0xffffffffffffffff;

    READ_WRITE_REG_NOCLOBBER(cur_val, result, "s19:18", new_val);
    check64(result, mask_pair(new_val, cur_val, 0x00000000, 0x80001c00));

    READ_WRITE_REG_ENCODED(cur_val, result, "s21:20", new_val, S21_20_EQ_R1_0);
    check64(result, mask_pair(new_val, cur_val, 0xfc00fc00, 0xffffffff));

    READ_WRITE_REG_NOCLOBBER(cur_val, result, "s25:24", new_val);
    check64(result, mask_pair(new_val, cur_val, 0xfffffee0, 0x00000000));

    READ_WRITE_REG_ENCODED(cur_val, result, "s27:26", new_val, S27_26_EQ_R1_0);
    check64(result, mask_pair(new_val, cur_val, 0xffffffff, 0x00000000));

    READ_WRITE_REG_ENCODED(cur_val, result, "s29:28", new_val, S29_28_EQ_R1_0);
    check64(result, mask_pair(new_val, cur_val, 0xffffffff, 0x00000000));

    READ_WRITE_REG_ENCODED(cur_val, result, "s33:32", new_val, S33_32_EQ_R1_0);
    check64(result, mask_pair(new_val, cur_val, 0xe0000000, 0xffffffff));

    READ_WRITE_REG_NOCLOBBER(cur_val, result, "s37:36", new_val);
    check64(result, mask_pair(new_val, cur_val, 0xfc007000, 0x00000003));

    READ_WRITE_REG_NOCLOBBER(cur_val, result, "s39:38", new_val);
    check64(result, mask_pair(new_val, cur_val, 0xfc007000, 0x00000003));

    READ_WRITE_REG_ENCODED(cur_val, result, "s41:40", new_val, S41_40_EQ_R1_0);
    check64(result, mask_pair(new_val, cur_val, 0x00000000, 0xffffffff));

    READ_WRITE_REG_NOCLOBBER(cur_val, result, "s43:42", new_val);
    check64(result, mask_pair(new_val, cur_val, 0x00000000, 0xfffffffe));

    READ_WRITE_REG_ENCODED(cur_val, result, "s57:56", new_val, S57_56_EQ_R1_0);
    check64(result, mask_pair(new_val, cur_val, 0xffffffff, 0xffffffff));
}

int main()
{
    write_thread_local_system_control_registers();
    write_global_system_control_registers();
    write_thread_local_system_control_register_pairs();
    write_global_system_control_register_pairs();

    printf("%s\n", ((err) ? "FAIL" : "PASS"));
    return err;
}
