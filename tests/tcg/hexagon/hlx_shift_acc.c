/*
 *  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 * HLX shift-accumulate tests
 *
 * Tests: LX_shra_h_uh, LX_shrar_h_uh, LX_shra_h_h, LX_shrar_h_h,
 *        LX_shra_w_uw, LX_shrar_w_uw, LX_shra_w_w, LX_shrar_w_w
 */

#include "hlx_test.h"

/* Named constants */
#define ACC_H            1000
#define SRC_H            0x1234
#define SHAMT_H          4
#define ACC_W            50000
#define SRC_W            0x00123400
#define SHAMT_W          8

/* Single-register buffers */
static uint16_t *buf_src;
static uint16_t *buf_shamt;
static uint16_t *buf_acc;
static uint16_t *buf_out;
static uint16_t expected[HLX_HALF_ELEMS];

/* Pair buffers for word accumulate */
static uint32_t *pw_src_lo;
static uint32_t *pw_src_hi;
static uint32_t *pw_acc_lo;
static uint32_t *pw_acc_hi;
static uint32_t *pw_out_lo;
static uint32_t *pw_out_hi;
static uint32_t expected_w[HLX_HALF_ELEMS];

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

/*
 * fLSHIFTU/fLSHIFT: shamt <= 0 -> left shift, shamt > 0 -> right shift
 */
static inline uint16_t ref_lshiftu16(uint16_t val, int16_t shamt)
{
    if (shamt <= 0) {
        int n = -shamt;
        if (n > 32) {
            n = 32;
        }
        return (uint16_t)((uint64_t)val << n);
    }
    int n = shamt > 32 ? 32 : shamt;
    return (uint16_t)((uint64_t)val >> n);
}

static inline int16_t ref_lshift16(int16_t val, int16_t shamt)
{
    if (shamt <= 0) {
        int n = -shamt;
        if (n > 32) {
            n = 32;
        }
        return (int16_t)(uint16_t)((int64_t)val << n);
    }
    int n = shamt > 32 ? 32 : shamt;
    return (int16_t)(uint16_t)((int64_t)val >> n);
}

static inline uint32_t ref_lshiftu32(uint32_t val, int16_t shamt)
{
    if (shamt <= 0) {
        int n = -shamt;
        if (n > 32) {
            n = 32;
        }
        return (uint32_t)((uint64_t)val << n);
    }
    int n = shamt > 32 ? 32 : shamt;
    return (uint32_t)((uint64_t)val >> n);
}

static inline int32_t ref_lshift32(int32_t val, int16_t shamt)
{
    if (shamt <= 0) {
        int n = -shamt;
        if (n > 32) {
            n = 32;
        }
        return (int32_t)(uint32_t)((int64_t)val << n);
    }
    int n = shamt > 32 ? 32 : shamt;
    return (int32_t)(uint32_t)((int64_t)val >> n);
}

/* ===== Halfword shift-accumulate ===== */

static void test_shra_h_uh(void)
{
    fill_h(buf_acc, ACC_H);
    fill_h(buf_src, (int16_t)SRC_H);
    fill_h(buf_shamt, (int16_t)SHAMT_H);

    HLX_Vector lacc = Q6_L_tmp_mem_A(buf_acc);
    HLX_Vector lsrc = Q6_L_tmp_mem_A(buf_src);
    HLX_Vector lshamt = Q6_L_tmp_mem_A(buf_shamt);
    Q6_mem_Ku16_new_AL(buf_out, Q6_Lh_shracc_LuhLh(lacc, lsrc, lshamt));

    uint16_t exp = (uint16_t)((int16_t)ACC_H +
                   (int16_t)ref_lshiftu16(SRC_H, SHAMT_H));
    for (int i = 0; i < HLX_HALF_ELEMS; i++) {
        expected[i] = exp;
    }
    check_h("LX_shra_h_uh", buf_out, expected, HLX_HALF_ELEMS);
}

static void test_shrar_h_uh(void)
{
    fill_h(buf_acc, ACC_H);
    fill_h(buf_src, (int16_t)SRC_H);
    int16_t scalar = -SHAMT_H;

    HLX_Vector lacc = Q6_L_tmp_mem_A(buf_acc);
    HLX_Vector lsrc = Q6_L_tmp_mem_A(buf_src);
    Q6_mem_Ku16_new_AL(buf_out, Q6_Lh_shracc_LuhRh(lacc, lsrc, scalar));

    uint16_t exp = (uint16_t)((int16_t)ACC_H +
                   (int16_t)ref_lshiftu16(SRC_H, scalar));
    for (int i = 0; i < HLX_HALF_ELEMS; i++) {
        expected[i] = exp;
    }
    check_h("LX_shrar_h_uh", buf_out, expected, HLX_HALF_ELEMS);
}

static void test_shra_h_h(void)
{
    fill_h(buf_acc, ACC_H);
    fill_h(buf_src, (int16_t)SRC_H);
    fill_h(buf_shamt, (int16_t)SHAMT_H);

    HLX_Vector lacc = Q6_L_tmp_mem_A(buf_acc);
    HLX_Vector lsrc = Q6_L_tmp_mem_A(buf_src);
    HLX_Vector lshamt = Q6_L_tmp_mem_A(buf_shamt);
    Q6_mem_Ku16_new_AL(buf_out, Q6_Lh_shracc_LhLh(lacc, lsrc, lshamt));

    uint16_t exp = (uint16_t)((int16_t)ACC_H +
                   ref_lshift16((int16_t)SRC_H, SHAMT_H));
    for (int i = 0; i < HLX_HALF_ELEMS; i++) {
        expected[i] = exp;
    }
    check_h("LX_shra_h_h", buf_out, expected, HLX_HALF_ELEMS);
}

static void test_shrar_h_h(void)
{
    fill_h(buf_acc, ACC_H);
    fill_h(buf_src, (int16_t)SRC_H);
    int16_t scalar = -SHAMT_H;

    HLX_Vector lacc = Q6_L_tmp_mem_A(buf_acc);
    HLX_Vector lsrc = Q6_L_tmp_mem_A(buf_src);
    Q6_mem_Ku16_new_AL(buf_out, Q6_Lh_shracc_LhRh(lacc, lsrc, scalar));

    uint16_t exp = (uint16_t)((int16_t)ACC_H +
                   ref_lshift16((int16_t)SRC_H, scalar));
    for (int i = 0; i < HLX_HALF_ELEMS; i++) {
        expected[i] = exp;
    }
    check_h("LX_shrar_h_h", buf_out, expected, HLX_HALF_ELEMS);
}

/* ===== Word shift-accumulate ===== */

static void test_shra_w_uw(void)
{
    fill_w(pw_acc_lo, pw_acc_hi, ACC_W);
    fill_w(pw_src_lo, pw_src_hi, (int32_t)SRC_W);
    fill_h(buf_shamt, (int16_t)SHAMT_W);

    HLX_VectorPair acc = Q6_LW_combine_LL(Q6_L_tmp_mem_A(pw_acc_hi),
                                            Q6_L_tmp_mem_A(pw_acc_lo));
    HLX_VectorPair src = Q6_LW_combine_LL(Q6_L_tmp_mem_A(pw_src_hi),
                                            Q6_L_tmp_mem_A(pw_src_lo));
    HLX_Vector lshamt = Q6_L_tmp_mem_A(buf_shamt);
    HLX_VectorPair result = Q6_LWw_shracc_LWuwLh(acc, src, lshamt);
    Q6_mem_Ku16_new_AL((uint16_t *)pw_out_lo, Q6_L_lo_LW(result));
    Q6_mem_Ku16_new_AL((uint16_t *)pw_out_hi, Q6_L_hi_LW(result));

    uint32_t exp = ACC_W + ref_lshiftu32(SRC_W, SHAMT_W);
    for (int j = 0; j < HLX_HALF_ELEMS; j++) {
        expected_w[j] = exp;
    }
    check_wp("LX_shra_w_uw", pw_out_lo, pw_out_hi, expected_w, HLX_HALF_ELEMS);
}

static void test_shrar_w_uw(void)
{
    fill_w(pw_acc_lo, pw_acc_hi, ACC_W);
    fill_w(pw_src_lo, pw_src_hi, (int32_t)SRC_W);
    int16_t scalar = -SHAMT_W;

    HLX_VectorPair acc = Q6_LW_combine_LL(Q6_L_tmp_mem_A(pw_acc_hi),
                                            Q6_L_tmp_mem_A(pw_acc_lo));
    HLX_VectorPair src = Q6_LW_combine_LL(Q6_L_tmp_mem_A(pw_src_hi),
                                            Q6_L_tmp_mem_A(pw_src_lo));
    HLX_VectorPair result = Q6_LWw_shracc_LWuwRh(acc, src, scalar);
    Q6_mem_Ku16_new_AL((uint16_t *)pw_out_lo, Q6_L_lo_LW(result));
    Q6_mem_Ku16_new_AL((uint16_t *)pw_out_hi, Q6_L_hi_LW(result));

    uint32_t exp = ACC_W + ref_lshiftu32(SRC_W, scalar);
    for (int j = 0; j < HLX_HALF_ELEMS; j++) {
        expected_w[j] = exp;
    }
    check_wp("LX_shrar_w_uw", pw_out_lo, pw_out_hi, expected_w, HLX_HALF_ELEMS);
}

static void test_shra_w_w(void)
{
    fill_w(pw_acc_lo, pw_acc_hi, ACC_W);
    fill_w(pw_src_lo, pw_src_hi, (int32_t)SRC_W);
    fill_h(buf_shamt, (int16_t)SHAMT_W);

    HLX_VectorPair acc = Q6_LW_combine_LL(Q6_L_tmp_mem_A(pw_acc_hi),
                                            Q6_L_tmp_mem_A(pw_acc_lo));
    HLX_VectorPair src = Q6_LW_combine_LL(Q6_L_tmp_mem_A(pw_src_hi),
                                            Q6_L_tmp_mem_A(pw_src_lo));
    HLX_Vector lshamt = Q6_L_tmp_mem_A(buf_shamt);
    HLX_VectorPair result = Q6_LWw_shracc_LWwLh(acc, src, lshamt);
    Q6_mem_Ku16_new_AL((uint16_t *)pw_out_lo, Q6_L_lo_LW(result));
    Q6_mem_Ku16_new_AL((uint16_t *)pw_out_hi, Q6_L_hi_LW(result));

    uint32_t exp = (uint32_t)((int32_t)ACC_W +
                   ref_lshift32((int32_t)SRC_W, SHAMT_W));
    for (int j = 0; j < HLX_HALF_ELEMS; j++) {
        expected_w[j] = exp;
    }
    check_wp("LX_shra_w_w", pw_out_lo, pw_out_hi, expected_w, HLX_HALF_ELEMS);
}

static void test_shrar_w_w(void)
{
    fill_w(pw_acc_lo, pw_acc_hi, ACC_W);
    fill_w(pw_src_lo, pw_src_hi, (int32_t)SRC_W);
    int16_t scalar = -SHAMT_W;

    HLX_VectorPair acc = Q6_LW_combine_LL(Q6_L_tmp_mem_A(pw_acc_hi),
                                            Q6_L_tmp_mem_A(pw_acc_lo));
    HLX_VectorPair src = Q6_LW_combine_LL(Q6_L_tmp_mem_A(pw_src_hi),
                                            Q6_L_tmp_mem_A(pw_src_lo));
    HLX_VectorPair result = Q6_LWw_shracc_LWwRh(acc, src, scalar);
    Q6_mem_Ku16_new_AL((uint16_t *)pw_out_lo, Q6_L_lo_LW(result));
    Q6_mem_Ku16_new_AL((uint16_t *)pw_out_hi, Q6_L_hi_LW(result));

    uint32_t exp = (uint32_t)((int32_t)ACC_W +
                   ref_lshift32((int32_t)SRC_W, scalar));
    for (int j = 0; j < HLX_HALF_ELEMS; j++) {
        expected_w[j] = exp;
    }
    check_wp("LX_shrar_w_w", pw_out_lo, pw_out_hi, expected_w, HLX_HALF_ELEMS);
}

int main(void)
{
    init_vtcm();
    enable_hlx();

    buf_src = vtcm_alloc(HLX_HALF_ELEMS * sizeof(uint16_t));
    buf_shamt = vtcm_alloc(HLX_HALF_ELEMS * sizeof(uint16_t));
    buf_acc = vtcm_alloc(HLX_HALF_ELEMS * sizeof(uint16_t));
    buf_out = vtcm_alloc(HLX_HALF_ELEMS * sizeof(uint16_t));
    pw_src_lo = vtcm_alloc(HLX_WORD_ELEMS * sizeof(uint32_t));
    pw_src_hi = vtcm_alloc(HLX_WORD_ELEMS * sizeof(uint32_t));
    pw_acc_lo = vtcm_alloc(HLX_WORD_ELEMS * sizeof(uint32_t));
    pw_acc_hi = vtcm_alloc(HLX_WORD_ELEMS * sizeof(uint32_t));
    pw_out_lo = vtcm_alloc(HLX_WORD_ELEMS * sizeof(uint32_t));
    pw_out_hi = vtcm_alloc(HLX_WORD_ELEMS * sizeof(uint32_t));
    init_hlx_vsp();

    /* Halfword shift-accumulate */
    test_shra_h_uh();
    test_shrar_h_uh();
    test_shra_h_h();
    test_shrar_h_h();

    /* Word shift-accumulate */
    test_shra_w_uw();
    test_shrar_w_uw();
    test_shra_w_w();
    test_shrar_w_w();

    puts(err ? "FAIL" : "PASS");
    return err;
}
