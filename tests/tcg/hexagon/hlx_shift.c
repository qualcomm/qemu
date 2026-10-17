/*
 *  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 * HLX shift operation tests
 *
 * Tests: LX_shr_uh_uh, LX_shrr_uh_uh, LX_shr_h, LX_shrr_h,
 *        LX_shr_uw_uw, LX_shrr_uw_uw, LX_shr_w_w, LX_shrr_w_w,
 *        LX_shr_h_w_uh, LX_shrr_h_w_uh,
 *        LX_shr_h_w_sat, LX_shrr_h_w_sat,
 *        LX_shr_h_w_rnd_sat, LX_shrr_h_w_rnd_sat,
 *        LX_shr_uh_uw_sat, LX_shrr_uh_uw_sat,
 *        LX_shr_uh_uw_rnd_sat, LX_shrr_uh_uw_rnd_sat,
 *        LX_shr_uh_w_sat, LX_shrr_uh_w_sat,
 *        LX_shr_uh_w_rnd_sat, LX_shrr_uh_w_rnd_sat
 */

#include "hlx_test.h"

/* Named constants */
#define SRC_UH           0x5678
#define SRC_SH           ((int16_t)-1234)
#define SHAMT_H          4
#define SRC_UW           0x12345678u
#define SRC_SW           ((int32_t)-12345678)
#define SHAMT_W          8
#define NARROW_SRC_W     0x00ABCD00
#define NARROW_SHAMT     8

/* Single-register buffers */
static uint16_t *buf_src;
static uint16_t *buf_shamt;
static uint16_t *buf_out;
static uint16_t expected[HLX_HALF_ELEMS];

/* Pair buffers */
static uint32_t *pw_src_lo;
static uint32_t *pw_src_hi;
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
 * fLSHIFTU/fLSHIFT: bidirectional shift.
 * Convention: shamt <= 0 -> LEFT shift by |shamt|
 *             shamt >  0 -> RIGHT shift by shamt
 * Matches the macros in target/hexagon/macros.h.
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
        return (int16_t)(uint16_t)((uint64_t)(int64_t)val << n);
    }
    int n = shamt > 32 ? 32 : shamt;
    return (int16_t)(uint16_t)((int64_t)val >> n);
}

static inline uint64_t ref_lshiftu32_64(uint32_t val, int16_t shamt)
{
    if (shamt <= 0) {
        int n = -shamt;
        if (n > 32) {
            n = 32;
        }
        return (uint64_t)val << n;
    }
    int n = shamt > 32 ? 32 : shamt;
    return (uint64_t)val >> n;
}

static inline uint32_t ref_lshiftu32(uint32_t val, int16_t shamt)
{
    return (uint32_t)ref_lshiftu32_64(val, shamt);
}

static inline int64_t ref_lshift32_64(int32_t val, int16_t shamt)
{
    if (shamt <= 0) {
        int n = -shamt;
        if (n > 32) {
            n = 32;
        }
        return (int64_t)((uint64_t)(int64_t)val << n);
    }
    int n = shamt > 32 ? 32 : shamt;
    return (int64_t)val >> n;
}

static inline int32_t ref_lshift32(int32_t val, int16_t shamt)
{
    return (int32_t)(uint32_t)ref_lshift32_64(val, shamt);
}

static inline int64_t ref_lshift32_rnd_64(int32_t val, int16_t shamt)
{
    if (shamt <= 0) {
        int n = -shamt;
        if (n > 32) {
            n = 32;
        }
        return (int64_t)((uint64_t)(int64_t)val << n);
    }
    int nm1 = shamt - 1;
    if (nm1 > 32) {
        nm1 = 32;
    }
    return (((int64_t)val >> nm1) + 1) >> 1;
}

static inline int32_t ref_lshift32_rnd(int32_t val, int16_t shamt)
{
    return (int32_t)(uint32_t)ref_lshift32_rnd_64(val, shamt);
}

static inline uint64_t ref_lshiftu32_rnd_64(uint32_t val, int16_t shamt)
{
    if (shamt <= 0) {
        int n = -shamt;
        if (n > 32) {
            n = 32;
        }
        return (uint64_t)val << n;
    }
    int nm1 = shamt - 1;
    if (nm1 > 32) {
        nm1 = 32;
    }
    return (((uint64_t)val >> nm1) + 1) >> 1;
}

static inline uint32_t ref_lshiftu32_rnd(uint32_t val, int16_t shamt)
{
    return (uint32_t)ref_lshiftu32_rnd_64(val, shamt);
}

/* 64-bit saturation helpers for narrowing shift results */
static inline int16_t sat_h_64(int64_t val)
{
    if (val > 32767) {
        return 32767;
    }
    if (val < -32768) {
        return -32768;
    }
    return (int16_t)val;
}

static inline uint16_t sat_uh64(uint64_t val)
{
    return (val > 0xFFFF) ? 0xFFFF : (uint16_t)val;
}

static inline uint16_t sat_uh64s(int64_t val)
{
    if (val < 0) {
        return 0;
    }
    if (val > 0xFFFF) {
        return 0xFFFF;
    }
    return (uint16_t)val;
}

/* ===== Basic halfword shifts ===== */

static void test_shr_uh_uh(void)
{
    fill_h(buf_src, (int16_t)SRC_UH);
    fill_h(buf_shamt, (int16_t)SHAMT_H);

    HLX_Vector la = Q6_L_tmp_mem_A(buf_src);
    HLX_Vector lshamt = Q6_L_tmp_mem_A(buf_shamt);
    Q6_mem_Ku16_new_AL(buf_out, Q6_Luh_shr_LuhLh(la, lshamt));

    uint16_t exp = ref_lshiftu16(SRC_UH, SHAMT_H);
    for (int i = 0; i < HLX_HALF_ELEMS; i++) {
        expected[i] = exp;
    }
    check_h("LX_shr_uh_uh", buf_out, expected, HLX_HALF_ELEMS);
}

static void test_shrr_uh_uh(void)
{
    fill_h(buf_src, (int16_t)SRC_UH);
    int16_t scalar = -SHAMT_H;

    HLX_Vector la = Q6_L_tmp_mem_A(buf_src);
    Q6_mem_Ku16_new_AL(buf_out, Q6_Luh_shr_LuhRh(la, scalar));

    uint16_t exp = ref_lshiftu16(SRC_UH, scalar);
    for (int i = 0; i < HLX_HALF_ELEMS; i++) {
        expected[i] = exp;
    }
    check_h("LX_shrr_uh_uh", buf_out, expected, HLX_HALF_ELEMS);
}

static void test_shr_h(void)
{
    fill_h(buf_src, SRC_SH);
    fill_h(buf_shamt, (int16_t)SHAMT_H);

    HLX_Vector la = Q6_L_tmp_mem_A(buf_src);
    HLX_Vector lshamt = Q6_L_tmp_mem_A(buf_shamt);
    Q6_mem_Ku16_new_AL(buf_out, Q6_Lh_shr_LhLh(la, lshamt));

    uint16_t exp = (uint16_t)ref_lshift16(SRC_SH, SHAMT_H);
    for (int i = 0; i < HLX_HALF_ELEMS; i++) {
        expected[i] = exp;
    }
    check_h("LX_shr_h", buf_out, expected, HLX_HALF_ELEMS);
}

static void test_shrr_h(void)
{
    fill_h(buf_src, SRC_SH);
    int16_t scalar = -SHAMT_H;

    HLX_Vector la = Q6_L_tmp_mem_A(buf_src);
    Q6_mem_Ku16_new_AL(buf_out, Q6_Lh_shr_LhRh(la, scalar));

    uint16_t exp = (uint16_t)ref_lshift16(SRC_SH, scalar);
    for (int i = 0; i < HLX_HALF_ELEMS; i++) {
        expected[i] = exp;
    }
    check_h("LX_shrr_h", buf_out, expected, HLX_HALF_ELEMS);
}

/* ===== Word pair shifts ===== */

static void test_shr_uw_uw(void)
{
    fill_w(pw_src_lo, pw_src_hi, (int32_t)SRC_UW);
    fill_h(buf_shamt, (int16_t)SHAMT_W);

    HLX_VectorPair pa = Q6_LW_combine_LL(Q6_L_tmp_mem_A(pw_src_hi),
                                           Q6_L_tmp_mem_A(pw_src_lo));
    HLX_Vector lshamt = Q6_L_tmp_mem_A(buf_shamt);
    HLX_VectorPair result = Q6_LWuw_shr_LWuwLh(pa, lshamt);
    Q6_mem_Ku16_new_AL((uint16_t *)pw_out_lo, Q6_L_lo_LW(result));
    Q6_mem_Ku16_new_AL((uint16_t *)pw_out_hi, Q6_L_hi_LW(result));

    for (int j = 0; j < HLX_HALF_ELEMS; j++) {
        expected_w[j] = ref_lshiftu32(SRC_UW, SHAMT_W);
    }
    check_wp("LX_shr_uw_uw", pw_out_lo, pw_out_hi, expected_w, HLX_HALF_ELEMS);
}

static void test_shrr_uw_uw(void)
{
    fill_w(pw_src_lo, pw_src_hi, (int32_t)SRC_UW);
    int16_t scalar = -SHAMT_W;

    HLX_VectorPair pa = Q6_LW_combine_LL(Q6_L_tmp_mem_A(pw_src_hi),
                                           Q6_L_tmp_mem_A(pw_src_lo));
    HLX_VectorPair result = Q6_LWuw_shr_LWuwRh(pa, scalar);
    Q6_mem_Ku16_new_AL((uint16_t *)pw_out_lo, Q6_L_lo_LW(result));
    Q6_mem_Ku16_new_AL((uint16_t *)pw_out_hi, Q6_L_hi_LW(result));

    for (int j = 0; j < HLX_HALF_ELEMS; j++) {
        expected_w[j] = ref_lshiftu32(SRC_UW, scalar);
    }
    check_wp("LX_shrr_uw_uw", pw_out_lo, pw_out_hi, expected_w, HLX_HALF_ELEMS);
}

static void test_shr_w_w(void)
{
    fill_w(pw_src_lo, pw_src_hi, SRC_SW);
    fill_h(buf_shamt, (int16_t)SHAMT_W);

    HLX_VectorPair pa = Q6_LW_combine_LL(Q6_L_tmp_mem_A(pw_src_hi),
                                           Q6_L_tmp_mem_A(pw_src_lo));
    HLX_Vector lshamt = Q6_L_tmp_mem_A(buf_shamt);
    HLX_VectorPair result = Q6_LWw_shr_LWwLh(pa, lshamt);
    Q6_mem_Ku16_new_AL((uint16_t *)pw_out_lo, Q6_L_lo_LW(result));
    Q6_mem_Ku16_new_AL((uint16_t *)pw_out_hi, Q6_L_hi_LW(result));

    for (int j = 0; j < HLX_HALF_ELEMS; j++) {
        expected_w[j] = (uint32_t)ref_lshift32(SRC_SW, SHAMT_W);
    }
    check_wp("LX_shr_w_w", pw_out_lo, pw_out_hi, expected_w, HLX_HALF_ELEMS);
}

static void test_shrr_w_w(void)
{
    fill_w(pw_src_lo, pw_src_hi, SRC_SW);
    int16_t scalar = -SHAMT_W;

    HLX_VectorPair pa = Q6_LW_combine_LL(Q6_L_tmp_mem_A(pw_src_hi),
                                           Q6_L_tmp_mem_A(pw_src_lo));
    HLX_VectorPair result = Q6_LWw_shr_LWwRh(pa, scalar);
    Q6_mem_Ku16_new_AL((uint16_t *)pw_out_lo, Q6_L_lo_LW(result));
    Q6_mem_Ku16_new_AL((uint16_t *)pw_out_hi, Q6_L_hi_LW(result));

    for (int j = 0; j < HLX_HALF_ELEMS; j++) {
        expected_w[j] = (uint32_t)ref_lshift32(SRC_SW, scalar);
    }
    check_wp("LX_shrr_w_w", pw_out_lo, pw_out_hi, expected_w, HLX_HALF_ELEMS);
}

/* ===== Narrowing shifts (word pair -> halfword) ===== */

static void test_shr_h_w_uh(void)
{
    fill_w(pw_src_lo, pw_src_hi, (int32_t)NARROW_SRC_W);
    fill_h(buf_shamt, (int16_t)NARROW_SHAMT);

    HLX_VectorPair pa = Q6_LW_combine_LL(Q6_L_tmp_mem_A(pw_src_hi),
                                           Q6_L_tmp_mem_A(pw_src_lo));
    HLX_Vector lshamt = Q6_L_tmp_mem_A(buf_shamt);
    Q6_mem_Ku16_new_AL(buf_out, Q6_Lh_shr_LWwLuh(pa, lshamt));

    uint16_t exp = (uint16_t)((int32_t)NARROW_SRC_W >> (NARROW_SHAMT & 15));
    for (int i = 0; i < HLX_HALF_ELEMS; i++) {
        expected[i] = exp;
    }
    check_h("LX_shr_h_w_uh", buf_out, expected, HLX_HALF_ELEMS);
}

static void test_shrr_h_w_uh(void)
{
    fill_w(pw_src_lo, pw_src_hi, (int32_t)NARROW_SRC_W);
    uint16_t scalar = NARROW_SHAMT;

    HLX_VectorPair pa = Q6_LW_combine_LL(Q6_L_tmp_mem_A(pw_src_hi),
                                           Q6_L_tmp_mem_A(pw_src_lo));
    Q6_mem_Ku16_new_AL(buf_out, Q6_Lh_shr_LWwRuh(pa, scalar));

    uint16_t exp = (uint16_t)((int32_t)NARROW_SRC_W >> (scalar & 15));
    for (int i = 0; i < HLX_HALF_ELEMS; i++) {
        expected[i] = exp;
    }
    check_h("LX_shrr_h_w_uh", buf_out, expected, HLX_HALF_ELEMS);
}

/* ===== Narrowing shifts with saturation ===== */

static void test_shr_h_w_sat(void)
{
    fill_w(pw_src_lo, pw_src_hi, SRC_SW);
    fill_h(buf_shamt, (int16_t)SHAMT_W);

    HLX_VectorPair pa = Q6_LW_combine_LL(Q6_L_tmp_mem_A(pw_src_hi),
                                           Q6_L_tmp_mem_A(pw_src_lo));
    HLX_Vector lshamt = Q6_L_tmp_mem_A(buf_shamt);
    Q6_mem_Ku16_new_AL(buf_out, Q6_Lh_shr_LWwLh_sat(pa, lshamt));

    uint16_t exp = (uint16_t)sat_h_64(ref_lshift32_64(SRC_SW, SHAMT_W));
    for (int i = 0; i < HLX_HALF_ELEMS; i++) {
        expected[i] = exp;
    }
    check_h("LX_shr_h_w_sat", buf_out, expected, HLX_HALF_ELEMS);
}

static void test_shrr_h_w_sat(void)
{
    fill_w(pw_src_lo, pw_src_hi, SRC_SW);
    int16_t scalar = -SHAMT_H;

    HLX_VectorPair pa = Q6_LW_combine_LL(Q6_L_tmp_mem_A(pw_src_hi),
                                           Q6_L_tmp_mem_A(pw_src_lo));
    Q6_mem_Ku16_new_AL(buf_out, Q6_Lh_shr_LWwRh_sat(pa, scalar));

    uint16_t exp = (uint16_t)sat_h_64(ref_lshift32_64(SRC_SW, scalar));
    for (int i = 0; i < HLX_HALF_ELEMS; i++) {
        expected[i] = exp;
    }
    check_h("LX_shrr_h_w_sat", buf_out, expected, HLX_HALF_ELEMS);
}

static void test_shr_h_w_rnd_sat(void)
{
    fill_w(pw_src_lo, pw_src_hi, SRC_SW);
    fill_h(buf_shamt, (int16_t)SHAMT_W);

    HLX_VectorPair pa = Q6_LW_combine_LL(Q6_L_tmp_mem_A(pw_src_hi),
                                           Q6_L_tmp_mem_A(pw_src_lo));
    HLX_Vector lshamt = Q6_L_tmp_mem_A(buf_shamt);
    Q6_mem_Ku16_new_AL(buf_out, Q6_Lh_shr_LWwLh_rnd_sat(pa, lshamt));

    uint16_t exp = (uint16_t)sat_h_64(ref_lshift32_rnd_64(SRC_SW, SHAMT_W));
    for (int i = 0; i < HLX_HALF_ELEMS; i++) {
        expected[i] = exp;
    }
    check_h("LX_shr_h_w_rnd_sat", buf_out, expected, HLX_HALF_ELEMS);
}

static void test_shrr_h_w_rnd_sat(void)
{
    fill_w(pw_src_lo, pw_src_hi, SRC_SW);
    int16_t scalar = -SHAMT_W;

    HLX_VectorPair pa = Q6_LW_combine_LL(Q6_L_tmp_mem_A(pw_src_hi),
                                           Q6_L_tmp_mem_A(pw_src_lo));
    Q6_mem_Ku16_new_AL(buf_out, Q6_Lh_shr_LWwRh_rnd_sat(pa, scalar));

    uint16_t exp = (uint16_t)sat_h_64(ref_lshift32_rnd_64(SRC_SW, scalar));
    for (int i = 0; i < HLX_HALF_ELEMS; i++) {
        expected[i] = exp;
    }
    check_h("LX_shrr_h_w_rnd_sat", buf_out, expected, HLX_HALF_ELEMS);
}

/* ===== Unsigned narrowing shifts with saturation ===== */

static void test_shr_uh_uw_sat(void)
{
    fill_w(pw_src_lo, pw_src_hi, (int32_t)SRC_UW);
    fill_h(buf_shamt, (int16_t)SHAMT_W);

    HLX_VectorPair pa = Q6_LW_combine_LL(Q6_L_tmp_mem_A(pw_src_hi),
                                           Q6_L_tmp_mem_A(pw_src_lo));
    HLX_Vector lshamt = Q6_L_tmp_mem_A(buf_shamt);
    Q6_mem_Ku16_new_AL(buf_out, Q6_Luh_shr_LWuwLh_sat(pa, lshamt));

    uint16_t exp = sat_uh64(ref_lshiftu32_64(SRC_UW, SHAMT_W));
    for (int i = 0; i < HLX_HALF_ELEMS; i++) {
        expected[i] = exp;
    }
    check_h("LX_shr_uh_uw_sat", buf_out, expected, HLX_HALF_ELEMS);
}

static void test_shrr_uh_uw_sat(void)
{
    fill_w(pw_src_lo, pw_src_hi, (int32_t)SRC_UW);
    int16_t scalar = -SHAMT_W;

    HLX_VectorPair pa = Q6_LW_combine_LL(Q6_L_tmp_mem_A(pw_src_hi),
                                           Q6_L_tmp_mem_A(pw_src_lo));
    Q6_mem_Ku16_new_AL(buf_out, Q6_Luh_shr_LWuwRh_sat(pa, scalar));

    uint16_t exp = sat_uh64(ref_lshiftu32_64(SRC_UW, scalar));
    for (int i = 0; i < HLX_HALF_ELEMS; i++) {
        expected[i] = exp;
    }
    check_h("LX_shrr_uh_uw_sat", buf_out, expected, HLX_HALF_ELEMS);
}

static void test_shr_uh_uw_rnd_sat(void)
{
    fill_w(pw_src_lo, pw_src_hi, (int32_t)SRC_UW);
    fill_h(buf_shamt, (int16_t)SHAMT_W);

    HLX_VectorPair pa = Q6_LW_combine_LL(Q6_L_tmp_mem_A(pw_src_hi),
                                           Q6_L_tmp_mem_A(pw_src_lo));
    HLX_Vector lshamt = Q6_L_tmp_mem_A(buf_shamt);
    Q6_mem_Ku16_new_AL(buf_out, Q6_Luh_shr_LWuwLh_rnd_sat(pa, lshamt));

    uint16_t exp = sat_uh64(ref_lshiftu32_rnd_64(SRC_UW, SHAMT_W));
    for (int i = 0; i < HLX_HALF_ELEMS; i++) {
        expected[i] = exp;
    }
    check_h("LX_shr_uh_uw_rnd_sat", buf_out, expected, HLX_HALF_ELEMS);
}

static void test_shrr_uh_uw_rnd_sat(void)
{
    fill_w(pw_src_lo, pw_src_hi, (int32_t)SRC_UW);
    int16_t scalar = -SHAMT_W;

    HLX_VectorPair pa = Q6_LW_combine_LL(Q6_L_tmp_mem_A(pw_src_hi),
                                           Q6_L_tmp_mem_A(pw_src_lo));
    Q6_mem_Ku16_new_AL(buf_out, Q6_Luh_shr_LWuwRh_rnd_sat(pa, scalar));

    uint16_t exp = sat_uh64(ref_lshiftu32_rnd_64(SRC_UW, scalar));
    for (int i = 0; i < HLX_HALF_ELEMS; i++) {
        expected[i] = exp;
    }
    check_h("LX_shrr_uh_uw_rnd_sat", buf_out, expected, HLX_HALF_ELEMS);
}

static void test_shr_uh_w_sat(void)
{
    fill_w(pw_src_lo, pw_src_hi, SRC_SW);
    fill_h(buf_shamt, (int16_t)SHAMT_W);

    HLX_VectorPair pa = Q6_LW_combine_LL(Q6_L_tmp_mem_A(pw_src_hi),
                                           Q6_L_tmp_mem_A(pw_src_lo));
    HLX_Vector lshamt = Q6_L_tmp_mem_A(buf_shamt);
    Q6_mem_Ku16_new_AL(buf_out, Q6_Luh_shr_LWwLh_sat(pa, lshamt));

    uint16_t exp = sat_uh64s(ref_lshift32_64(SRC_SW, SHAMT_W));
    for (int i = 0; i < HLX_HALF_ELEMS; i++) {
        expected[i] = exp;
    }
    check_h("LX_shr_uh_w_sat", buf_out, expected, HLX_HALF_ELEMS);
}

static void test_shrr_uh_w_sat(void)
{
    fill_w(pw_src_lo, pw_src_hi, SRC_SW);
    int16_t scalar = -SHAMT_W;

    HLX_VectorPair pa = Q6_LW_combine_LL(Q6_L_tmp_mem_A(pw_src_hi),
                                           Q6_L_tmp_mem_A(pw_src_lo));
    Q6_mem_Ku16_new_AL(buf_out, Q6_Luh_shr_LWwRh_sat(pa, scalar));

    uint16_t exp = sat_uh64s(ref_lshift32_64(SRC_SW, scalar));
    for (int i = 0; i < HLX_HALF_ELEMS; i++) {
        expected[i] = exp;
    }
    check_h("LX_shrr_uh_w_sat", buf_out, expected, HLX_HALF_ELEMS);
}

static void test_shr_uh_w_rnd_sat(void)
{
    fill_w(pw_src_lo, pw_src_hi, SRC_SW);
    fill_h(buf_shamt, (int16_t)SHAMT_W);

    HLX_VectorPair pa = Q6_LW_combine_LL(Q6_L_tmp_mem_A(pw_src_hi),
                                           Q6_L_tmp_mem_A(pw_src_lo));
    HLX_Vector lshamt = Q6_L_tmp_mem_A(buf_shamt);
    Q6_mem_Ku16_new_AL(buf_out, Q6_Luh_shr_LWwLh_rnd_sat(pa, lshamt));

    uint16_t exp = sat_uh64s(ref_lshift32_rnd_64(SRC_SW, SHAMT_W));
    for (int i = 0; i < HLX_HALF_ELEMS; i++) {
        expected[i] = exp;
    }
    check_h("LX_shr_uh_w_rnd_sat", buf_out, expected, HLX_HALF_ELEMS);
}

static void test_shrr_uh_w_rnd_sat(void)
{
    fill_w(pw_src_lo, pw_src_hi, SRC_SW);
    int16_t scalar = -SHAMT_W;

    HLX_VectorPair pa = Q6_LW_combine_LL(Q6_L_tmp_mem_A(pw_src_hi),
                                           Q6_L_tmp_mem_A(pw_src_lo));
    Q6_mem_Ku16_new_AL(buf_out, Q6_Luh_shr_LWwRh_rnd_sat(pa, scalar));

    uint16_t exp = sat_uh64s(ref_lshift32_rnd_64(SRC_SW, scalar));
    for (int i = 0; i < HLX_HALF_ELEMS; i++) {
        expected[i] = exp;
    }
    check_h("LX_shrr_uh_w_rnd_sat", buf_out, expected, HLX_HALF_ELEMS);
}

int main(void)
{
    init_vtcm();
    enable_hlx();

    buf_src = vtcm_alloc(HLX_HALF_ELEMS * sizeof(uint16_t));
    buf_shamt = vtcm_alloc(HLX_HALF_ELEMS * sizeof(uint16_t));
    buf_out = vtcm_alloc(HLX_HALF_ELEMS * sizeof(uint16_t));
    pw_src_lo = vtcm_alloc(HLX_WORD_ELEMS * sizeof(uint32_t));
    pw_src_hi = vtcm_alloc(HLX_WORD_ELEMS * sizeof(uint32_t));
    pw_out_lo = vtcm_alloc(HLX_WORD_ELEMS * sizeof(uint32_t));
    pw_out_hi = vtcm_alloc(HLX_WORD_ELEMS * sizeof(uint32_t));
    init_hlx_vsp();

    /* Basic halfword shifts */
    test_shr_uh_uh();
    test_shrr_uh_uh();
    test_shr_h();
    test_shrr_h();

    /* Word pair shifts */
    test_shr_uw_uw();
    test_shrr_uw_uw();
    test_shr_w_w();
    test_shrr_w_w();

    /* Narrowing shifts */
    test_shr_h_w_uh();
    test_shrr_h_w_uh();

    /* Narrowing shifts with saturation */
    test_shr_h_w_sat();
    test_shrr_h_w_sat();
    test_shr_h_w_rnd_sat();
    test_shrr_h_w_rnd_sat();

    /* Unsigned narrowing with saturation */
    test_shr_uh_uw_sat();
    test_shrr_uh_uw_sat();
    test_shr_uh_uw_rnd_sat();
    test_shrr_uh_uw_rnd_sat();
    test_shr_uh_w_sat();
    test_shrr_uh_w_sat();
    test_shr_uh_w_rnd_sat();
    test_shrr_uh_w_rnd_sat();

    puts(err ? "FAIL" : "PASS");
    return err;
}
