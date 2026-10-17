/*
 *  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 * HLX multiply tests
 *
 * Tests: LX_mpy_uw_uh, LX_mpyr_uw_uh, LX_mpy_w_huh, LX_mpyr_w_huh,
 *        LX_mpy_w_h, LX_mpyr_w_h,
 *        LX_mpa_wuh, LX_mpar_wuh, LX_mpa_whuh, LX_mpar_whuh,
 *        LX_mpa_wh, LX_mpar_wh
 */

#include "hlx_test.h"

#define MPY_A_UH        100
#define MPY_B_UH        200
#define MPY_A_SH        ((int16_t)-50)
#define MPY_B_SH        ((int16_t)30)
#define MPY_SCALAR_UH   42
#define MPY_SCALAR_SH   ((int16_t)-13)
#define MPY_SCALAR_SUH  50
#define ACC_INIT         5000

/* Pair buffers */
static uint32_t *pw_out_lo;
static uint32_t *pw_out_hi;
static uint32_t *pw_acc_lo;
static uint32_t *pw_acc_hi;
static uint32_t expected_w[HLX_HALF_ELEMS];

/* Single-register buffers (halfword) */
static uint16_t *buf_a;
static uint16_t *buf_b;

/* fLMPY16UU: (uint32_t)((uint16_t)A * (uint16_t)B) */
static inline uint32_t ref_mpy_uu(uint16_t a, uint16_t b)
{
    return (uint32_t)a * (uint32_t)b;
}

/* fLMPY16SU: (int32_t)((int16_t)A * (uint16_t)B) */
static inline int32_t ref_mpy_su(int16_t a, uint16_t b)
{
    return (int32_t)a * (int32_t)(uint32_t)b;
}

/* fLMPY16SS: (int32_t)((int16_t)A * (int16_t)B) */
static inline int32_t ref_mpy_ss(int16_t a, int16_t b)
{
    return (int32_t)a * (int32_t)b;
}

static void fill_h(uint16_t *buf, int16_t val)
{
    Q6_mem_Ku16_new_AL(buf, Q6_Lh_equals_Rh(val));
}

static void fill_w(uint32_t *buf_lo, uint32_t *buf_hi, int32_t val)
{
    HLX_VectorPair wp = Q6_LWw_equals_Rw(val);
    Q6_mem_Ku16_new_AL((uint16_t *)buf_lo, Q6_L_lo_LW(wp));
    Q6_mem_Ku16_new_AL((uint16_t *)buf_hi, Q6_L_hi_LW(wp));
}

/* ===== Widening multiply ===== */

static void test_mpy_uw_uh(void)
{
    fill_h(buf_a, MPY_A_UH);
    fill_h(buf_b, MPY_B_UH);

    HLX_Vector la = Q6_L_tmp_mem_A(buf_a);
    HLX_Vector lb = Q6_L_tmp_mem_A(buf_b);
    HLX_VectorPair result = Q6_LWuw_mpy_LuhLuh(la, lb);
    Q6_mem_Ku16_new_AL((uint16_t *)pw_out_lo, Q6_L_lo_LW(result));
    Q6_mem_Ku16_new_AL((uint16_t *)pw_out_hi, Q6_L_hi_LW(result));

    uint32_t exp = ref_mpy_uu(MPY_A_UH, MPY_B_UH);
    for (int j = 0; j < HLX_HALF_ELEMS; j++) {
        expected_w[j] = exp;
    }
    check_wp("LX_mpy_uw_uh", pw_out_lo, pw_out_hi, expected_w, HLX_HALF_ELEMS);
}

static void test_mpyr_uw_uh(void)
{
    fill_h(buf_a, MPY_A_UH);

    HLX_Vector la = Q6_L_tmp_mem_A(buf_a);
    HLX_VectorPair result = Q6_LWuw_mpy_LuhRuh(la, MPY_SCALAR_UH);
    Q6_mem_Ku16_new_AL((uint16_t *)pw_out_lo, Q6_L_lo_LW(result));
    Q6_mem_Ku16_new_AL((uint16_t *)pw_out_hi, Q6_L_hi_LW(result));

    uint32_t exp = ref_mpy_uu(MPY_A_UH, MPY_SCALAR_UH);
    for (int j = 0; j < HLX_HALF_ELEMS; j++) {
        expected_w[j] = exp;
    }
    check_wp("LX_mpyr_uw_uh", pw_out_lo, pw_out_hi, expected_w, HLX_HALF_ELEMS);
}

static void test_mpy_w_huh(void)
{
    fill_h(buf_a, MPY_A_SH);
    fill_h(buf_b, MPY_B_UH);

    HLX_Vector la = Q6_L_tmp_mem_A(buf_a);
    HLX_Vector lb = Q6_L_tmp_mem_A(buf_b);
    HLX_VectorPair result = Q6_LWw_mpy_LhLuh(la, lb);
    Q6_mem_Ku16_new_AL((uint16_t *)pw_out_lo, Q6_L_lo_LW(result));
    Q6_mem_Ku16_new_AL((uint16_t *)pw_out_hi, Q6_L_hi_LW(result));

    uint32_t exp = (uint32_t)ref_mpy_su(MPY_A_SH, MPY_B_UH);
    for (int j = 0; j < HLX_HALF_ELEMS; j++) {
        expected_w[j] = exp;
    }
    check_wp("LX_mpy_w_huh", pw_out_lo, pw_out_hi, expected_w, HLX_HALF_ELEMS);
}

static void test_mpyr_w_huh(void)
{
    fill_h(buf_a, MPY_A_SH);

    HLX_Vector la = Q6_L_tmp_mem_A(buf_a);
    HLX_VectorPair result = Q6_LWw_mpy_LhRuh(la, MPY_SCALAR_SUH);
    Q6_mem_Ku16_new_AL((uint16_t *)pw_out_lo, Q6_L_lo_LW(result));
    Q6_mem_Ku16_new_AL((uint16_t *)pw_out_hi, Q6_L_hi_LW(result));

    uint32_t exp = (uint32_t)ref_mpy_su(MPY_A_SH, MPY_SCALAR_SUH);
    for (int j = 0; j < HLX_HALF_ELEMS; j++) {
        expected_w[j] = exp;
    }
    check_wp("LX_mpyr_w_huh", pw_out_lo, pw_out_hi, expected_w, HLX_HALF_ELEMS);
}

static void test_mpy_w_h(void)
{
    fill_h(buf_a, MPY_A_SH);
    fill_h(buf_b, MPY_B_SH);

    HLX_Vector la = Q6_L_tmp_mem_A(buf_a);
    HLX_Vector lb = Q6_L_tmp_mem_A(buf_b);
    HLX_VectorPair result = Q6_LWw_mpy_LhLh(la, lb);
    Q6_mem_Ku16_new_AL((uint16_t *)pw_out_lo, Q6_L_lo_LW(result));
    Q6_mem_Ku16_new_AL((uint16_t *)pw_out_hi, Q6_L_hi_LW(result));

    uint32_t exp = (uint32_t)ref_mpy_ss(MPY_A_SH, MPY_B_SH);
    for (int j = 0; j < HLX_HALF_ELEMS; j++) {
        expected_w[j] = exp;
    }
    check_wp("LX_mpy_w_h", pw_out_lo, pw_out_hi, expected_w, HLX_HALF_ELEMS);
}

static void test_mpyr_w_h(void)
{
    fill_h(buf_a, MPY_A_SH);

    HLX_Vector la = Q6_L_tmp_mem_A(buf_a);
    HLX_VectorPair result = Q6_LWw_mpy_LhRh(la, MPY_SCALAR_SH);
    Q6_mem_Ku16_new_AL((uint16_t *)pw_out_lo, Q6_L_lo_LW(result));
    Q6_mem_Ku16_new_AL((uint16_t *)pw_out_hi, Q6_L_hi_LW(result));

    uint32_t exp = (uint32_t)ref_mpy_ss(MPY_A_SH, MPY_SCALAR_SH);
    for (int j = 0; j < HLX_HALF_ELEMS; j++) {
        expected_w[j] = exp;
    }
    check_wp("LX_mpyr_w_h", pw_out_lo, pw_out_hi, expected_w, HLX_HALF_ELEMS);
}

/* ===== Multiply-accumulate ===== */

static void test_mpa_wuh(void)
{
    fill_h(buf_a, MPY_A_UH);
    fill_h(buf_b, MPY_B_UH);
    fill_w(pw_acc_lo, pw_acc_hi, ACC_INIT);

    HLX_Vector la = Q6_L_tmp_mem_A(buf_a);
    HLX_Vector lb = Q6_L_tmp_mem_A(buf_b);
    HLX_VectorPair acc = Q6_LW_combine_LL(Q6_L_tmp_mem_A(pw_acc_hi),
                                            Q6_L_tmp_mem_A(pw_acc_lo));
    HLX_VectorPair result = Q6_LWw_mpyacc_LuhLuh(acc, la, lb);
    Q6_mem_Ku16_new_AL((uint16_t *)pw_out_lo, Q6_L_lo_LW(result));
    Q6_mem_Ku16_new_AL((uint16_t *)pw_out_hi, Q6_L_hi_LW(result));

    uint32_t exp = ACC_INIT + ref_mpy_uu(MPY_A_UH, MPY_B_UH);
    for (int j = 0; j < HLX_HALF_ELEMS; j++) {
        expected_w[j] = exp;
    }
    check_wp("LX_mpa_wuh", pw_out_lo, pw_out_hi, expected_w, HLX_HALF_ELEMS);
}

static void test_mpar_wuh(void)
{
    fill_h(buf_a, MPY_A_UH);
    fill_w(pw_acc_lo, pw_acc_hi, ACC_INIT);

    HLX_Vector la = Q6_L_tmp_mem_A(buf_a);
    HLX_VectorPair acc = Q6_LW_combine_LL(Q6_L_tmp_mem_A(pw_acc_hi),
                                            Q6_L_tmp_mem_A(pw_acc_lo));
    HLX_VectorPair result = Q6_LWw_mpyacc_LuhRuh(acc, la, MPY_SCALAR_UH);
    Q6_mem_Ku16_new_AL((uint16_t *)pw_out_lo, Q6_L_lo_LW(result));
    Q6_mem_Ku16_new_AL((uint16_t *)pw_out_hi, Q6_L_hi_LW(result));

    uint32_t exp = ACC_INIT + ref_mpy_uu(MPY_A_UH, MPY_SCALAR_UH);
    for (int j = 0; j < HLX_HALF_ELEMS; j++) {
        expected_w[j] = exp;
    }
    check_wp("LX_mpar_wuh", pw_out_lo, pw_out_hi, expected_w, HLX_HALF_ELEMS);
}

static void test_mpa_whuh(void)
{
    fill_h(buf_a, MPY_A_SH);
    fill_h(buf_b, MPY_B_UH);
    fill_w(pw_acc_lo, pw_acc_hi, ACC_INIT);

    HLX_Vector la = Q6_L_tmp_mem_A(buf_a);
    HLX_Vector lb = Q6_L_tmp_mem_A(buf_b);
    HLX_VectorPair acc = Q6_LW_combine_LL(Q6_L_tmp_mem_A(pw_acc_hi),
                                            Q6_L_tmp_mem_A(pw_acc_lo));
    HLX_VectorPair result = Q6_LWw_mpyacc_LhLuh(acc, la, lb);
    Q6_mem_Ku16_new_AL((uint16_t *)pw_out_lo, Q6_L_lo_LW(result));
    Q6_mem_Ku16_new_AL((uint16_t *)pw_out_hi, Q6_L_hi_LW(result));

    uint32_t exp = (uint32_t)((int32_t)ACC_INIT +
                   ref_mpy_su(MPY_A_SH, MPY_B_UH));
    for (int j = 0; j < HLX_HALF_ELEMS; j++) {
        expected_w[j] = exp;
    }
    check_wp("LX_mpa_whuh", pw_out_lo, pw_out_hi, expected_w, HLX_HALF_ELEMS);
}

static void test_mpar_whuh(void)
{
    fill_h(buf_a, MPY_A_SH);
    fill_w(pw_acc_lo, pw_acc_hi, ACC_INIT);

    HLX_Vector la = Q6_L_tmp_mem_A(buf_a);
    HLX_VectorPair acc = Q6_LW_combine_LL(Q6_L_tmp_mem_A(pw_acc_hi),
                                            Q6_L_tmp_mem_A(pw_acc_lo));
    HLX_VectorPair result = Q6_LWw_mpyacc_LhRuh(acc, la, MPY_SCALAR_SUH);
    Q6_mem_Ku16_new_AL((uint16_t *)pw_out_lo, Q6_L_lo_LW(result));
    Q6_mem_Ku16_new_AL((uint16_t *)pw_out_hi, Q6_L_hi_LW(result));

    uint32_t exp = (uint32_t)((int32_t)ACC_INIT +
                   ref_mpy_su(MPY_A_SH, MPY_SCALAR_SUH));
    for (int j = 0; j < HLX_HALF_ELEMS; j++) {
        expected_w[j] = exp;
    }
    check_wp("LX_mpar_whuh", pw_out_lo, pw_out_hi, expected_w, HLX_HALF_ELEMS);
}

static void test_mpa_wh(void)
{
    fill_h(buf_a, MPY_A_SH);
    fill_h(buf_b, MPY_B_SH);
    fill_w(pw_acc_lo, pw_acc_hi, ACC_INIT);

    HLX_Vector la = Q6_L_tmp_mem_A(buf_a);
    HLX_Vector lb = Q6_L_tmp_mem_A(buf_b);
    HLX_VectorPair acc = Q6_LW_combine_LL(Q6_L_tmp_mem_A(pw_acc_hi),
                                            Q6_L_tmp_mem_A(pw_acc_lo));
    HLX_VectorPair result = Q6_LWw_mpyacc_LhLh(acc, la, lb);
    Q6_mem_Ku16_new_AL((uint16_t *)pw_out_lo, Q6_L_lo_LW(result));
    Q6_mem_Ku16_new_AL((uint16_t *)pw_out_hi, Q6_L_hi_LW(result));

    uint32_t exp = (uint32_t)((int32_t)ACC_INIT +
                   ref_mpy_ss(MPY_A_SH, MPY_B_SH));
    for (int j = 0; j < HLX_HALF_ELEMS; j++) {
        expected_w[j] = exp;
    }
    check_wp("LX_mpa_wh", pw_out_lo, pw_out_hi, expected_w, HLX_HALF_ELEMS);
}

static void test_mpar_wh(void)
{
    fill_h(buf_a, MPY_A_SH);
    fill_w(pw_acc_lo, pw_acc_hi, ACC_INIT);

    HLX_Vector la = Q6_L_tmp_mem_A(buf_a);
    HLX_VectorPair acc = Q6_LW_combine_LL(Q6_L_tmp_mem_A(pw_acc_hi),
                                            Q6_L_tmp_mem_A(pw_acc_lo));
    HLX_VectorPair result = Q6_LWw_mpyacc_LhRh(acc, la, MPY_SCALAR_SH);
    Q6_mem_Ku16_new_AL((uint16_t *)pw_out_lo, Q6_L_lo_LW(result));
    Q6_mem_Ku16_new_AL((uint16_t *)pw_out_hi, Q6_L_hi_LW(result));

    uint32_t exp = (uint32_t)((int32_t)ACC_INIT +
                   ref_mpy_ss(MPY_A_SH, MPY_SCALAR_SH));
    for (int j = 0; j < HLX_HALF_ELEMS; j++) {
        expected_w[j] = exp;
    }
    check_wp("LX_mpar_wh", pw_out_lo, pw_out_hi, expected_w, HLX_HALF_ELEMS);
}

int main(void)
{
    init_vtcm();
    enable_hlx();

    buf_a = vtcm_alloc(HLX_HALF_ELEMS * sizeof(uint16_t));
    buf_b = vtcm_alloc(HLX_HALF_ELEMS * sizeof(uint16_t));
    pw_out_lo = vtcm_alloc(HLX_WORD_ELEMS * sizeof(uint32_t));
    pw_out_hi = vtcm_alloc(HLX_WORD_ELEMS * sizeof(uint32_t));
    pw_acc_lo = vtcm_alloc(HLX_WORD_ELEMS * sizeof(uint32_t));
    pw_acc_hi = vtcm_alloc(HLX_WORD_ELEMS * sizeof(uint32_t));
    init_hlx_vsp();

    /* Widening multiply */
    test_mpy_uw_uh();
    test_mpyr_uw_uh();
    test_mpy_w_huh();
    test_mpyr_w_huh();
    test_mpy_w_h();
    test_mpyr_w_h();

    /* Multiply-accumulate */
    test_mpa_wuh();
    test_mpar_wuh();
    test_mpa_whuh();
    test_mpar_whuh();
    test_mpa_wh();
    test_mpar_wh();

    puts(err ? "FAIL" : "PASS");
    return err;
}
