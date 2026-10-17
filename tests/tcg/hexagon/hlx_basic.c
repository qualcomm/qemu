/*
 *  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 * Simple HLX (Hexagon eLement-wise eXtensions) coprocessor test
 *
 * Tests basic load/store/compute using compiler intrinsics.
 *
 * Compile with:
 *   hexagon-clang -mv85 -mhlx -G0 -O2 -static hlx_basic.c -o hlx_basic
 *
 * Run with QEMU:
 *   qemu-system-hexagon -M V85QA_1 -display none -kernel hlx_basic
 * Run with hexagon-sim:
 *   hexagon-sim -mv85qa_1 hlx_basic
 */

#include "hlx_test.h"

/* Test constants */
#define ADD_A           100
#define ADD_B           200
#define PATTERN_EVEN    ((int16_t)0xAAAA)
#define PATTERN_ODD     0x5555

static uint16_t *buf_a;
static uint16_t *buf_b;
static uint16_t *buf_out;
static uint16_t expected[HLX_HALF_ELEMS];

/*
 * Test: Halfword add - validates load/compute/store pipeline
 */
static void test_add_h(void)
{
    HLX_Vector va = Q6_Lh_equals_Rh(ADD_A);
    Q6_mem_Ku16_new_AL(buf_a, va);

    HLX_Vector vb = Q6_Lh_equals_Rh(ADD_B);
    Q6_mem_Ku16_new_AL(buf_b, vb);

    HLX_Vector la = Q6_L_tmp_mem_A(buf_a);
    HLX_Vector lb = Q6_L_tmp_mem_A(buf_b);
    Q6_mem_Ku16_new_AL(buf_out, Q6_Lh_add_LhLh(la, lb));

    for (int i = 0; i < HLX_HALF_ELEMS; i++) {
        expected[i] = ADD_A + ADD_B;
    }
    check_h("add_h", buf_out, expected, HLX_HALF_ELEMS);
}

/*
 * Test: Logical AND
 */
static void test_and(void)
{
    Q6_mem_Ku16_new_AL(buf_a, Q6_Lh_equals_Rh(PATTERN_EVEN));
    Q6_mem_Ku16_new_AL(buf_b, Q6_Lh_equals_Rh(PATTERN_ODD));

    HLX_Vector la = Q6_L_tmp_mem_A(buf_a);
    HLX_Vector lb = Q6_L_tmp_mem_A(buf_b);
    Q6_mem_Ku16_new_AL(buf_out, Q6_L_and_LL(la, lb));

    for (int i = 0; i < HLX_HALF_ELEMS; i++) {
        expected[i] = (uint16_t)PATTERN_EVEN & PATTERN_ODD;
    }
    check_h("and", buf_out, expected, HLX_HALF_ELEMS);
}

/*
 * Test: Logical OR
 */
static void test_or(void)
{
    Q6_mem_Ku16_new_AL(buf_a, Q6_Lh_equals_Rh(PATTERN_EVEN));
    Q6_mem_Ku16_new_AL(buf_b, Q6_Lh_equals_Rh(PATTERN_ODD));

    HLX_Vector la = Q6_L_tmp_mem_A(buf_a);
    HLX_Vector lb = Q6_L_tmp_mem_A(buf_b);
    Q6_mem_Ku16_new_AL(buf_out, Q6_L_or_LL(la, lb));

    for (int i = 0; i < HLX_HALF_ELEMS; i++) {
        expected[i] = (uint16_t)PATTERN_EVEN | PATTERN_ODD;
    }
    check_h("or", buf_out, expected, HLX_HALF_ELEMS);
}

/*
 * Test: Logical NOT
 */
static void test_not(void)
{
    Q6_mem_Ku16_new_AL(buf_a, Q6_Lh_equals_Rh(PATTERN_EVEN));

    HLX_Vector la = Q6_L_tmp_mem_A(buf_a);
    Q6_mem_Ku16_new_AL(buf_out, Q6_L_not_L(la));

    for (int i = 0; i < HLX_HALF_ELEMS; i++) {
        expected[i] = (uint16_t)~(uint16_t)PATTERN_EVEN;
    }
    check_h("not", buf_out, expected, HLX_HALF_ELEMS);
}

int main(void)
{
    init_vtcm();
    enable_hlx();

    buf_a = vtcm_alloc(HLX_HALF_ELEMS * sizeof(uint16_t));
    buf_b = vtcm_alloc(HLX_HALF_ELEMS * sizeof(uint16_t));
    buf_out = vtcm_alloc(HLX_HALF_ELEMS * sizeof(uint16_t));
    init_hlx_vsp();

    test_add_h();
    test_and();
    test_or();
    test_not();

    puts(err ? "FAIL" : "PASS");
    return err;
}
