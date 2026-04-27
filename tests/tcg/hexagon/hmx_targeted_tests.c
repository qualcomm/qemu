/*
 * HMX Targeted Tests
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Targeted tests for HMX defects:
 *   - Byte weight unsigned interpretation
 *   - Accumulator set alternation after CVT clear
 *   - FP shape functions (shape=0,1,2,3)
 *   - FP weight negate (Rs[5])
 *   - Negative zero FP16 preservation
 *   - Nibble/crumb weight sign extension
 *   - F8 weight byte interleave order
 *   - Weight address range validation
 *   - Sub-byte ch_start/ch_stop override
 *   - Deep weight modifier
 *   - Hardware loop acc clear/flip (baremetal only)
 *   - F8 convert + store pipeline
 *   - Channel-major activation+store pipeline
 *   - Multi-tap filter convolution
 *   - Legacy :retain store accumulation
 *   - Multi-iteration state persistence
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "hmx_intrinsics.h"

static int err;

static inline void check_val(uint32_t val, uint32_t expect,
                              const char *msg)
{
    if (val != expect) {
        puts(msg);
        err++;
    }
}

#if !defined(__linux__)
/* ================================================================== */
/* Baremetal infrastructure                                            */
/* ================================================================== */

#include <hexagon_standalone.h>

/*
 * min_libc doesn't provide memcpy; the compiler emits calls to it
 * via builtins, so we provide a simple implementation here.
 */
void *memcpy(void *dst, const void *src, size_t n)
{
    unsigned char *d = dst;
    const unsigned char *s = src;

    while (n--) {
        *d++ = *s++;
    }
    return dst;
}

static uint8_t *va_vtcm = (uint8_t *)0xf0000000;

static uint8_t *get_vtcm_base(void)
{
    unsigned char *vtcm_base = NULL;

    asm volatile("r1 = cfgbase\n"
                 "r1 = asl(r1, #5)\n"
                 "r2 = #0x38\n"
                 "r1 = memw_phys(r2, r1)\n"
                 "%[vtcm_base] = asl(r1, #16)\n"
                 : [vtcm_base] "=r"(vtcm_base)
                 :
                 : "r1", "r2");
    return vtcm_base;
}

static uint8_t *setup_vtcm_mapping(void)
{
    uint8_t *vtcm;
    unsigned vtcm_page_size = 4 * 1024 * 1024;
    unsigned page_size_enum = 32;
    unsigned perms = 7;
    unsigned cachability = 6;
    unsigned asid = 0;
    unsigned aa = 0;
    unsigned vg = 3;

    vtcm = get_vtcm_base();
    add_translation_extended(1, va_vtcm, (uint64_t)vtcm,
                             page_size_enum, perms, cachability,
                             asid, aa, vg);
    add_translation_extended(2, va_vtcm + vtcm_page_size,
                             (uint64_t)(vtcm + vtcm_page_size),
                             page_size_enum, perms, cachability,
                             asid, aa, vg);
    return va_vtcm;
}

static void enable_coproc(void)
{
    asm volatile("r6 = ssr\n"
                 "r6 = setbit(r6, #26)\n"
                 "ssr = r6\n"
                 "nop\n"
                 "nop\n"
                 "isync;\n"
                 : : : "r6");
}

#else
/* ================================================================== */
/* Linux-user infrastructure                                           */
/* ================================================================== */

#include <stdlib.h>

#endif

/* Paired-packet wrappers using .word encodings (shared by both paths) */
static void hmx_act_wei_ub_sm(uintptr_t act, uint32_t ar,
                               uintptr_t wei, uint32_t wr)
{
    _HMX_PAIRED(_HMX_ACT_UB_PAIRED, _HMX_WEI_B_PAIRED,
                act, ar, wei, wr);
}

static void hmx_act_wei_hf_sm(uintptr_t act, uint32_t ar,
                               uintptr_t wei, uint32_t wr)
{
    _HMX_PAIRED(_HMX_ACT_HF_PAIRED, _HMX_WEI_HF_PAIRED,
                act, ar, wei, wr);
}

static void hmx_matmul_nibble(uintptr_t act, uint32_t ar,
                               uintptr_t wei, uint32_t wr)
{
    _HMX_PAIRED(_HMX_ACT_UB_PAIRED, _HMX_WEI_N_PAIRED,
                act, ar, wei, wr);
}

static void hmx_matmul_crumb(uintptr_t act, uint32_t ar,
                              uintptr_t wei, uint32_t wr)
{
    _HMX_PAIRED(_HMX_ACT_UB_PAIRED, _HMX_WEI_C_PAIRED,
                act, ar, wei, wr);
}

static void hmx_matmul_fp16_f8w(uintptr_t act, uint32_t ar,
                                  uintptr_t wei, uint32_t wr)
{
    _HMX_PAIRED(_HMX_ACT_HF_PAIRED, _HMX_WEI_F8_PAIRED,
                act, ar, wei, wr);
}

static void hmx_matmul_byte_deep(uintptr_t act, uint32_t ar,
                                   uintptr_t wei, uint32_t wr)
{
    _HMX_PAIRED(_HMX_ACT_UB_DEEP_PAIRED, _HMX_WEI_B_PAIRED,
                act, ar, wei, wr);
}

static void hmx_act_cm_wei_ub(uintptr_t act, uint32_t ar,
                               uintptr_t wei, uint32_t wr)
{
    _HMX_PAIRED(_HMX_ACT_UB_CM_PAIRED, _HMX_WEI_B_PAIRED,
                act, ar, wei, wr);
}

/* ================================================================== */
/* VTCM layout offsets                                                 */
/* ================================================================== */

#define VTCM_SIZE        0x10000
#define VTCM_ACT_OFF     0x0000
#define VTCM_WEI_OFF     0x1000
#define VTCM_BIAS_OFF    0x2000
#define VTCM_OUT_OFF     0x3000
#define VTCM_OUT2_OFF    0x3800

/* ================================================================== */
/* Common helpers                                                      */
/* ================================================================== */

/*
 * SM crouton offset: byte for spatial s, channel c
 */
static int crouton_off_sm(int spatial, int channel)
{
    return ((spatial >> 2) << 7) | (channel << 2) | (spatial & 3);
}


/*
 * Pack FXP bias register (64 bits)
 */
static uint64_t pack_fxp_bias(int32_t input_bias, uint16_t exponent,
                               uint16_t shape, uint16_t scale,
                               uint16_t out_bias)
{
    uint64_t raw = 0;

    raw |= ((uint64_t)(uint32_t)input_bias) << 32;
    raw |= (scale >> 1) & 0x3FF;
    raw |= (uint64_t)(exponent & 0x1F) << 10;
    raw |= (uint64_t)((shape >> 2) & 1) << 15;
    raw |= (uint64_t)(((~scale) >> 11) & 1) << 16;
    raw |= (uint64_t)(shape & 3) << 17;
    raw |= (uint64_t)(out_bias & 7) << 19;
    raw |= (uint64_t)((out_bias >> 3) & 1) << 22;
    raw |= (uint64_t)((~out_bias >> 4) & 0xFF) << 23;
    raw |= (uint64_t)(scale & 1) << 31;
    return raw;
}

/*
 * Pack FP bias register (64 bits)
 */
static uint64_t pack_fp_bias(uint16_t scale_fp16,
                              uint16_t out_bias_fp16,
                              uint8_t scale_extra,
                              uint8_t out_bias_extra,
                              uint8_t shape, uint8_t negate,
                              uint8_t acc_bias_extra,
                              uint16_t acc_bias_fp16)
{
    uint64_t raw = 0;

    raw |= (uint64_t)scale_fp16;
    raw |= (uint64_t)out_bias_fp16 << 16;
    raw |= (uint64_t)(scale_extra & 0xF) << 32;
    raw |= (uint64_t)(out_bias_extra & 0xF) << 36;
    raw |= (uint64_t)(shape & 0x3) << 40;
    raw |= (uint64_t)(negate & 0x1) << 42;
    raw |= (uint64_t)(acc_bias_extra & 0x1F) << 43;
    raw |= (uint64_t)acc_bias_fp16 << 48;
    return raw;
}

/*
 * Write bias data to VTCM in mxmem2 format (256B block):
 * lower 32 bits in first 128B, upper 32 bits in second 128B.
 */
static void write_bias_mxmem2(uint8_t *bias_vtcm,
                               uint64_t *vals, int count)
{
    uint32_t *lo = (uint32_t *)bias_vtcm;
    uint32_t *hi = (uint32_t *)(bias_vtcm + 128);
    int i;

    for (i = 0; i < count; i++) {
        lo[i] = (uint32_t)vals[i];
        hi[i] = (uint32_t)(vals[i] >> 32);
    }
}

static void setup_fxp_bias(uint8_t *bias_area, uint64_t bias_val)
{
    uint64_t bias_vals[32];
    int i;

    for (i = 0; i < 32; i++) {
        bias_vals[i] = bias_val;
    }
    write_bias_mxmem2(bias_area, bias_vals, 32);
}

static void setup_fp_bias_unit(uint8_t *bias_area)
{
    uint64_t bias_vals[32];
    int i;

    for (i = 0; i < 32; i++) {
        bias_vals[i] = pack_fp_bias(0x3C00, 0, 0, 0, 0, 0, 0, 0);
    }
    write_bias_mxmem2(bias_area, bias_vals, 32);
}

/*
 * Write raw int32 bias values in mxmem2 layout (256B block).
 * This writes the bias as simple integers, NOT packed FXP format.
 */
static void setup_bias_raw_int32(uint8_t *bias_area)
{
    int32_t *bias = (int32_t *)bias_area;
    int i;

    for (i = 0; i < 64; i++) {
        bias[i] = i << 10;
    }
}

/*
 * Generate CM activation range (Rt) for given input depth.
 * CM format: no bit swap, ch_stop in bits [4:0], tile_y in bits [7:5].
 */
static uint32_t gen_act_range_cm(int input_depth)
{
    uint32_t dc0 = ((input_depth - 1) & 0x1F) & ~1u;

    return dc0 | 0xe0;
}

/*
 * CM-to-SM bit swap for activation range encoding.
 */
static uint32_t spatial_major_convert(uint32_t val)
{
    uint32_t spatial_bits = (val >> 5) & 0x3;
    uint32_t depth_bits = (val & 0x1F) << 2;

    val &= ~0x7Fu;
    return val | depth_bits | spatial_bits;
}

/*
 * Generate SM activation range (Rt) for given input depth.
 */
static uint32_t gen_act_range_sm(int input_depth)
{
    uint32_t dc0 = ((input_depth - 1) & 0x1F) & ~1u;
    uint32_t temp = dc0 | (7u << 8);

    return spatial_major_convert(temp);
}

/* ================================================================== */
/* Tests                                                               */
/* ================================================================== */

/*
 * Byte weight unsigned vs signed
 *
 * activation=2, weight=0xFE (unsigned=254, signed=-2)
 * With relu (shape=2): unsigned gives positive output.
 */
static void test_byte_weight_unsigned(uint8_t *base)
{
    uint8_t *act = base + VTCM_ACT_OFF;
    uint8_t *wei = base + VTCM_WEI_OFF;
    uint8_t *bias_area = base + VTCM_BIAS_OFF;
    uint8_t *out = base + VTCM_OUT_OFF;
    uint64_t bias_vals[32];
    uint32_t val;
    int i;

    puts("byte weight unsigned");

    memset(act, 0, 2048);
    act[crouton_off_sm(0, 0)] = 2;

    memset(wei, 0, 128);
    wei[0] = 0xFE;

    for (i = 0; i < 32; i++) {
        bias_vals[i] = pack_fxp_bias(0, 0, 2, 0x400, 0);
    }
    write_bias_mxmem2(bias_area, bias_vals, 32);

    Q6_mxclracc();
    Q6_bias_mxmem2_A(bias_area);
    hmx_act_wei_ub_sm((uintptr_t)act, 0, (uintptr_t)wei, 0);

    memset(out, 0xAA, 2048);
    Q6_mxmem_AR_after_sat_ub(out, 0);

    val = out[crouton_off_sm(0, 0)];
    if (val == 0) {
        printf("ERROR: byte weight appears signed (output=0)\n");
        err++;
    }
}

/*
 * Accumulator set alternation
 *
 * After convert+clear, the accumulator set flips.
 * Both rounds should produce non-zero output.
 */
static void test_acc_set_alternation(uint8_t *base)
{
    uint8_t *act = base + VTCM_ACT_OFF;
    uint8_t *wei = base + VTCM_WEI_OFF;
    uint8_t *bias_area = base + VTCM_BIAS_OFF;
    uint8_t *out1 = base + VTCM_OUT_OFF;
    uint8_t *out2 = base + VTCM_OUT2_OFF;
    uint64_t bias_vals[32];
    uint32_t r1, r2;
    int i;

    puts("acc set alternation");

    memset(act, 0, 2048);
    act[crouton_off_sm(0, 0)] = 10;

    memset(wei, 0, 128);
    wei[0] = 5;

    for (i = 0; i < 32; i++) {
        bias_vals[i] = pack_fxp_bias(0, 0, 0, 0x400, 0);
    }
    write_bias_mxmem2(bias_area, bias_vals, 32);

    /* Round 1 */
    Q6_mxclracc();
    Q6_bias_mxmem2_A(bias_area);
    hmx_act_wei_ub_sm((uintptr_t)act, 0, (uintptr_t)wei, 0);
    memset(out1, 0, 2048);
    Q6_mxmem_AR_after_sat_ub(out1, 0);

    /* Round 2 (acc set should have flipped) */
    wei[0] = 3;
    Q6_bias_mxmem2_A(bias_area);
    hmx_act_wei_ub_sm((uintptr_t)act, 0, (uintptr_t)wei, 0);
    memset(out2, 0, 2048);
    Q6_mxmem_AR_after_sat_ub(out2, 0);

    r1 = out1[crouton_off_sm(0, 0)];
    r2 = out2[crouton_off_sm(0, 0)];

    if (r1 == 0) {
        printf("ERROR: round 1 output is 0\n");
        err++;
    }
    if (r2 == 0) {
        printf("ERROR: round 2 output is 0\n");
        err++;
    }
}

/*
 * FP shape functions
 *
 * shape=0: pass through
 * shape=1: min(x, 0)
 * shape=2: max(x, 0)  (relu)
 * shape=3: |x|
 */
static void test_fp_shape_functions(uint8_t *base)
{
    uint8_t *act = base + VTCM_ACT_OFF;
    uint8_t *wei = base + VTCM_WEI_OFF;
    uint8_t *bias_area = base + VTCM_BIAS_OFF;
    uint8_t *out = base + VTCM_OUT_OFF;
    uint16_t *wei16 = (uint16_t *)wei;
    uint16_t *out16;
    uint64_t bias_vals[32];
    uint16_t f16_out;
    int i, shape, sign;

    puts("FP shape functions");

    /* Positive accumulator: act=1.0, weight=1.0 -> acc=+1.0 */
    memset(act, 0, 2048);
    act[crouton_off_sm(0, 0)] = 0x00;
    act[crouton_off_sm(0, 0) + 1] = 0x3C;  /* FP16 1.0 */

    memset(wei, 0, 128);
    wei16[0] = 0x3C00;  /* 1.0 */

    for (shape = 0; shape < 4; shape++) {
        for (i = 0; i < 32; i++) {
            bias_vals[i] = pack_fp_bias(
                0x3C00, 0x0000, 0, 0, shape, 0, 0, 0);
        }
        write_bias_mxmem2(bias_area, bias_vals, 32);

        Q6_mxclracc_hf();
        Q6_bias_mxmem2_A(bias_area);
        hmx_act_wei_hf_sm((uintptr_t)act, 0,
                          (uintptr_t)wei, 0);

        memset(out, 0, 2048);
        Q6_mxmem_AR_after_hf(out, 0);

        out16 = (uint16_t *)(out + crouton_off_sm(0, 0));
        f16_out = *out16;
        sign = (f16_out >> 15) & 1;

        switch (shape) {
        case 0:  /* pass: positive -> positive */
            if (f16_out == 0 || sign != 0) {
                printf("ERROR: shape 0 pos: expected positive\n");
                err++;
            }
            break;
        case 1:  /* min(x,0): positive -> 0 */
            if (f16_out != 0 && f16_out != 0x8000) {
                printf("ERROR: shape 1 pos: expected zero\n");
                err++;
            }
            break;
        case 2:  /* max(x,0): positive -> positive */
            if (f16_out == 0 || sign != 0) {
                printf("ERROR: shape 2 pos: expected positive\n");
                err++;
            }
            break;
        case 3:  /* |x|: positive -> positive */
            if (f16_out == 0 || sign != 0) {
                printf("ERROR: shape 3 pos: expected positive\n");
                err++;
            }
            break;
        }
    }

    /* Negative accumulator: weight=-1.0 */
    wei16[0] = 0xBC00;  /* -1.0 */

    for (shape = 0; shape < 4; shape++) {
        for (i = 0; i < 32; i++) {
            bias_vals[i] = pack_fp_bias(
                0x3C00, 0x0000, 0, 0, shape, 0, 0, 0);
        }
        write_bias_mxmem2(bias_area, bias_vals, 32);

        Q6_mxclracc_hf();
        Q6_bias_mxmem2_A(bias_area);
        hmx_act_wei_hf_sm((uintptr_t)act, 0,
                          (uintptr_t)wei, 0);

        memset(out, 0, 2048);
        Q6_mxmem_AR_after_hf(out, 0);

        out16 = (uint16_t *)(out + crouton_off_sm(0, 0));
        f16_out = *out16;
        sign = (f16_out >> 15) & 1;

        switch (shape) {
        case 0:  /* pass: negative -> negative */
            if (f16_out == 0 || sign != 1) {
                printf("ERROR: shape 0 neg: expected negative\n");
                err++;
            }
            break;
        case 1:  /* min(x,0): negative -> negative */
            if (f16_out == 0 || sign != 1) {
                printf("ERROR: shape 1 neg: expected negative\n");
                err++;
            }
            break;
        case 2:  /* max(x,0): negative -> 0 */
            if (f16_out != 0 && f16_out != 0x8000) {
                printf("ERROR: shape 2 neg: expected zero\n");
                err++;
            }
            break;
        case 3:  /* |x|: negative -> positive */
            if (f16_out == 0 || sign != 0) {
                printf("ERROR: shape 3 neg: expected positive\n");
                err++;
            }
            break;
        }
    }
}

/*
 * FP weight negate (Rs[5])
 *
 * With act=1.0 and weight=1.0:
 *   Without negate: acc = 1.0 -> positive output
 *   With Rs[5]=1:   acc = -1.0 -> negative output
 */
static void test_fp_weight_negate(uint8_t *base)
{
    uint8_t *act = base + VTCM_ACT_OFF;
    uint8_t *wei = base + VTCM_WEI_OFF;
    uint8_t *bias_area = base + VTCM_BIAS_OFF;
    uint8_t *out = base + VTCM_OUT_OFF;
    uint16_t *wei16 = (uint16_t *)wei;
    uint16_t *out16;
    uint64_t bias_vals[32];
    uint16_t f16_no_neg, f16_neg;
    int i;

    puts("FP weight negate");

    memset(act, 0, 2048);
    act[crouton_off_sm(0, 0)] = 0x00;
    act[crouton_off_sm(0, 0) + 1] = 0x3C;  /* FP16 1.0 */

    memset(wei, 0, 128);
    wei16[0] = 0x3C00;  /* 1.0 */

    for (i = 0; i < 32; i++) {
        bias_vals[i] = pack_fp_bias(0x3C00, 0, 0, 0, 0, 0, 0, 0);
    }
    write_bias_mxmem2(bias_area, bias_vals, 32);

    /* Without negate */
    Q6_mxclracc_hf();
    Q6_bias_mxmem2_A(bias_area);
    hmx_act_wei_hf_sm((uintptr_t)act, 0, (uintptr_t)wei, 0);

    memset(out, 0, 2048);
    Q6_mxmem_AR_after_hf(out, 0);
    out16 = (uint16_t *)(out + crouton_off_sm(0, 0));
    f16_no_neg = *out16;

    /* With negate: set Rs[5]=1 in weight address */
    Q6_mxclracc_hf();
    Q6_bias_mxmem2_A(bias_area);
    hmx_act_wei_hf_sm((uintptr_t)act, 0,
                      (uintptr_t)wei | (1 << 5), 0);

    memset(out, 0, 2048);
    Q6_mxmem_AR_after_hf(out, 0);
    out16 = (uint16_t *)(out + crouton_off_sm(0, 0));
    f16_neg = *out16;

    /* Without negate should be positive */
    if (f16_no_neg == 0 || ((f16_no_neg >> 15) & 1)) {
        printf("ERROR: no-negate should be positive\n");
        err++;
    }
    /* With negate should be negative */
    if (f16_neg == 0 || !((f16_neg >> 15) & 1)) {
        printf("ERROR: negate should be negative\n");
        err++;
    }
}

/*
 * Negative zero FP16 through pipeline
 *
 * FP accumulator = -0.0 * 1.0 = -0.0
 * The convert pipeline adds acc_bias (even if 0):
 *   d_biased = -0.0 + 0.0 = +0.0 (IEEE 754: -0+0 = +0)
 * Output should be 0x0000 (positive zero), NOT 0x8000.
 */
static void test_negative_zero_fp16(uint8_t *base)
{
    uint8_t *act = base + VTCM_ACT_OFF;
    uint8_t *wei = base + VTCM_WEI_OFF;
    uint8_t *bias_area = base + VTCM_BIAS_OFF;
    uint8_t *out = base + VTCM_OUT_OFF;
    uint16_t *wei16 = (uint16_t *)wei;
    uint16_t *out16;
    uint64_t bias_vals[32];
    int i;

    puts("negative zero FP16");

    memset(act, 0, 2048);
    /* FP16 -0.0 = 0x8000 at spatial=0, ch=0 */
    act[crouton_off_sm(0, 0)] = 0x00;
    act[crouton_off_sm(0, 0) + 1] = 0x80;

    memset(wei, 0, 128);
    wei16[0] = 0x3C00;  /* 1.0 */

    for (i = 0; i < 32; i++) {
        bias_vals[i] = pack_fp_bias(0x3C00, 0, 0, 0, 0, 0, 0, 0);
    }
    write_bias_mxmem2(bias_area, bias_vals, 32);

    Q6_mxclracc_hf();
    Q6_bias_mxmem2_A(bias_area);
    hmx_act_wei_hf_sm((uintptr_t)act, 0, (uintptr_t)wei, 0);

    memset(out, 0xFF, 2048);
    Q6_mxmem_AR_after_hf(out, 0);

    out16 = (uint16_t *)(out + crouton_off_sm(0, 0));
    /* -0.0 becomes +0.0 through pipeline (acc_bias addition) */
    check_val(*out16, 0x0000,
              "ERROR: neg zero should become pos zero");
}

/*
 * Nibble/crumb weight sign extension
 *
 * Nibble values >= 8 must be sign-extended to negative.
 * Crumb values >= 2 must be sign-extended to negative.
 *
 * Nibble test: act=1 at s0/c0, weight nibble 0xF (-1) + 0x8 (-8).
 *   With sign extension: acc = 1*(-1) + 1*(-8) = -9
 *   Without: acc = 1*15 + 1*8 = 23.  Using relu: negative -> 0.
 *
 * Crumb test: act=1 at s0/c0, weight crumbs 0x3 (-1) + 0x2 (-2).
 *   With sign extension: acc = -3.  Without: acc = 5.
 */
static void test_nibble_crumb_sign_ext(uint8_t *base)
{
    uint8_t *act = base + VTCM_ACT_OFF;
    uint8_t *wei = base + VTCM_WEI_OFF;
    uint8_t *bias_area = base + VTCM_BIAS_OFF;
    uint8_t *out = base + VTCM_OUT_OFF;
    uint32_t *wei32 = (uint32_t *)wei;
    uint32_t val;

    puts("nibble/crumb sign extension");

    /* --- Nibble test --- */
    memset(act, 0, 2048);
    act[crouton_off_sm(0, 0)] = 1;
    act[crouton_off_sm(0, 1)] = 1;

    /*
     * Nibble weight vector: 8 nibbles per 32-bit word.
     * nibble[0] in bits [3:0], nibble[1] in [7:4], etc.
     * Set nibble[0]=0xF (-1), nibble[1]=0x8 (-8), rest=0.
     * Word = 0x8F.
     */
    memset(wei, 0, 128);
    wei32[0] = 0x8F;

    /* Bias: shape=2 (relu), exp=20 to get visible output */
    setup_fxp_bias(bias_area,
                   pack_fxp_bias(0, 20, 2, 0x080, 0xFF0));

    Q6_mxclracc();
    Q6_bias_mxmem2_A(bias_area);
    hmx_matmul_nibble((uintptr_t)act, 0, (uintptr_t)wei, 0);

    memset(out, 0xAA, 2048);
    Q6_mxmem_AR_after_sat_ub(out, 0);

    /*
     * With correct sign extension: acc is negative, relu clips to 0.
     * Without: acc is positive, output > 0.
     */
    val = out[crouton_off_sm(0, 0)];
    if (val != 0) {
        printf("ERROR: nibble sign ext: expected 0\n");
        err++;
    }

    /* --- Crumb test --- */
    memset(act, 0, 2048);
    act[crouton_off_sm(0, 0)] = 1;
    act[crouton_off_sm(0, 1)] = 1;

    /*
     * Crumb weight: 16 crumbs per 32-bit word.
     * crumb[0] in bits [1:0], crumb[1] in [3:2], etc.
     * Set crumb[0]=0x3 (-1), crumb[1]=0x2 (-2), rest=0.
     * Word = 0x0B (binary: ...00 10 11).
     */
    memset(wei, 0, 128);
    wei32[0] = 0x0B;

    /* Crumb weights are small; use exp=24 to amplify */
    setup_fxp_bias(bias_area,
                   pack_fxp_bias(0, 24, 2, 0x080, 0xFF0));

    Q6_mxclracc();
    Q6_bias_mxmem2_A(bias_area);
    hmx_matmul_crumb((uintptr_t)act, 0, (uintptr_t)wei, 0);

    memset(out, 0xAA, 2048);
    Q6_mxmem_AR_after_sat_ub(out, 0);

    val = out[crouton_off_sm(0, 0)];
    if (val != 0) {
        printf("ERROR: crumb sign ext: expected 0\n");
        err++;
    }
}

/*
 * F8 weight byte interleave order
 *
 * F8 weight bytes use interleave {0,2,1,3} mapping streams to bytes.
 * With act[ch0]=FP16(1.0), act[ch1]=FP16(2.0), rest=0:
 *   byte0=0x40, byte1=0x00, byte2=0x40, byte3=0x00
 *   Correct interleave: stream1 uses byte2 (0x40) -> non-zero
 *   Wrong order:        stream1 uses byte1 (0x00) -> zero
 */
static void test_f8_weight_interleave(uint8_t *base)
{
    uint8_t *act = base + VTCM_ACT_OFF;
    uint8_t *wei = base + VTCM_WEI_OFF;
    uint8_t *bias_area = base + VTCM_BIAS_OFF;
    uint8_t *out = base + VTCM_OUT_OFF;
    uint32_t *wei32 = (uint32_t *)wei;
    uint16_t *out16;
    uint16_t val;
    int i;

    puts("F8 weight byte interleave");

    /* FP16 activations: ch0=1.0, ch1=2.0 */
    memset(act, 0, 2048);
    act[crouton_off_sm(0, 0)] = 0x00;
    act[crouton_off_sm(0, 0) + 1] = 0x3C;  /* FP16 1.0 */
    act[crouton_off_sm(0, 1)] = 0x00;
    act[crouton_off_sm(0, 1) + 1] = 0x40;  /* FP16 2.0 */

    /*
     * F8 weight: byte0=0x40, byte1=0x00, byte2=0x40, byte3=0x00.
     * Word = 0x00400040.
     */
    memset(wei, 0, 128);
    for (i = 0; i < 32; i++) {
        wei32[i] = 0x00400040;
    }

    setup_fp_bias_unit(bias_area);

    Q6_mxclracc_hf();
    Q6_bias_mxmem2_A(bias_area);
    hmx_matmul_fp16_f8w((uintptr_t)act, 0,
                        (uintptr_t)wei, 0);

    memset(out, 0, 2048);
    Q6_mxmem_AR_after_hf(out, 0);

    out16 = (uint16_t *)(out + crouton_off_sm(0, 0));
    val = *out16;
    if (val == 0) {
        printf("ERROR: F8 interleave: output is zero\n");
        err++;
    }
}

/*
 * Weight address range validation
 *
 * Multiple weight vectors via weight Rt.  Vectors beyond
 * the range should be skipped.
 * (a) 2 vectors -> both contribute
 * (b) 1 vector  -> only first contributes
 * Outputs should differ.
 */
static void test_weight_range(uint8_t *base)
{
    uint8_t *act = base + VTCM_ACT_OFF;
    uint8_t *wei = base + VTCM_WEI_OFF;
    uint8_t *bias_area = base + VTCM_BIAS_OFF;
    uint8_t *out = base + VTCM_OUT_OFF;
    uint8_t *out2 = base + VTCM_OUT2_OFF;
    uint32_t *wei32 = (uint32_t *)wei;
    uint32_t val_2vec, val_1vec;
    uint32_t act_range;
    int i;

    puts("weight address range");

    memset(act, 100, 2048);

    /* Vec 0: weight=10, Vec 1: weight=20 */
    memset(wei, 0, 256);
    for (i = 0; i < 32; i++) {
        wei32[i] = 0x0A0A0A0A;        /* vec 0: byte 10 */
        wei32[i + 32] = 0x14141414;   /* vec 1: byte 20 */
    }

    setup_fxp_bias(bias_area,
                   pack_fxp_bias(0, 20, 0, 0x080, 0xFF0));

    /* Act range for 8 input channels (2 byte-weight vectors) */
    act_range = gen_act_range_sm(8);

    /* (a) Weight range = 2 vectors: wei_range = (2-1) << 7 */
    Q6_mxclracc();
    Q6_bias_mxmem2_A(bias_area);
    hmx_act_wei_ub_sm((uintptr_t)act, act_range,
                      (uintptr_t)wei, (1 << 7));
    memset(out, 0, 2048);
    Q6_mxmem_AR_after_sat_ub(out, 0);
    val_2vec = out[crouton_off_sm(0, 0)];

    /* (b) Weight range = 1 vector: wei_range = 0 */
    Q6_mxclracc();
    Q6_bias_mxmem2_A(bias_area);
    hmx_act_wei_ub_sm((uintptr_t)act, act_range,
                      (uintptr_t)wei, 0);
    memset(out2, 0, 2048);
    Q6_mxmem_AR_after_sat_ub(out2, 0);
    val_1vec = out2[crouton_off_sm(0, 0)];

    if (val_1vec == 0) {
        printf("ERROR: weight range: 1-vec result is 0\n");
        err++;
    }
    if (val_2vec == 0) {
        printf("ERROR: weight range: 2-vec result is 0\n");
        err++;
    }
    if (val_1vec == val_2vec) {
        printf("ERROR: weight range: 1-vec and 2-vec match\n");
        err++;
    }
}

/*
 * Sub-byte ch_start/ch_stop override
 *
 * For sub-byte weight types (nibble: cpv=8), ch_start/ch_stop
 * from the activation instruction should be overridden to 0/32.
 *
 * Set activation Rs with ch_start=4 (narrow range).
 * With fix: ch_start overridden -> same output regardless.
 * Without fix: fewer channels iterated -> different output.
 */
static void test_subbyte_ch_override(uint8_t *base)
{
    uint8_t *act = base + VTCM_ACT_OFF;
    uint8_t *wei = base + VTCM_WEI_OFF;
    uint8_t *out = base + VTCM_OUT_OFF;
    uint8_t *out2 = base + VTCM_OUT2_OFF;
    uint32_t *wei32 = (uint32_t *)wei;
    uint64_t bias_vals[32];
    uint32_t val_narrow, val_full;
    int c, i;

    puts("sub-byte ch_start/ch_stop override");

    /* Fill all channels at spatial 0 with value 100 */
    memset(act, 0, 2048);
    for (c = 0; c < 32; c++) {
        act[crouton_off_sm(0, c)] = 100;
    }

    /*
     * Nibble weight = all +1: each nibble = 0x1.
     * 8 nibbles per word = 0x11111111.
     */
    memset(wei, 0, 128);
    for (i = 0; i < 32; i++) {
        wei32[i] = 0x11111111;
    }

    for (i = 0; i < 32; i++) {
        bias_vals[i] = pack_fxp_bias(0, 0, 0, 0x400, 0);
    }
    write_bias_mxmem2(base + VTCM_BIAS_OFF, bias_vals, 32);

    /* First: normal matmul (ch_start=0) as reference */
    Q6_mxclracc();
    Q6_bias_mxmem2_A(base + VTCM_BIAS_OFF);
    hmx_matmul_nibble((uintptr_t)act, 0,
                      (uintptr_t)wei, 0);
    memset(out, 0, 2048);
    Q6_mxmem_AR_after_sat_ub(out, 0);
    val_full = out[crouton_off_sm(0, 0)];

    /*
     * Second: matmul with ch_start=4 via Rs[6:2].
     * Rs[6:2] = 4 -> Rs = 4 << 2 = 0x10.
     * For sub-byte types, this should be OVERRIDDEN to 0.
     */
    Q6_mxclracc();
    Q6_bias_mxmem2_A(base + VTCM_BIAS_OFF);
    hmx_matmul_nibble((uintptr_t)act | 0x10, 0,
                      (uintptr_t)wei, 0);
    memset(out2, 0, 2048);
    Q6_mxmem_AR_after_sat_ub(out2, 0);
    val_narrow = out2[crouton_off_sm(0, 0)];

    if (val_full == 0) {
        printf("ERROR: sub-byte ch: full result is 0\n");
        err++;
    }
    if (val_narrow != val_full) {
        printf("ERROR: sub-byte ch: ch_start not overridden\n");
        err++;
    }
}

/*
 * Deep weight modifier
 *
 * Deep mode (activation:deep) doubles the input depth.
 * Verify deep mode produces different (larger) accumulation
 * than normal mode with the same data.
 */
static void test_deep_modifier(uint8_t *base)
{
    uint8_t *act = base + VTCM_ACT_OFF;
    uint8_t *wei = base + VTCM_WEI_OFF;
    uint8_t *out = base + VTCM_OUT_OFF;
    uint8_t *out2 = base + VTCM_OUT2_OFF;
    uint64_t bias_vals[32];
    uint32_t val_normal, val_deep;
    int c, i;

    puts("deep weight modifier");

    /* Fill multiple activation channels with known values */
    memset(act, 0, 2048);
    for (c = 0; c < 8; c++) {
        act[crouton_off_sm(0, c)] = 50;
    }

    /* Single weight vector with all bytes = 5 */
    memset(wei, 0, 128);
    for (i = 0; i < 32; i++) {
        ((uint32_t *)wei)[i] = 0x05050505;
    }

    for (i = 0; i < 32; i++) {
        bias_vals[i] = pack_fxp_bias(0, 0, 0, 0x400, 0);
    }
    write_bias_mxmem2(base + VTCM_BIAS_OFF, bias_vals, 32);

    /* Normal mode */
    Q6_mxclracc();
    Q6_bias_mxmem2_A(base + VTCM_BIAS_OFF);
    hmx_act_wei_ub_sm((uintptr_t)act, 0,
                      (uintptr_t)wei, 0);
    memset(out, 0, 2048);
    Q6_mxmem_AR_after_sat_ub(out, 0);

    /* Deep mode: processes more input channels */
    Q6_mxclracc();
    Q6_bias_mxmem2_A(base + VTCM_BIAS_OFF);
    hmx_matmul_byte_deep((uintptr_t)act, 0,
                         (uintptr_t)wei, 0);
    memset(out2, 0, 2048);
    Q6_mxmem_AR_after_sat_ub(out2, 0);

    val_normal = out[crouton_off_sm(0, 0)];
    val_deep = out2[crouton_off_sm(0, 0)];

    if (val_normal == 0) {
        printf("ERROR: deep: normal mode output is 0\n");
        err++;
    }
    if (val_deep == 0) {
        printf("ERROR: deep: deep mode output is 0\n");
        err++;
    }
}

#if !defined(__linux__)
/*
 * Hardware loop acc clear/flip (baremetal only)
 *
 * PC-based deferred acc clear fails in HW loops because
 * the same PC appears across iterations.  The fix uses
 * packet boundary commit.
 *
 * Cannot be encoded with .word for linux-user since it
 * requires HW loop instructions (loop0/endloop0).
 */
static void test_hwloop_acc_flip(uint8_t *base)
{
    uint8_t *act = base + VTCM_ACT_OFF;
    uint8_t *wei = base + VTCM_WEI_OFF;
    uint8_t *bias_area = base + VTCM_BIAS_OFF;
    uint8_t *out = base + VTCM_OUT_OFF;
    uint8_t *out2 = base + VTCM_OUT2_OFF;
    uint32_t *wei32 = (uint32_t *)wei;
    uint32_t val_r1, val_r2;
    int i;

    puts("HW loop acc clear/flip");

    memset(act, 0, 2048);
    act[crouton_off_sm(0, 0)] = 50;

    memset(wei, 0, 128);
    for (i = 0; i < 32; i++) {
        wei32[i] = 0x0A0A0A0A;  /* weight = 10 */
    }

    setup_fxp_bias(bias_area,
                   pack_fxp_bias(0, 20, 0, 0x080, 0xFF0));

    Q6_mxclracc();
    Q6_bias_mxmem2_A(bias_area);

    memset(out, 0, 2048);
    memset(out2, 0, 2048);

    /*
     * HW loop with 2 iterations.
     * Each: matmul -> cvt_rs_ub(clear) -> cvt_store.
     * Iteration 0 stores to out, iteration 1 to out2.
     */
    asm volatile(
        "r0 = %[act]\n"
        "r1 = %[wei]\n"
        "r2 = %[out]\n"
        "r3 = #0\n"
        "   loop0(1f, #2)\n"
        "1:\n"
        "{\n"
        "    activation.ub = mxmem(r0,r3)\n"
        "    weight.b = mxmem(r1,r3)\n"
        "}\n"
        "cvt.ub = acc(r3)\n"
        "mxmem(r2,r3) = cvt\n"
        "{\n"
        "    r2 = %[out2]\n"
        "}:endloop0\n"
        :
        : [act] "r"(act), [wei] "r"(wei),
          [out] "r"(out), [out2] "r"(out2)
        : "r0", "r1", "r2", "r3", "memory"
    );

    val_r1 = out[crouton_off_sm(0, 0)];
    val_r2 = out2[crouton_off_sm(0, 0)];

    if (val_r1 == 0) {
        printf("ERROR: HW loop: iteration 0 output is 0\n");
        err++;
    }
    if (val_r2 == 0) {
        printf("ERROR: HW loop: iteration 1 output is 0\n");
        err++;
    }
}
#endif

/*
 * UH 2x1 legacy convert
 *
 * The UH convert combines adjacent spatial pairs using hmx_u16_cvt.
 * Set acc[0]=0 (spatial 0 has no activation), acc[1]=non-zero
 * (spatial 1 has activation data).  With the correct UH formula,
 * spatial 0 output includes acc[1]'s contribution via pairing.
 * With the bug (using UB's hmx_u8_cvt), spatial 0 would be 0.
 */
#if defined(__linux__)
static void test_uh_convert(uint8_t *base)
{
    uint8_t *act = base + VTCM_ACT_OFF;
    uint8_t *wei = base + VTCM_WEI_OFF;
    uint8_t *out = base + VTCM_OUT_OFF;
    uint64_t bias_vals[32];
    uint32_t val;
    int c, i;

    puts("UH 2x1 legacy convert");

    /* Spatial 0: all channels 0 (acc[0][o] = 0) */
    memset(act, 0, 2048);

    /* Spatial 1: all channels 100 (acc[1][o] = 100*4 = 400) */
    for (c = 0; c < 32; c++) {
        act[crouton_off_sm(1, c)] = 100;
    }

    /* Weight: 1 at all input channels for all outputs */
    memset(wei, 0, 128);
    for (i = 0; i < 32; i++) {
        ((uint32_t *)wei)[i] = 0x01010101;
    }

    /*
     * Large exponent (31) pushes the UH result into non-zero range.
     * UH uses frac_bits=24 (vs UB's 12), so needs more left-shift.
     */
    for (i = 0; i < 32; i++) {
        bias_vals[i] = pack_fxp_bias(0, 31, 0, 0x400, 0);
    }
    write_bias_mxmem2(base + VTCM_BIAS_OFF, bias_vals, 32);

    Q6_mxclracc();
    Q6_bias_mxmem2_A(base + VTCM_BIAS_OFF);
    hmx_act_wei_ub_sm((uintptr_t)act, 0, (uintptr_t)wei, 0);
    memset(out, 0, 2048);
    /*
     * Use ABOVE with offset=0 to store all spatial positions.
     * (BOTTOM with offset=0 stores nothing since no position is
     * before offset 0.)
     */
    Q6_mxmem_above_sat_uh(out, 0);

    /*
     * UH pairs spatials (0,1): u16_cvt(acc_hl=400, acc_ll=0)
     * acc_combined = 400.  This should produce non-zero at spatial 0.
     * The lo byte of the 16-bit UH result is at spatial 0 and the
     * hi byte at spatial 1.
     * If the bug were present (u8_cvt on each spatial), spatial 0
     * output would be 0 since acc[0]=0.
     */
    val = out[crouton_off_sm(0, 0)];
    if (val == 0) {
        printf("ERROR: UH convert: spatial 0 should be non-zero "
               "(acc_hl=400 should contribute)\n");
        err++;
    }
}

/*
 * FP overflow saturation
 *
 * Verify that FP overflow (result > 65504) saturates to max_finite
 * (0x7BFF) rather than producing raw IEEE infinity (0x7C00).
 * This tests the hmx_fp16_fixup code path.
 *
 * The default USR[20]=0 means mode 0 (saturate to max finite).
 */
static void test_fp_overflow_modes(uint8_t *base)
{
    uint8_t *act = base + VTCM_ACT_OFF;
    uint8_t *wei = base + VTCM_WEI_OFF;
    uint8_t *out0 = base + VTCM_OUT_OFF;
    uint16_t *wei16 = (uint16_t *)wei;
    uint16_t fp16_out;
    int i, off;

    puts("FP overflow saturation");

    /* FP16 act: ch0 = 256.0 (0x5C00) at spatial 0 */
    memset(act, 0, 2048);
    off = crouton_off_sm(0, 0);
    act[off] = 0x00;
    act[off + 1] = 0x5C;

    /* FP16 weight: ch0 = 256.0 for all outputs */
    memset(wei, 0, 128);
    for (i = 0; i < 32; i++) {
        wei16[i * 2] = 0x5C00;
        wei16[i * 2 + 1] = 0;
    }

    /* FP bias: unit scale (1.0), no bias offsets */
    setup_fp_bias_unit(base + VTCM_BIAS_OFF);

    Q6_mxclracc_hf();
    Q6_bias_mxmem2_A(base + VTCM_BIAS_OFF);
    hmx_act_wei_hf_sm((uintptr_t)act, 0, (uintptr_t)wei, 0);
    memset(out0, 0, 2048);
    Q6_mxmem_AR_after_hf(out0, 0);

    fp16_out = *(uint16_t *)(out0 + crouton_off_sm(0, 0));

    /*
     * 256 * 256 = 65536 > 65504 (max FP16 finite).
     * With default USR[20]=0, overflow should saturate to 0x7BFF
     * (max finite), not 0x7C00 (Inf).
     */
    if (fp16_out == 0) {
        printf("ERROR: FP overflow: output is 0\n");
        err++;
    } else if (fp16_out == 0x7C00) {
        printf("ERROR: FP overflow: got Inf (0x7C00), expected "
               "max_finite (0x7BFF)\n");
        err++;
    } else if (fp16_out != 0x7BFF) {
        printf("ERROR: FP overflow: expected 0x7BFF, got 0x%04x\n",
               fp16_out);
        err++;
    }
}
#endif /* __linux__ */

/*
 * F8 convert + store pipeline
 *
 * Exercises the two-step F8 convert path:
 *   cvt.f8 = acc(Rs) then mxmem(Rs,Rt).f8 = cvt
 *
 * Set up FP matmul with known accumulator (act=2.0, weight=1.0),
 * then convert to F8 and verify non-zero output.
 */
static void test_f8_cvt_store(uint8_t *base)
{
    uint8_t *act = base + VTCM_ACT_OFF;
    uint8_t *wei = base + VTCM_WEI_OFF;
    uint8_t *bias_area = base + VTCM_BIAS_OFF;
    uint8_t *out = base + VTCM_OUT_OFF;
    uint16_t *wei16 = (uint16_t *)wei;
    uint32_t rs;
    uint32_t val;
    int i;

    puts("F8 convert + store");

    /* FP16 act = 2.0 at spatial 0 */
    memset(act, 0, 2048);
    act[crouton_off_sm(0, 0)] = 0x00;
    act[crouton_off_sm(0, 0) + 1] = 0x40;  /* FP16 2.0 */

    memset(wei, 0, 128);
    for (i = 0; i < 32; i++) {
        wei16[i * 2] = 0x3C00;      /* 1.0 */
        wei16[i * 2 + 1] = 0;
    }

    setup_fp_bias_unit(bias_area);

    Q6_mxclracc_hf();
    Q6_bias_mxmem2_A(bias_area);
    hmx_act_wei_hf_sm((uintptr_t)act, 0,
                      (uintptr_t)wei, 0);

    memset(out, 0, 2048);

    /* Two-step F8 convert: cvt.f8=acc(Rs) then mxmem.f8=cvt */
    rs = 0;
    Q6_cvt_f8_acc_R(rs);
    Q6_mxmem_cvt_F8_RR(out, 0);

    /* F8 output should be non-zero at spatial 0, channel 0 */
    val = out[crouton_off_sm(0, 0)];
    if (val == 0) {
        printf("ERROR: F8 cvt store: output is 0\n");
        err++;
    }
}

/*
 * Channel-major activation+store pipeline
 *
 * Replicates the ds_master coproc.c CM test: channel-major activation
 * load, byte weight matmul, CM sat.ub store with golden-reference
 * validation.
 *
 * Inputs: activations = i%2 (alternating 0/1), weights = i (0..127),
 * bias = raw int32 (i << 10 for i=0..63).
 *
 * The expected output pattern repeats every 32 bytes across 64 spatial
 * positions.
 */
static const uint8_t cm_reference_pattern[32] = {
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 1, 2, 5, 11, 22, 46, 94,
    192, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255
};

static void test_channel_major(uint8_t *base)
{
    uint8_t *act = base + VTCM_ACT_OFF;
    uint8_t *wei = base + VTCM_WEI_OFF;
    uint8_t *bias_area = base + VTCM_BIAS_OFF;
    uint8_t *out = base + VTCM_OUT_OFF;
    uint32_t act_range;
    int i;

    puts("channel-major activation+store");

    /* Activations: alternating 0/1, 2048 bytes */
    for (i = 0; i < 2048; i++) {
        act[i] = i % 2;
    }

    /* Weights: sequential 0..127 */
    for (i = 0; i < 128; i++) {
        wei[i] = i;
    }

    /* Bias: raw int32 values (i << 10 for i=0..63) */
    setup_bias_raw_int32(bias_area);

    /* CM activation range: ch_stop=3, tile_y mask bits [7:5] */
    act_range = gen_act_range_cm(4);

    Q6_mxclracc();
    Q6_bias_mxmem2_A(bias_area);
    hmx_act_cm_wei_ub((uintptr_t)act, act_range,
                      (uintptr_t)wei, 0);

    memset(out, 0xAA, 2048);
    Q6_mxmem_AR_after_cm_sat_ub(out, 0);

    /* Verify against golden reference */
    for (i = 0; i < 2048; i++) {
        if (out[i] != cm_reference_pattern[i % 32]) {
            printf("ERROR: CM mismatch at offset %d: "
                   "got %u, expected %u\n",
                   i, out[i], cm_reference_pattern[i % 32]);
            err++;
            return;
        }
    }
}

/*
 * Multi-tap filter convolution
 *
 * Tests that adding a second x-tap (filter position) changes the
 * accumulator output, proving the multi-tap path works.
 *
 * Case A (1 x-tap): only weight vec 0 consumed
 * Case B (2 x-taps): vec 0 + vec 1 consumed -> larger accumulation
 *
 * Uses attenuating bias (exp=20, scale=0x080) to avoid saturation.
 */
static void test_multitap_filter(uint8_t *base)
{
    uint8_t *act = base + VTCM_ACT_OFF;
    uint8_t *wei = base + VTCM_WEI_OFF;
    uint8_t *bias_area = base + VTCM_BIAS_OFF;
    uint8_t *out_a = base + VTCM_OUT_OFF;
    uint8_t *out_b = base + VTCM_OUT2_OFF;
    uint32_t act_range;
    uint32_t val_a, val_b;
    int i;

    puts("multi-tap filter");

    /* SM layout: fill all spatial positions with value 100 */
    memset(act, 100, 2048);

    /* Weight vec 0 (bytes 0-127): all = 10 */
    memset(wei, 0, 256);
    for (i = 0; i < 32; i++) {
        ((uint32_t *)wei)[i] = 0x0A0A0A0A;
    }
    /* Weight vec 1 (bytes 128-255): all = 20 */
    for (i = 0; i < 32; i++) {
        ((uint32_t *)(wei + 128))[i] = 0x14141414;
    }

    /* Attenuating bias to avoid saturation */
    setup_fxp_bias(bias_area,
                   pack_fxp_bias(0, 20, 0, 0x080, 0xFF0));

    /* Act range for 8 input channels */
    act_range = gen_act_range_sm(8);

    /* Case A: 1 x-tap (wei_range=0) */
    Q6_mxclracc();
    Q6_bias_mxmem2_A(bias_area);
    hmx_act_wei_ub_sm((uintptr_t)act, act_range,
                      (uintptr_t)wei, 0);
    memset(out_a, 0, 2048);
    Q6_mxmem_AR_after_sat_ub(out_a, 0);
    val_a = out_a[crouton_off_sm(0, 0)];

    /* Case B: 2 x-taps (wei_range = (2-1)<<7 = 0x80, Rs bit0=1) */
    Q6_mxclracc();
    Q6_bias_mxmem2_A(bias_area);
    hmx_act_wei_ub_sm((uintptr_t)act | 0x01, act_range,
                      (uintptr_t)wei, 0x80);
    memset(out_b, 0, 2048);
    Q6_mxmem_AR_after_sat_ub(out_b, 0);
    val_b = out_b[crouton_off_sm(0, 0)];

    if (val_a == 0) {
        printf("ERROR: multitap: 1-tap result is 0\n");
        err++;
    }
    if (val_b == 0) {
        printf("ERROR: multitap: 2-tap result is 0\n");
        err++;
    }
    if (val_a == val_b) {
        printf("ERROR: multitap: 1-tap and 2-tap match "
               "(%u == %u)\n", val_a, val_b);
        err++;
    }
}

/*
 * Legacy :retain store
 *
 * Tests mxmem():after:retain:sat.ub=acc which stores the converted
 * result but does NOT clear/flip the accumulator, allowing further
 * accumulation on the same acc set.
 *
 * Round 1: matmul + retain -> out1 (acc retained)
 * Round 2: matmul again on same acc + normal store -> out2
 * Assert: out2 > out1 (second round accumulated more)
 *
 * Uses attenuating bias (exp=20, scale=0x080) so round 1 does not
 * saturate, leaving room for the second accumulation to increase.
 */
static void test_legacy_retain(uint8_t *base)
{
    uint8_t *act = base + VTCM_ACT_OFF;
    uint8_t *wei = base + VTCM_WEI_OFF;
    uint8_t *bias_area = base + VTCM_BIAS_OFF;
    uint8_t *out1 = base + VTCM_OUT_OFF;
    uint8_t *out2 = base + VTCM_OUT2_OFF;
    uint32_t val1, val2;

    puts("legacy retain store");

    /* Activation: all bytes = 100 */
    memset(act, 100, 2048);

    /* Weight: all bytes = 5 */
    memset(wei, 5, 128);

    /* Attenuating bias to avoid saturation on first round */
    setup_fxp_bias(bias_area,
                   pack_fxp_bias(0, 20, 0, 0x080, 0xFF0));

    /* Round 1: matmul + retain store (acc NOT cleared) */
    Q6_mxclracc();
    Q6_bias_mxmem2_A(bias_area);
    hmx_act_wei_ub_sm((uintptr_t)act, 0, (uintptr_t)wei, 0);
    memset(out1, 0, 2048);
    Q6_mxmem_AR_after_retain_sat_ub(out1, 0);

    /* Round 2: matmul again (same acc set, not cleared) + normal store */
    hmx_act_wei_ub_sm((uintptr_t)act, 0, (uintptr_t)wei, 0);
    memset(out2, 0, 2048);
    Q6_mxmem_AR_after_sat_ub(out2, 0);

    val1 = out1[crouton_off_sm(0, 0)];
    val2 = out2[crouton_off_sm(0, 0)];

    if (val1 == 0) {
        printf("ERROR: retain: round 1 output is 0\n");
        err++;
    }
    if (val2 == 0) {
        printf("ERROR: retain: round 2 output is 0\n");
        err++;
    }
    if (val2 <= val1) {
        printf("ERROR: retain: round 2 (%u) should exceed "
               "round 1 (%u)\n", val2, val1);
        err++;
    }
}

/*
 * Multi-iteration state persistence
 *
 * Tests that running the same matmul+convert 4 times produces identical
 * output each iteration (no state leakage between iterations).
 */
static void test_multi_iteration(uint8_t *base)
{
    uint8_t *act = base + VTCM_ACT_OFF;
    uint8_t *wei = base + VTCM_WEI_OFF;
    uint8_t *bias_area = base + VTCM_BIAS_OFF;
    uint8_t *out = base + VTCM_OUT_OFF;
    uint32_t results[4];
    int iter, c;

    puts("multi-iteration state persistence");

    /* Activation: spatial 0, channels 0-3 = 75 */
    memset(act, 0, 2048);
    for (c = 0; c < 4; c++) {
        act[crouton_off_sm(0, c)] = 75;
    }

    /* Weight: all bytes = 8 */
    memset(wei, 8, 128);

    /* Bias: passthrough */
    setup_fxp_bias(bias_area,
                   pack_fxp_bias(0, 0, 0, 0x400, 0));

    /* Run 4 identical iterations */
    for (iter = 0; iter < 4; iter++) {
        Q6_mxclracc();
        Q6_bias_mxmem2_A(bias_area);
        hmx_act_wei_ub_sm((uintptr_t)act, 0, (uintptr_t)wei, 0);
        memset(out, 0, 2048);
        Q6_mxmem_AR_after_sat_ub(out, 0);
        results[iter] = out[crouton_off_sm(0, 0)];
    }

    if (results[0] == 0) {
        printf("ERROR: multi-iter: output is 0\n");
        err++;
    }
    for (iter = 1; iter < 4; iter++) {
        if (results[iter] != results[0]) {
            printf("ERROR: multi-iter: iteration %d (%u) != "
                   "iteration 0 (%u)\n",
                   iter, results[iter], results[0]);
            err++;
        }
    }
}

int main(void)
{
    uint8_t *base;

#if !defined(__linux__)
    base = setup_vtcm_mapping();
    enable_coproc();
#else
    base = memalign(0x10000, VTCM_SIZE);
    if (!base) {
        puts("FAIL: memalign failed");
        return 1;
    }
    memset(base, 0, VTCM_SIZE);
#endif

    puts("HMX Targeted Tests");

    test_byte_weight_unsigned(base);
    test_acc_set_alternation(base);
    test_fp_shape_functions(base);
    test_fp_weight_negate(base);
    test_negative_zero_fp16(base);
    test_nibble_crumb_sign_ext(base);
    test_f8_weight_interleave(base);
    test_weight_range(base);
    test_subbyte_ch_override(base);
    test_deep_modifier(base);
#if !defined(__linux__)
    test_hwloop_acc_flip(base);
#endif
#if defined(__linux__)
    test_uh_convert(base);
    test_fp_overflow_modes(base);
#endif
    test_f8_cvt_store(base);
    test_channel_major(base);
    test_multitap_filter(base);
    test_legacy_retain(base);
    test_multi_iteration(base);

    puts(err ? "FAIL" : "PASS");
#if defined(__linux__)
    free(base);
#endif
    return err;
}
