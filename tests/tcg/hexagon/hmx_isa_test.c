/*
 * HMX ISA Comprehensive Test
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Self-checking tests for HMX (Hexagon Matrix eXtensions) ISA coverage.
 * Uses in-memory checks with error counting (check-tcg pattern) instead
 * of print-and-diff against a reference.
 *
 * Categories:
 *   A. FXP weight types (byte, SM, nibble, crumb, scrumb, ubit, sbit)
 *   B. FXP convert pipeline (exponent, scale, input_bias, shapes)
 *   C. FP matmul (FP16, F8 weight, weight negate)
 *   D. Accumulator management (alternation, mxswapacc)
 *   F. Seeded random FXP (byte, SM, nibble, crumb, scrumb, ubit, sbit)
 *   H. Multi-vector FXP byte (varying input depth)
 *   I. Multi-vector FP16
 *   M. Weight modifiers (single, after, drop, dilate)
 *   N. Activation modifiers (single, above)
 *   O. FP bias extended fields
 *   Q. Non-legacy convert (cvt.ub=acc + mxmem=cvt)
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int err;

static void check_nonzero(uint32_t val, const char *msg)
{
    if (val == 0) {
        printf("ERROR: %s: expected nonzero\n", msg);
        err++;
    }
}

static void check_ne(uint32_t a, uint32_t b, const char *msg)
{
    if (a == b) {
        printf("ERROR: %s: 0x%x == 0x%x\n", msg, a, b);
        err++;
    }
}

static void check_zero(uint32_t val, const char *msg)
{
    if (val != 0) {
        printf("ERROR: %s: expected 0, got 0x%x\n", msg, val);
        err++;
    }
}

static void check_mem_eq(const void *a, const void *b, int n,
                          const char *msg)
{
    if (memcmp(a, b, n) != 0) {
        printf("ERROR: %s: memory mismatch\n", msg);
        err++;
    }
}

static void check_mem_ne(const void *a, const void *b, int n,
                          const char *msg)
{
    if (memcmp(a, b, n) == 0) {
        printf("ERROR: %s: memory unexpectedly equal\n", msg);
        err++;
    }
}

static int any_byte_nonzero(const uint8_t *buf, int n)
{
    int i;

    for (i = 0; i < n; i++) {
        if (buf[i] != 0) {
            return 1;
        }
    }
    return 0;
}

#if !defined(__linux__)
/* ================================================================== */
/* Baremetal infrastructure                                            */
/* ================================================================== */

#include <hexagon_standalone.h>
#include <hmx_hexagon_protos.h>

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

/* Baremetal matmul wrappers using real asm */

static void hmx_matmul_byte(uintptr_t act, uint32_t ar,
                              uintptr_t wei, uint32_t wr)
{
    asm volatile("{\n"
                 "    activation.ub = mxmem(%[act],%[ar])\n"
                 "    weight.b = mxmem(%[wei],%[wr])\n"
                 "}\n"
                 :: [act] "r"(act), [ar] "r"(ar),
                    [wei] "r"(wei), [wr] "r"(wr)
                 : "memory");
}

static void hmx_matmul_sm(uintptr_t act, uint32_t ar,
                            uintptr_t wei, uint32_t wr)
{
    asm volatile("{\n"
                 "    activation.ub = mxmem(%[act],%[ar])\n"
                 "    weight.sm = mxmem(%[wei],%[wr])\n"
                 "}\n"
                 :: [act] "r"(act), [ar] "r"(ar),
                    [wei] "r"(wei), [wr] "r"(wr)
                 : "memory");
}

static void hmx_matmul_nibble(uintptr_t act, uint32_t ar,
                                uintptr_t wei, uint32_t wr)
{
    asm volatile("{\n"
                 "    activation.ub = mxmem(%[act],%[ar])\n"
                 "    weight.n = mxmem(%[wei],%[wr])\n"
                 "}\n"
                 :: [act] "r"(act), [ar] "r"(ar),
                    [wei] "r"(wei), [wr] "r"(wr)
                 : "memory");
}

static void hmx_matmul_crumb(uintptr_t act, uint32_t ar,
                               uintptr_t wei, uint32_t wr)
{
    asm volatile("{\n"
                 "    activation.ub = mxmem(%[act],%[ar])\n"
                 "    weight.c = mxmem(%[wei],%[wr])\n"
                 "}\n"
                 :: [act] "r"(act), [ar] "r"(ar),
                    [wei] "r"(wei), [wr] "r"(wr)
                 : "memory");
}

static void hmx_matmul_scrumb(uintptr_t act, uint32_t ar,
                                uintptr_t wei, uint32_t wr)
{
    asm volatile("{\n"
                 "    activation.ub = mxmem(%[act],%[ar])\n"
                 "    weight.sc = mxmem(%[wei],%[wr])\n"
                 "}\n"
                 :: [act] "r"(act), [ar] "r"(ar),
                    [wei] "r"(wei), [wr] "r"(wr)
                 : "memory");
}

static void hmx_matmul_ubit(uintptr_t act, uint32_t ar,
                              uintptr_t wei, uint32_t wr)
{
    asm volatile("{\n"
                 "    activation.ub = mxmem(%[act],%[ar])\n"
                 "    weight.ubit = mxmem(%[wei],%[wr])\n"
                 "}\n"
                 :: [act] "r"(act), [ar] "r"(ar),
                    [wei] "r"(wei), [wr] "r"(wr)
                 : "memory");
}

static void hmx_matmul_sbit(uintptr_t act, uint32_t ar,
                              uintptr_t wei, uint32_t wr)
{
    asm volatile("{\n"
                 "    activation.ub = mxmem(%[act],%[ar])\n"
                 "    weight.sbit = mxmem(%[wei],%[wr])\n"
                 "}\n"
                 :: [act] "r"(act), [ar] "r"(ar),
                    [wei] "r"(wei), [wr] "r"(wr)
                 : "memory");
}

static void hmx_matmul_fp16(uintptr_t act, uint32_t ar,
                              uintptr_t wei, uint32_t wr)
{
    asm volatile("{\n"
                 "    activation.hf = mxmem(%[act],%[ar])\n"
                 "    weight.hf = mxmem(%[wei],%[wr])\n"
                 "}\n"
                 :: [act] "r"(act), [ar] "r"(ar),
                    [wei] "r"(wei), [wr] "r"(wr)
                 : "memory");
}

static void hmx_matmul_fp16_f8w(uintptr_t act, uint32_t ar,
                                  uintptr_t wei, uint32_t wr)
{
    asm volatile("{\n"
                 "    activation.hf = mxmem(%[act],%[ar])\n"
                 "    weight.f8 = mxmem(%[wei],%[wr])\n"
                 "}\n"
                 :: [act] "r"(act), [ar] "r"(ar),
                    [wei] "r"(wei), [wr] "r"(wr)
                 : "memory");
}

static void hmx_matmul_byte_single(uintptr_t act, uint32_t ar,
                                     uintptr_t wei, uint32_t wr)
{
    asm volatile("{\n"
                 "    activation.ub = mxmem(%[act],%[ar])\n"
                 "    weight.b = mxmem(%[wei],%[wr]):single\n"
                 "}\n"
                 :: [act] "r"(act), [ar] "r"(ar),
                    [wei] "r"(wei), [wr] "r"(wr)
                 : "memory");
}

static void hmx_matmul_byte_after(uintptr_t act, uint32_t ar,
                                    uintptr_t wei, uint32_t wr)
{
    asm volatile("{\n"
                 "    activation.ub = mxmem(%[act],%[ar])\n"
                 "    weight.b = mxmem(%[wei],%[wr]):after\n"
                 "}\n"
                 :: [act] "r"(act), [ar] "r"(ar),
                    [wei] "r"(wei), [wr] "r"(wr)
                 : "memory");
}

static void hmx_matmul_byte_drop(uintptr_t act, uint32_t ar,
                                   uintptr_t wei, uint32_t wr)
{
    asm volatile("{\n"
                 "    activation.ub = mxmem(%[act],%[ar])\n"
                 "    weight.b = mxmem(%[wei],%[wr]):drop\n"
                 "}\n"
                 :: [act] "r"(act), [ar] "r"(ar),
                    [wei] "r"(wei), [wr] "r"(wr)
                 : "memory");
}

static void hmx_matmul_byte_dilate(uintptr_t act, uint32_t ar,
                                     uintptr_t wei, uint32_t wr)
{
    asm volatile("{\n"
                 "    activation.ub = mxmem(%[act],%[ar])\n"
                 "    weight.b = mxmem(%[wei],%[wr]):dilate\n"
                 "}\n"
                 :: [act] "r"(act), [ar] "r"(ar),
                    [wei] "r"(wei), [wr] "r"(wr)
                 : "memory");
}

static void hmx_matmul_byte_act_single(uintptr_t act, uint32_t ar,
                                         uintptr_t wei, uint32_t wr)
{
    asm volatile("{\n"
                 "    activation.ub = mxmem(%[act],%[ar]):single\n"
                 "    weight.b = mxmem(%[wei],%[wr])\n"
                 "}\n"
                 :: [act] "r"(act), [ar] "r"(ar),
                    [wei] "r"(wei), [wr] "r"(wr)
                 : "memory");
}

static void hmx_matmul_byte_act_above(uintptr_t act, uint32_t ar,
                                        uintptr_t wei, uint32_t wr)
{
    asm volatile("{\n"
                 "    activation.ub = mxmem(%[act],%[ar]):above\n"
                 "    weight.b = mxmem(%[wei],%[wr])\n"
                 "}\n"
                 :: [act] "r"(act), [ar] "r"(ar),
                    [wei] "r"(wei), [wr] "r"(wr)
                 : "memory");
}

#else
/* ================================================================== */
/* Linux-user infrastructure                                           */
/* ================================================================== */

#include <stdlib.h>
#include "hmx_intrinsics.h"

/* Linux-user matmul wrappers: paired activation+weight packets */

static void hmx_matmul_byte(uintptr_t act, uint32_t ar,
                              uintptr_t wei, uint32_t wr)
{
    _HMX_PAIRED(_HMX_ACT_UB_PAIRED, _HMX_WEI_B_PAIRED,
                act, ar, wei, wr);
}

static void hmx_matmul_sm(uintptr_t act, uint32_t ar,
                            uintptr_t wei, uint32_t wr)
{
    _HMX_PAIRED(_HMX_ACT_UB_PAIRED, _HMX_WEI_SM_PAIRED,
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

static void hmx_matmul_scrumb(uintptr_t act, uint32_t ar,
                                uintptr_t wei, uint32_t wr)
{
    _HMX_PAIRED(_HMX_ACT_UB_PAIRED, _HMX_WEI_SC_PAIRED,
                act, ar, wei, wr);
}

static void hmx_matmul_ubit(uintptr_t act, uint32_t ar,
                              uintptr_t wei, uint32_t wr)
{
    _HMX_PAIRED(_HMX_ACT_UB_PAIRED, _HMX_WEI_UBIT_PAIRED,
                act, ar, wei, wr);
}

static void hmx_matmul_sbit(uintptr_t act, uint32_t ar,
                              uintptr_t wei, uint32_t wr)
{
    _HMX_PAIRED(_HMX_ACT_UB_PAIRED, _HMX_WEI_SBIT_PAIRED,
                act, ar, wei, wr);
}

static void hmx_matmul_fp16(uintptr_t act, uint32_t ar,
                              uintptr_t wei, uint32_t wr)
{
    _HMX_PAIRED(_HMX_ACT_HF_PAIRED, _HMX_WEI_HF_PAIRED,
                act, ar, wei, wr);
}

static void hmx_matmul_fp16_f8w(uintptr_t act, uint32_t ar,
                                  uintptr_t wei, uint32_t wr)
{
    _HMX_PAIRED(_HMX_ACT_HF_PAIRED, _HMX_WEI_F8_PAIRED,
                act, ar, wei, wr);
}

static void hmx_matmul_byte_single(uintptr_t act, uint32_t ar,
                                     uintptr_t wei, uint32_t wr)
{
    _HMX_PAIRED(_HMX_ACT_UB_PAIRED, _HMX_WEI_B_SINGLE_PAIRED,
                act, ar, wei, wr);
}

static void hmx_matmul_byte_after(uintptr_t act, uint32_t ar,
                                    uintptr_t wei, uint32_t wr)
{
    _HMX_PAIRED(_HMX_ACT_UB_PAIRED, _HMX_WEI_B_AFTER_PAIRED,
                act, ar, wei, wr);
}

static void hmx_matmul_byte_drop(uintptr_t act, uint32_t ar,
                                   uintptr_t wei, uint32_t wr)
{
    _HMX_PAIRED(_HMX_ACT_UB_PAIRED, _HMX_WEI_B_DROP_PAIRED,
                act, ar, wei, wr);
}

static void hmx_matmul_byte_dilate(uintptr_t act, uint32_t ar,
                                     uintptr_t wei, uint32_t wr)
{
    _HMX_PAIRED(_HMX_ACT_UB_PAIRED, _HMX_WEI_B_DILATE_PAIRED,
                act, ar, wei, wr);
}

static void hmx_matmul_byte_act_single(uintptr_t act, uint32_t ar,
                                         uintptr_t wei, uint32_t wr)
{
    _HMX_PAIRED(_HMX_ACT_UB_SINGLE_PAIRED, _HMX_WEI_B_PAIRED,
                act, ar, wei, wr);
}

static void hmx_matmul_byte_act_above(uintptr_t act, uint32_t ar,
                                        uintptr_t wei, uint32_t wr)
{
    _HMX_PAIRED(_HMX_ACT_UB_ABOVE_PAIRED, _HMX_WEI_B_PAIRED,
                act, ar, wei, wr);
}

#endif

/* ================================================================== */
/* VTCM layout offsets                                                 */
/* ================================================================== */

#define VTCM_SIZE        0x10000
#define VTCM_ACT_OFF     0x0000
#define VTCM_WEI_OFF     0x1000
#define VTCM_BIAS_OFF    0x2000
#define VTCM_OUT_OFF     0x3000
#define VTCM_OUT2_OFF    0x3800

/* Scratch area for reproducibility checks */
#define VTCM_SCRATCH_OFF 0x4000

/* ================================================================== */
/* Common helpers                                                      */
/* ================================================================== */

static int crouton_off_sm(int spatial, int channel)
{
    return ((spatial >> 2) << 7) | (channel << 2) | (spatial & 3);
}

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

/* Common bias configurations */

static uint64_t fxp_passthru_bias(void)
{
    return pack_fxp_bias(0, 20, 0, 0x080, 0xFF0);
}

static uint64_t fxp_shape_bias(uint16_t shape)
{
    return pack_fxp_bias(0, 20, shape, 0x080, 0xFF0);
}

static uint64_t fxp_exp_bias(uint16_t exp)
{
    return pack_fxp_bias(0, exp, 0, 0x080, 0xFF0);
}

static uint64_t fxp_scale_bias(uint16_t scale)
{
    return pack_fxp_bias(0, 20, 0, scale, 0xFF0);
}

static uint64_t fxp_input_bias(int32_t ibias)
{
    return pack_fxp_bias(ibias, 20, 0, 0x080, 0xFF0);
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

/* ================================================================== */
/* Deterministic PRNG (xorshift32)                                     */
/* ================================================================== */

static uint32_t prng_state;

static void prng_seed(uint32_t seed)
{
    prng_state = seed;
}

static uint32_t prng_next(void)
{
    uint32_t x = prng_state;

    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    prng_state = x;
    return x;
}

static uint8_t prng_u8(void)
{
    return (uint8_t)(prng_next() >> 16);
}

static uint16_t prng_fp16_small(void)
{
    uint32_t r = prng_next();
    uint16_t sign = (r >> 15) & 1;
    uint16_t exp = ((r >> 8) & 0x3) + 12;
    uint16_t mant = r & 0x3FF;

    return (sign << 15) | (exp << 10) | mant;
}

/* ================================================================== */
/* Data fill helpers                                                   */
/* ================================================================== */

#define CROUTON_SPATIAL  64
#define CROUTON_CHANNELS 32

static void fill_act_random(uint8_t *act)
{
    int s, c;

    for (s = 0; s < CROUTON_SPATIAL; s++) {
        for (c = 0; c < CROUTON_CHANNELS; c++) {
            act[crouton_off_sm(s, c)] = prng_u8();
        }
    }
}

static void fill_wei_random(uint8_t *wei)
{
    uint32_t *w32 = (uint32_t *)wei;
    int i;

    for (i = 0; i < 32; i++) {
        w32[i] = prng_next();
    }
}

static void fill_wei_multi_random(uint8_t *wei, int num_vecs)
{
    uint32_t *w32 = (uint32_t *)wei;
    int i;

    for (i = 0; i < num_vecs * 32; i++) {
        w32[i] = prng_next();
    }
}

static void fill_fxp_bias_random(uint8_t *bias_area, uint16_t exp)
{
    uint64_t bias_vals[32];
    int i;

    for (i = 0; i < 32; i++) {
        int32_t ibias = (int32_t)(prng_next() & 0xFFFF) - 0x8000;

        bias_vals[i] = pack_fxp_bias(ibias, exp, 0, 0x080, 0xFF0);
    }
    write_bias_mxmem2(bias_area, bias_vals, 32);
}

static void fill_fp16_wei_multi_random(uint8_t *wei, int num_vecs)
{
    uint16_t *w16 = (uint16_t *)wei;
    int i;

    for (i = 0; i < num_vecs * 64; i++) {
        w16[i] = prng_fp16_small();
    }
}

static void fill_fp16_act_random_depth(uint8_t *act, int input_depth)
{
    uint16_t *act16;
    int s, c;
    int nch = input_depth < 32 ? input_depth : 32;

    memset(act, 0, 2048);
    for (s = 0; s < 32; s++) {
        for (c = 0; c < nch; c++) {
            act16 = (uint16_t *)(act + crouton_off_sm(s * 2, c));
            *act16 = prng_fp16_small();
        }
    }
}

/* Multi-vector helpers */

static uint32_t spatial_major_convert(uint32_t val)
{
    uint32_t spatial_bits = (val >> 5) & 0x3;
    uint32_t depth_bits = (val & 0x1F) << 2;

    val &= ~0x7Fu;
    return val | depth_bits | spatial_bits;
}

static uint32_t gen_act_range_sm(int input_depth)
{
    uint32_t dc0 = ((input_depth - 1) & 0x1F) & ~1u;
    uint32_t temp = dc0 | (7u << 8);

    return spatial_major_convert(temp);
}

/* ================================================================== */
/* Matmul function pointer type                                        */
/* ================================================================== */

typedef void (*matmul_fn_t)(uintptr_t, uint32_t, uintptr_t, uint32_t);

/* ================================================================== */
/* Helper: run matmul + convert to FXP output crouton                  */
/* ================================================================== */

static void run_fxp_matmul(uint8_t *base, matmul_fn_t matmul,
                             uintptr_t act, uint32_t ar,
                             uintptr_t wei, uint32_t wr,
                             uint8_t *out)
{
    Q6_mxclracc();
    Q6_bias_mxmem2_A(base + VTCM_BIAS_OFF);
    matmul(act, ar, wei, wr);
    memset(out, 0, 2048);
    Q6_mxmem_AR_after_sat_ub(out, 0);
}

static void run_fp_matmul(uint8_t *base, matmul_fn_t matmul,
                            uintptr_t act, uint32_t ar,
                            uintptr_t wei, uint32_t wr,
                            uint8_t *out)
{
    Q6_mxclracc_hf();
    Q6_bias_mxmem2_A(base + VTCM_BIAS_OFF);
    matmul(act, ar, wei, wr);
    memset(out, 0, 2048);
    Q6_mxmem_AR_after_hf(out, 0);
}

/* ================================================================== */
/* Category A: FXP Weight Types                                        */
/*                                                                     */
/* Each weight type interprets the weight vector differently.          */
/* We verify each type produces non-zero output and that different     */
/* types produce different outputs from the same weight data.          */
/* ================================================================== */

static void run_fxp_weight_type(uint8_t *base, matmul_fn_t matmul,
                                 uint32_t wei_word, uint64_t bias)
{
    uint8_t *act = base + VTCM_ACT_OFF;
    uint8_t *wei = base + VTCM_WEI_OFF;
    uint8_t *out = base + VTCM_OUT_OFF;
    uint32_t *wei32 = (uint32_t *)wei;
    int i;

    memset(act, 0, 2048);
    act[crouton_off_sm(0, 0)] = 255;
    act[crouton_off_sm(0, 1)] = 200;
    act[crouton_off_sm(0, 2)] = 150;
    act[crouton_off_sm(0, 3)] = 100;
    act[crouton_off_sm(1, 0)] = 128;

    for (i = 0; i < 32; i++) {
        wei32[i] = wei_word;
    }

    setup_fxp_bias(base + VTCM_BIAS_OFF, bias);
    run_fxp_matmul(base, matmul, (uintptr_t)act, 0,
                   (uintptr_t)wei, 0, out);
}

static void test_all_weight_types(uint8_t *base)
{
    uint8_t *out = base + VTCM_OUT_OFF;
    uint8_t *scratch = base + VTCM_SCRATCH_OFF;
    uint32_t byte_val, sm_val, nibble_val, crumb_val;
    uint32_t scrumb_val, ubit_val, sbit_val;

    puts("A: FXP weight types");

    /* A1: byte weights */
    run_fxp_weight_type(base, hmx_matmul_byte,
                         0x01020304, fxp_passthru_bias());
    byte_val = out[crouton_off_sm(0, 0)];
    check_nonzero(byte_val, "A1_byte");

    /* A2: SM weights (same data as byte - should differ) */
    run_fxp_weight_type(base, hmx_matmul_sm,
                         0x01020304, fxp_passthru_bias());
    sm_val = out[crouton_off_sm(0, 0)];
    check_nonzero(sm_val, "A2_sm");

    /* A3: nibble weights */
    run_fxp_weight_type(base, hmx_matmul_nibble,
                         0x07060504, fxp_passthru_bias());
    nibble_val = out[crouton_off_sm(0, 0)];
    check_nonzero(nibble_val, "A3_nibble");

    /* A4: crumb weights (all +1) */
    run_fxp_weight_type(base, hmx_matmul_crumb,
                         0x55555555, fxp_exp_bias(24));
    crumb_val = out[crouton_off_sm(0, 0)];
    check_nonzero(crumb_val, "A4_crumb");

    /* A5: scrumb weights (all +2) */
    run_fxp_weight_type(base, hmx_matmul_scrumb,
                         0x00000000, fxp_exp_bias(24));
    scrumb_val = out[crouton_off_sm(0, 0)];
    check_nonzero(scrumb_val, "A5_scrumb");

    /* A6: ubit weights (all 1) */
    run_fxp_weight_type(base, hmx_matmul_ubit,
                         0xFFFFFFFF, fxp_exp_bias(24));
    ubit_val = out[crouton_off_sm(0, 0)];
    check_nonzero(ubit_val, "A6_ubit");

    /* A7: sbit weights (mixed +1/-1) */
    run_fxp_weight_type(base, hmx_matmul_sbit,
                         0x00010000, fxp_exp_bias(24));
    sbit_val = out[crouton_off_sm(0, 0)];
    check_nonzero(sbit_val, "A7_sbit");

    /*
     * Verify different weight types produce different outputs.
     * With the same activation data and carefully chosen weight words,
     * each type should yield a distinguishable result.
     */
    check_ne(byte_val, nibble_val, "A byte!=nibble");
    check_ne(crumb_val, scrumb_val, "A crumb!=scrumb");
    check_ne(ubit_val, sbit_val, "A ubit!=sbit");

    /* A8: byte with high-bit weights (signed negative -> negative acc) */
    run_fxp_weight_type(base, hmx_matmul_byte,
                         0xC0C0C0C0, fxp_passthru_bias());
    /* Negative acc with sat.ub clips to 0; verify differs from positive */
    check_ne(out[crouton_off_sm(0, 0)], byte_val, "A8 neg!=pos byte");

    /* Save output for comparison */
    memcpy(scratch, out, 2048);

    /* A9: SM positive */
    run_fxp_weight_type(base, hmx_matmul_sm,
                         0x02020202, fxp_passthru_bias());
    sm_val = out[crouton_off_sm(0, 0)];
    check_nonzero(sm_val, "A9_sm_pos");

    /* A10: SM negative (negative acc -> sat.ub may clip to 0) */
    run_fxp_weight_type(base, hmx_matmul_sm,
                         0x82828282, fxp_passthru_bias());
    check_ne(sm_val, out[crouton_off_sm(0, 0)], "A10 sm pos!=neg");
}

/* ================================================================== */
/* Category B: FXP Convert Pipeline                                    */
/*                                                                     */
/* Known accumulator, then vary exponent/scale/bias/shape.             */
/* Checks that parameter changes produce observable output changes.    */
/* ================================================================== */

static void setup_known_fxp_acc(uint8_t *base)
{
    uint8_t *act = base + VTCM_ACT_OFF;
    uint8_t *wei = base + VTCM_WEI_OFF;
    uint32_t *wei32 = (uint32_t *)wei;
    int i;

    memset(act, 0, 2048);
    act[crouton_off_sm(0, 0)] = 100;
    act[crouton_off_sm(1, 0)] = 200;

    memset(wei, 0, 128);
    for (i = 0; i < 32; i++) {
        wei32[i] = 0x00000050;
    }
}

static void test_cvt_exponent(uint8_t *base)
{
    uint8_t *out = base + VTCM_OUT_OFF;
    uint32_t vals[4];
    const int exps[] = {15, 18, 20, 24};
    int i;

    puts("B1: FXP exponent");

    for (i = 0; i < 4; i++) {
        setup_known_fxp_acc(base);
        setup_fxp_bias(base + VTCM_BIAS_OFF, fxp_exp_bias(exps[i]));
        run_fxp_matmul(base, hmx_matmul_byte,
                       (uintptr_t)(base + VTCM_ACT_OFF), 0,
                       (uintptr_t)(base + VTCM_WEI_OFF), 0, out);
        vals[i] = out[crouton_off_sm(0, 0)];
    }

    /* Higher exponent should produce larger output (until saturation) */
    check_ne(vals[0], vals[2], "B1 exp15!=exp20");
    check_ne(vals[1], vals[2], "B1 exp18!=exp20");
}

static void test_cvt_scale(uint8_t *base)
{
    uint8_t *out = base + VTCM_OUT_OFF;
    uint32_t vals[4];
    const uint16_t scales[] = {0x010, 0x080, 0x200, 0x800};
    int i;

    puts("B2: FXP scale");

    for (i = 0; i < 4; i++) {
        setup_known_fxp_acc(base);
        setup_fxp_bias(base + VTCM_BIAS_OFF, fxp_scale_bias(scales[i]));
        run_fxp_matmul(base, hmx_matmul_byte,
                       (uintptr_t)(base + VTCM_ACT_OFF), 0,
                       (uintptr_t)(base + VTCM_WEI_OFF), 0, out);
        vals[i] = out[crouton_off_sm(0, 0)];
    }

    /* Larger scale should produce larger output */
    check_ne(vals[0], vals[2], "B2 scale_lo!=scale_hi");
}

static void test_cvt_input_bias(uint8_t *base)
{
    uint8_t *out = base + VTCM_OUT_OFF;
    uint32_t val_zero, val_pos, val_neg;

    puts("B3: FXP input bias");

    /* input_bias = 0 */
    setup_known_fxp_acc(base);
    setup_fxp_bias(base + VTCM_BIAS_OFF, fxp_input_bias(0));
    run_fxp_matmul(base, hmx_matmul_byte,
                   (uintptr_t)(base + VTCM_ACT_OFF), 0,
                   (uintptr_t)(base + VTCM_WEI_OFF), 0, out);
    val_zero = out[crouton_off_sm(0, 0)];
    check_nonzero(val_zero, "B3 ibias=0");

    /* input_bias = +50000 (should increase output) */
    setup_known_fxp_acc(base);
    setup_fxp_bias(base + VTCM_BIAS_OFF, fxp_input_bias(50000));
    run_fxp_matmul(base, hmx_matmul_byte,
                   (uintptr_t)(base + VTCM_ACT_OFF), 0,
                   (uintptr_t)(base + VTCM_WEI_OFF), 0, out);
    val_pos = out[crouton_off_sm(0, 0)];

    /* input_bias = -1000 (should decrease output) */
    setup_known_fxp_acc(base);
    setup_fxp_bias(base + VTCM_BIAS_OFF, fxp_input_bias(-1000));
    run_fxp_matmul(base, hmx_matmul_byte,
                   (uintptr_t)(base + VTCM_ACT_OFF), 0,
                   (uintptr_t)(base + VTCM_WEI_OFF), 0, out);
    val_neg = out[crouton_off_sm(0, 0)];

    check_ne(val_zero, val_pos, "B3 ibias 0!=pos");
    check_ne(val_zero, val_neg, "B3 ibias 0!=neg");
}

static void test_cvt_shapes(uint8_t *base)
{
    uint8_t *out = base + VTCM_OUT_OFF;
    uint32_t vals[4];
    int shape;

    puts("B4: FXP shapes");

    for (shape = 0; shape < 4; shape++) {
        setup_known_fxp_acc(base);
        setup_fxp_bias(base + VTCM_BIAS_OFF,
                       fxp_shape_bias(shape));
        run_fxp_matmul(base, hmx_matmul_byte,
                       (uintptr_t)(base + VTCM_ACT_OFF), 0,
                       (uintptr_t)(base + VTCM_WEI_OFF), 0, out);
        vals[shape] = out[crouton_off_sm(0, 0)];
    }

    /* With positive acc: relu (2) and abs (3) should match pass (0) */
    check_nonzero(vals[0], "B4 shape0");
    check_nonzero(vals[2], "B4 shape2 relu");
    check_nonzero(vals[3], "B4 shape3 abs");
}

/* ================================================================== */
/* Category C: FP Matmul                                               */
/* ================================================================== */

static void test_fp16_matmul(uint8_t *base)
{
    uint8_t *act = base + VTCM_ACT_OFF;
    uint8_t *wei = base + VTCM_WEI_OFF;
    uint8_t *out = base + VTCM_OUT_OFF;
    uint16_t *act16;
    uint16_t *wei16 = (uint16_t *)wei;
    uint16_t *out16;
    uint16_t f16_out;
    int i;

    puts("C1: FP16 matmul");

    memset(act, 0, 2048);
    memset(wei, 0, 128);

    /* FP16 act: ch0=3.0, ch1=2.0 */
    act16 = (uint16_t *)(act + crouton_off_sm(0, 0));
    *act16 = 0x4200;  /* 3.0 */
    act16 = (uint16_t *)(act + crouton_off_sm(0, 1));
    *act16 = 0x4000;  /* 2.0 */

    /* FP16 weight: ch0=1.0, ch1=0.5 for all output channels */
    for (i = 0; i < 32; i++) {
        wei16[i * 2] = 0x3C00;      /* 1.0 */
        wei16[i * 2 + 1] = 0x3800;  /* 0.5 */
    }

    setup_fp_bias_unit(base + VTCM_BIAS_OFF);
    run_fp_matmul(base, hmx_matmul_fp16,
                  (uintptr_t)act, 0, (uintptr_t)wei, 0, out);

    /* acc = 3.0*1.0 + 2.0*0.5 = 4.0 -> FP16 0x4400 */
    out16 = (uint16_t *)(out + crouton_off_sm(0, 0));
    f16_out = *out16;
    check_nonzero(f16_out, "C1 fp16 nonzero");

    /* Verify sign is positive */
    if ((f16_out >> 15) & 1) {
        printf("ERROR: C1 fp16 should be positive\n");
        err++;
    }
}

static void test_fp_f8_weight(uint8_t *base)
{
    uint8_t *act = base + VTCM_ACT_OFF;
    uint8_t *wei = base + VTCM_WEI_OFF;
    uint8_t *out = base + VTCM_OUT_OFF;
    uint16_t *act16;
    uint16_t *out16;
    uint16_t f16_out;
    int i;

    puts("C2: F8 weight");

    memset(act, 0, 2048);
    memset(wei, 0, 128);

    /* FP16 act = 2.0 */
    act16 = (uint16_t *)(act + crouton_off_sm(0, 0));
    *act16 = 0x4000;

    /* F8 weight: 0x3C at ch0 (expands to ~0.5 in FP16) */
    for (i = 0; i < 32; i++) {
        ((uint32_t *)wei)[i] = 0x0000003C;
    }

    setup_fp_bias_unit(base + VTCM_BIAS_OFF);
    run_fp_matmul(base, hmx_matmul_fp16_f8w,
                  (uintptr_t)act, 0, (uintptr_t)wei, 0, out);

    out16 = (uint16_t *)(out + crouton_off_sm(0, 0));
    f16_out = *out16;
    check_nonzero(f16_out, "C2 f8w nonzero");
}

static void test_fp_weight_negate(uint8_t *base)
{
    uint8_t *act = base + VTCM_ACT_OFF;
    uint8_t *wei = base + VTCM_WEI_OFF;
    uint8_t *out = base + VTCM_OUT_OFF;
    uint16_t *act16;
    uint16_t *wei16 = (uint16_t *)wei;
    uint16_t *out16;
    uint16_t f16_no_neg, f16_neg;
    int i;

    puts("C3: FP weight negate");

    memset(act, 0, 2048);
    memset(wei, 0, 128);

    act16 = (uint16_t *)(act + crouton_off_sm(0, 0));
    *act16 = 0x4000;  /* 2.0 */

    for (i = 0; i < 32; i++) {
        wei16[i * 2] = 0x3C00;  /* 1.0 */
        wei16[i * 2 + 1] = 0;
    }

    setup_fp_bias_unit(base + VTCM_BIAS_OFF);

    /* Without negate */
    run_fp_matmul(base, hmx_matmul_fp16,
                  (uintptr_t)act, 0, (uintptr_t)wei, 0, out);
    out16 = (uint16_t *)(out + crouton_off_sm(0, 0));
    f16_no_neg = *out16;

    /* With negate: Rs[5]=1 in weight address */
    run_fp_matmul(base, hmx_matmul_fp16,
                  (uintptr_t)act, 0,
                  (uintptr_t)wei | (1 << 5), 0, out);
    out16 = (uint16_t *)(out + crouton_off_sm(0, 0));
    f16_neg = *out16;

    /* Without negate: positive */
    if (f16_no_neg == 0 || ((f16_no_neg >> 15) & 1)) {
        printf("ERROR: C3 no-negate should be positive\n");
        err++;
    }
    /* With negate: negative */
    if (f16_neg == 0 || !((f16_neg >> 15) & 1)) {
        printf("ERROR: C3 negate should be negative\n");
        err++;
    }
}

/* ================================================================== */
/* Category D: Accumulator Management                                  */
/* ================================================================== */

static void test_acc_alternation(uint8_t *base)
{
    uint8_t *act = base + VTCM_ACT_OFF;
    uint8_t *wei = base + VTCM_WEI_OFF;
    uint8_t *out1 = base + VTCM_OUT_OFF;
    uint8_t *out2 = base + VTCM_OUT2_OFF;
    uint32_t *wei32 = (uint32_t *)wei;
    uint32_t r1, r2, r3;
    int i;

    puts("D1: FXP acc alternation");

    memset(act, 0, 2048);
    act[crouton_off_sm(0, 0)] = 100;
    setup_fxp_bias(base + VTCM_BIAS_OFF, fxp_passthru_bias());

    /* Round 1: weight=0x30 */
    memset(wei, 0, 128);
    for (i = 0; i < 32; i++) {
        wei32[i] = 0x00000030;
    }
    Q6_mxclracc();
    Q6_bias_mxmem2_A(base + VTCM_BIAS_OFF);
    hmx_matmul_byte((uintptr_t)act, 0, (uintptr_t)wei, 0);
    memset(out1, 0, 2048);
    Q6_mxmem_AR_after_sat_ub(out1, 0);
    r1 = out1[crouton_off_sm(0, 0)];

    /* Round 2: weight=0x20 (acc set flipped after convert) */
    for (i = 0; i < 32; i++) {
        wei32[i] = 0x00000020;
    }
    Q6_bias_mxmem2_A(base + VTCM_BIAS_OFF);
    hmx_matmul_byte((uintptr_t)act, 0, (uintptr_t)wei, 0);
    memset(out2, 0, 2048);
    Q6_mxmem_AR_after_sat_ub(out2, 0);
    r2 = out2[crouton_off_sm(0, 0)];

    /* Round 3: weight=0x10 */
    for (i = 0; i < 32; i++) {
        wei32[i] = 0x00000010;
    }
    Q6_bias_mxmem2_A(base + VTCM_BIAS_OFF);
    hmx_matmul_byte((uintptr_t)act, 0, (uintptr_t)wei, 0);
    memset(out1, 0, 2048);
    Q6_mxmem_AR_after_sat_ub(out1, 0);
    r3 = out1[crouton_off_sm(0, 0)];

    check_nonzero(r1, "D1 round 1");
    check_nonzero(r2, "D1 round 2");
    check_nonzero(r3, "D1 round 3");
    check_ne(r1, r2, "D1 r1!=r2");
    check_ne(r2, r3, "D1 r2!=r3");
}

static void test_swapacc(uint8_t *base)
{
    uint8_t *act = base + VTCM_ACT_OFF;
    uint8_t *wei = base + VTCM_WEI_OFF;
    uint8_t *out = base + VTCM_OUT_OFF;
    uint8_t *out2 = base + VTCM_OUT2_OFF;
    uint32_t *wei32 = (uint32_t *)wei;
    uint32_t val_rt, val_swap;
    int i;

    puts("D2: mxswapacc");

    memset(act, 0, 2048);
    act[crouton_off_sm(0, 0)] = 100;
    setup_fxp_bias(base + VTCM_BIAS_OFF, fxp_passthru_bias());

    memset(wei, 0, 128);
    for (i = 0; i < 32; i++) {
        wei32[i] = 0x00000040;
    }

    /* Load into primary acc */
    Q6_mxclracc();
    Q6_bias_mxmem2_A(base + VTCM_BIAS_OFF);
    hmx_matmul_byte((uintptr_t)act, 0, (uintptr_t)wei, 0);

    /* Round-trip swap: primary -> secondary -> primary */
    Q6_mxswapacc();
    Q6_mxswapacc();

    memset(out, 0xCC, 2048);
    Q6_mxmem_AR_after_sat_ub(out, 0);
    val_rt = out[crouton_off_sm(0, 0)];
    check_nonzero(val_rt, "D2 roundtrip");

    /* Single swap: data goes to secondary, primary is zero */
    Q6_mxclracc();
    Q6_bias_mxmem2_A(base + VTCM_BIAS_OFF);
    hmx_matmul_byte((uintptr_t)act, 0, (uintptr_t)wei, 0);
    Q6_mxswapacc();

    /* Convert primary (should be zero-ish since data is in secondary) */
    memset(out2, 0xCC, 2048);
    Q6_mxmem_AR_after_sat_ub(out2, 0);
    val_swap = out2[crouton_off_sm(0, 0)];

    check_ne(val_rt, val_swap, "D2 roundtrip!=swapped");
}

static void test_acc_alternation_fp(uint8_t *base)
{
    uint8_t *act = base + VTCM_ACT_OFF;
    uint8_t *wei = base + VTCM_WEI_OFF;
    uint8_t *out1 = base + VTCM_OUT_OFF;
    uint8_t *out2 = base + VTCM_OUT2_OFF;
    uint16_t *act16;
    uint16_t *wei16 = (uint16_t *)wei;
    uint16_t *out16;
    uint16_t r1, r2;
    int i;

    puts("D3: FP acc alternation");

    memset(act, 0, 2048);
    memset(wei, 0, 128);

    act16 = (uint16_t *)(act + crouton_off_sm(0, 0));
    *act16 = 0x4200;  /* 3.0 */

    setup_fp_bias_unit(base + VTCM_BIAS_OFF);

    /* Round 1: weight=2.0, acc=6.0 */
    for (i = 0; i < 32; i++) {
        wei16[i * 2] = 0x4000;
        wei16[i * 2 + 1] = 0;
    }

    Q6_mxclracc_hf();
    Q6_bias_mxmem2_A(base + VTCM_BIAS_OFF);
    hmx_matmul_fp16((uintptr_t)act, 0, (uintptr_t)wei, 0);
    memset(out1, 0, 2048);
    Q6_mxmem_AR_after_hf(out1, 0);

    out16 = (uint16_t *)(out1 + crouton_off_sm(0, 0));
    r1 = *out16;

    /* Round 2: weight=0.5, acc=1.5 */
    for (i = 0; i < 32; i++) {
        wei16[i * 2] = 0x3800;
        wei16[i * 2 + 1] = 0;
    }

    Q6_bias_mxmem2_A(base + VTCM_BIAS_OFF);
    hmx_matmul_fp16((uintptr_t)act, 0, (uintptr_t)wei, 0);
    memset(out2, 0, 2048);
    Q6_mxmem_AR_after_hf(out2, 0);

    out16 = (uint16_t *)(out2 + crouton_off_sm(0, 0));
    r2 = *out16;

    check_nonzero(r1, "D3 FP round 1");
    check_nonzero(r2, "D3 FP round 2");
    check_ne(r1, r2, "D3 FP r1!=r2");
}

/* ================================================================== */
/* Category F: Seeded Random FXP                                       */
/*                                                                     */
/* Fill activation/weight with PRNG data; verify output is non-zero    */
/* and reproducible (same seed -> same output).                        */
/* ================================================================== */

static void run_seeded_fxp(uint8_t *base, matmul_fn_t matmul,
                            uint32_t seed, uint16_t exp,
                            uint8_t *out)
{
    uint8_t *act = base + VTCM_ACT_OFF;
    uint8_t *wei = base + VTCM_WEI_OFF;

    prng_seed(seed);
    fill_act_random(act);
    fill_wei_random(wei);
    fill_fxp_bias_random(base + VTCM_BIAS_OFF, exp);

    memset(out, 0, 2048);
    Q6_mxclracc();
    Q6_bias_mxmem2_A(base + VTCM_BIAS_OFF);
    matmul((uintptr_t)act, 0, (uintptr_t)wei, 0);
    Q6_mxmem_AR_after_sat_ub(out, 0);
}

static void test_seeded_weight_types(uint8_t *base)
{
    uint8_t *out = base + VTCM_OUT_OFF;
    uint8_t *scratch = base + VTCM_SCRATCH_OFF;
    uint8_t saved_byte[2048];
    int any_nonzero;
    int s, o;

    puts("F: seeded FXP weight types");

    /* F1: byte */
    run_seeded_fxp(base, hmx_matmul_byte, 0xDEAD0001, 20, out);
    any_nonzero = 0;
    for (s = 0; s < 4; s++) {
        for (o = 0; o < 8; o++) {
            if (out[crouton_off_sm(s, o)] != 0) {
                any_nonzero = 1;
            }
        }
    }
    check_nonzero(any_nonzero, "F1 byte nonzero");
    memcpy(saved_byte, out, 2048);

    /* F1 reproducibility: same seed -> same output */
    run_seeded_fxp(base, hmx_matmul_byte, 0xDEAD0001, 20, out);
    check_mem_eq(saved_byte, out, 2048, "F1 byte reproducible");

    /* F1 different seed -> different output */
    run_seeded_fxp(base, hmx_matmul_byte, 0xDEAD0099, 20, out);
    check_mem_ne(saved_byte, out, 2048, "F1 byte diff seed");

    /* F2: SM */
    run_seeded_fxp(base, hmx_matmul_sm, 0xDEAD0002, 20, out);
    check_nonzero(out[crouton_off_sm(0, 0)] |
                  out[crouton_off_sm(1, 0)], "F2 SM nonzero");

    /* F3: nibble */
    run_seeded_fxp(base, hmx_matmul_nibble, 0xDEAD0003, 20, out);
    memcpy(scratch, out, 2048);
    run_seeded_fxp(base, hmx_matmul_nibble, 0xDEAD0003, 20, out);
    check_mem_eq(scratch, out, 2048, "F3 nibble reproducible");

    /* F4: crumb */
    run_seeded_fxp(base, hmx_matmul_crumb, 0xDEAD0004, 24, out);
    check_nonzero(out[crouton_off_sm(0, 0)] |
                  out[crouton_off_sm(1, 0)], "F4 crumb nonzero");

    /* F5: scrumb (tiny weight range; scan full buffer for nonzero) */
    run_seeded_fxp(base, hmx_matmul_scrumb, 0xDEAD0005, 24, out);
    check_nonzero(any_byte_nonzero(out, 2048), "F5 scrumb nonzero");

    /* F6: ubit (tiny weight range; scan full buffer for nonzero) */
    run_seeded_fxp(base, hmx_matmul_ubit, 0xDEAD0006, 24, out);
    check_nonzero(any_byte_nonzero(out, 2048), "F6 ubit nonzero");

    /* F7: sbit */
    run_seeded_fxp(base, hmx_matmul_sbit, 0xDEAD0007, 24, out);
    check_nonzero(out[crouton_off_sm(0, 0)] |
                  out[crouton_off_sm(1, 0)], "F7 sbit nonzero");

    /* F8: seeded FP16 */
    puts("F8: seeded FP16 matmul");
    {
        uint8_t *act = base + VTCM_ACT_OFF;
        uint8_t *wei = base + VTCM_WEI_OFF;
        uint16_t *wei16 = (uint16_t *)wei;
        uint16_t *act16;
        uint16_t *out16;
        int c, i;

        prng_seed(0xCAFE0008);
        memset(act, 0, 2048);
        for (s = 0; s < 32; s++) {
            for (c = 0; c < 2; c++) {
                act16 = (uint16_t *)(act + crouton_off_sm(s * 2, c));
                *act16 = prng_fp16_small();
            }
        }
        for (i = 0; i < 32; i++) {
            wei16[i * 2] = prng_fp16_small();
            wei16[i * 2 + 1] = prng_fp16_small();
        }
        setup_fp_bias_unit(base + VTCM_BIAS_OFF);
        run_fp_matmul(base, hmx_matmul_fp16,
                      (uintptr_t)act, 0, (uintptr_t)wei, 0, out);

        /* Verify some FP16 output is non-zero */
        any_nonzero = 0;
        for (s = 0; s < 4; s++) {
            out16 = (uint16_t *)(out + crouton_off_sm(s * 2, 0));
            if (*out16 != 0) {
                any_nonzero = 1;
            }
        }
        check_nonzero(any_nonzero, "F8 fp16 nonzero");
    }
}

/* ================================================================== */
/* Category H: Multi-Vector FXP Byte                                   */
/*                                                                     */
/* Test with multiple weight vectors (multi-channel depth).            */
/* Deeper input should produce different (typically larger) acc.       */
/* ================================================================== */

static void run_mv_fxp_byte(uint8_t *base, int input_depth,
                              uint32_t seed, uint8_t *out)
{
    uint8_t *act = base + VTCM_ACT_OFF;
    uint8_t *wei = base + VTCM_WEI_OFF;
    int vecs = input_depth / 4;
    uint32_t wei_range = (vecs - 1) << 7;
    uint32_t act_range = gen_act_range_sm(input_depth);

    prng_seed(seed);
    fill_act_random(act);
    fill_wei_multi_random(wei, vecs);
    fill_fxp_bias_random(base + VTCM_BIAS_OFF, 20);

    memset(out, 0, 2048);
    Q6_mxclracc();
    Q6_bias_mxmem2_A(base + VTCM_BIAS_OFF);
    hmx_matmul_byte((uintptr_t)act, act_range,
                    (uintptr_t)wei, wei_range);
    Q6_mxmem_AR_after_sat_ub(out, 0);
}

static void test_multivec_fxp(uint8_t *base)
{
    uint8_t *out = base + VTCM_OUT_OFF;
    uint8_t *scratch = base + VTCM_SCRATCH_OFF;
    int any_nonzero, s, o;

    puts("H: multi-vector FXP byte");

    /* H1: depth=8, 2 weight vectors */
    run_mv_fxp_byte(base, 8, 0xBEEF0001, out);
    any_nonzero = 0;
    for (s = 0; s < 4; s++) {
        for (o = 0; o < 8; o++) {
            if (out[crouton_off_sm(s, o)] != 0) {
                any_nonzero = 1;
            }
        }
    }
    check_nonzero(any_nonzero, "H1 d8 nonzero");

    /* Reproducibility */
    memcpy(scratch, out, 2048);
    run_mv_fxp_byte(base, 8, 0xBEEF0001, out);
    check_mem_eq(scratch, out, 2048, "H1 d8 reproducible");

    /* H2: depth=16 */
    run_mv_fxp_byte(base, 16, 0xBEEF0011, out);
    any_nonzero = 0;
    for (s = 0; s < 4; s++) {
        for (o = 0; o < 8; o++) {
            if (out[crouton_off_sm(s, o)] != 0) {
                any_nonzero = 1;
            }
        }
    }
    check_nonzero(any_nonzero, "H2 d16 nonzero");

    /* H3: depth=32 */
    run_mv_fxp_byte(base, 32, 0xBEEF0021, out);
    any_nonzero = 0;
    for (s = 0; s < 4; s++) {
        for (o = 0; o < 8; o++) {
            if (out[crouton_off_sm(s, o)] != 0) {
                any_nonzero = 1;
            }
        }
    }
    check_nonzero(any_nonzero, "H3 d32 nonzero");
}

/* ================================================================== */
/* Category I: Multi-Vector FP16                                       */
/* ================================================================== */

static void test_multivec_fp16(uint8_t *base)
{
    uint8_t *act = base + VTCM_ACT_OFF;
    uint8_t *wei = base + VTCM_WEI_OFF;
    uint8_t *out = base + VTCM_OUT_OFF;
    uint8_t *scratch = base + VTCM_SCRATCH_OFF;
    uint16_t *out16;
    int input_depth, vecs;
    uint32_t wei_range, act_range;
    int any_nonzero, s;

    puts("I: multi-vector FP16");

    /* I1: depth=4 */
    input_depth = 4;
    vecs = input_depth / 2;
    wei_range = (vecs - 1) << 7;
    act_range = gen_act_range_sm(input_depth);

    prng_seed(0xCAFE0001);
    fill_fp16_act_random_depth(act, input_depth);
    fill_fp16_wei_multi_random(wei, vecs);
    setup_fp_bias_unit(base + VTCM_BIAS_OFF);

    memset(out, 0, 2048);
    Q6_mxclracc_hf();
    Q6_bias_mxmem2_A(base + VTCM_BIAS_OFF);
    hmx_matmul_fp16((uintptr_t)act, act_range,
                    (uintptr_t)wei, wei_range);
    Q6_mxmem_AR_after_hf(out, 0);

    any_nonzero = 0;
    for (s = 0; s < 4; s++) {
        out16 = (uint16_t *)(out + crouton_off_sm(s * 2, 0));
        if (*out16 != 0) {
            any_nonzero = 1;
        }
    }
    check_nonzero(any_nonzero, "I1 d4 nonzero");

    /* Reproducibility */
    memcpy(scratch, out, 2048);
    prng_seed(0xCAFE0001);
    fill_fp16_act_random_depth(act, input_depth);
    fill_fp16_wei_multi_random(wei, vecs);
    setup_fp_bias_unit(base + VTCM_BIAS_OFF);

    memset(out, 0, 2048);
    Q6_mxclracc_hf();
    Q6_bias_mxmem2_A(base + VTCM_BIAS_OFF);
    hmx_matmul_fp16((uintptr_t)act, act_range,
                    (uintptr_t)wei, wei_range);
    Q6_mxmem_AR_after_hf(out, 0);

    check_mem_eq(scratch, out, 2048, "I1 d4 reproducible");

    /* I2: depth=8 */
    input_depth = 8;
    vecs = input_depth / 2;
    wei_range = (vecs - 1) << 7;
    act_range = gen_act_range_sm(input_depth);

    prng_seed(0xCAFE0011);
    fill_fp16_act_random_depth(act, input_depth);
    fill_fp16_wei_multi_random(wei, vecs);
    setup_fp_bias_unit(base + VTCM_BIAS_OFF);

    memset(out, 0, 2048);
    Q6_mxclracc_hf();
    Q6_bias_mxmem2_A(base + VTCM_BIAS_OFF);
    hmx_matmul_fp16((uintptr_t)act, act_range,
                    (uintptr_t)wei, wei_range);
    Q6_mxmem_AR_after_hf(out, 0);

    any_nonzero = 0;
    for (s = 0; s < 4; s++) {
        out16 = (uint16_t *)(out + crouton_off_sm(s * 2, 0));
        if (*out16 != 0) {
            any_nonzero = 1;
        }
    }
    check_nonzero(any_nonzero, "I2 d8 nonzero");
}

/* ================================================================== */
/* Category M: Weight Modifiers                                        */
/*                                                                     */
/* :single, :after, :drop, :dilate change x-tap behavior.             */
/* Each modifier should produce different output from normal mode.     */
/* ================================================================== */

static void run_wei_modifier(uint8_t *base, matmul_fn_t matmul,
                              uint32_t seed, uint8_t *out)
{
    uint8_t *act = base + VTCM_ACT_OFF;
    uint8_t *wei = base + VTCM_WEI_OFF;
    int total_vecs = 3;
    uint32_t wei_range = (total_vecs - 1) << 7;
    uint32_t act_range = gen_act_range_sm(4);

    prng_seed(seed);
    fill_act_random(act);
    fill_wei_multi_random(wei, total_vecs);
    fill_fxp_bias_random(base + VTCM_BIAS_OFF, 20);

    memset(out, 0, 2048);
    Q6_mxclracc();
    Q6_bias_mxmem2_A(base + VTCM_BIAS_OFF);
    /* Rs[1:0]=2 -> fx=2 -> 3 x-taps */
    matmul((uintptr_t)act | 2, act_range,
           (uintptr_t)wei, wei_range);
    Q6_mxmem_AR_after_sat_ub(out, 0);
}

static void test_weight_modifiers(uint8_t *base)
{
    uint8_t *out = base + VTCM_OUT_OFF;
    uint8_t normal[2048];
    uint8_t single_out[2048];

    puts("M: weight modifiers");

    /* M0: normal (scan full buffer; multi-tap may scatter output) */
    run_wei_modifier(base, hmx_matmul_byte, 0xABCD0001, out);
    memcpy(normal, out, 2048);
    check_nonzero(any_byte_nonzero(out, 2048), "M0 normal nonzero");

    /* Reproducibility */
    run_wei_modifier(base, hmx_matmul_byte, 0xABCD0001, out);
    check_mem_eq(normal, out, 2048, "M0 normal reproducible");

    /* M1: single */
    run_wei_modifier(base, hmx_matmul_byte_single, 0xABCD0001, out);
    memcpy(single_out, out, 2048);
    check_nonzero(any_byte_nonzero(out, 2048), "M1 single nonzero");
    check_mem_ne(normal, single_out, 2048, "M1 single!=normal");

    /* M2: after */
    run_wei_modifier(base, hmx_matmul_byte_after, 0xABCD0001, out);
    check_nonzero(any_byte_nonzero(out, 2048), "M2 after nonzero");
    check_mem_ne(normal, out, 2048, "M2 after!=normal");

    /*
     * M3: drop (only suppresses overflow routing to secondary acc;
     * primary output should match normal)
     */
    run_wei_modifier(base, hmx_matmul_byte_drop, 0xABCD0001, out);
    check_mem_eq(normal, out, 2048, "M3 drop primary==normal");

    /* M4: dilate */
    run_wei_modifier(base, hmx_matmul_byte_dilate, 0xABCD0001, out);
    check_mem_ne(normal, out, 2048, "M4 dilate!=normal");
}

/* ================================================================== */
/* Category N: Activation Modifiers                                    */
/* ================================================================== */

static void run_act_modifier(uint8_t *base, matmul_fn_t matmul,
                              uint32_t seed, uint8_t *out)
{
    uint8_t *act = base + VTCM_ACT_OFF;
    uint8_t *wei = base + VTCM_WEI_OFF;
    int total_vecs = 3;
    uint32_t wei_range = (total_vecs - 1) << 7;
    uint32_t act_range = gen_act_range_sm(4);

    prng_seed(seed);
    fill_act_random(act);
    fill_wei_multi_random(wei, total_vecs);
    fill_fxp_bias_random(base + VTCM_BIAS_OFF, 20);

    memset(out, 0, 2048);
    Q6_mxclracc();
    Q6_bias_mxmem2_A(base + VTCM_BIAS_OFF);
    /* Rs = act | 0x202: fy=0x200, fx=2 for 3x3 */
    matmul((uintptr_t)act | 0x202, act_range,
           (uintptr_t)wei, wei_range);
    Q6_mxmem_AR_after_sat_ub(out, 0);
}

static void test_activation_modifiers(uint8_t *base)
{
    uint8_t *out = base + VTCM_OUT_OFF;
    uint8_t normal[2048];

    puts("N: activation modifiers");

    /* N0: normal */
    run_act_modifier(base, hmx_matmul_byte, 0xBCDE0001, out);
    memcpy(normal, out, 2048);
    check_nonzero(out[crouton_off_sm(0, 0)] |
                  out[crouton_off_sm(1, 0)], "N0 normal nonzero");

    /* N1: single */
    run_act_modifier(base, hmx_matmul_byte_act_single,
                     0xBCDE0001, out);
    check_mem_ne(normal, out, 2048, "N1 single!=normal");

    /* N2: above */
    run_act_modifier(base, hmx_matmul_byte_act_above,
                     0xBCDE0001, out);
    check_mem_ne(normal, out, 2048, "N2 above!=normal");
}

/* ================================================================== */
/* Category O: FP Bias Extended Fields                                 */
/*                                                                     */
/* FP16 matmul giving acc=2.0, then vary bias parameters.              */
/* Each parameter change should produce different output.              */
/* ================================================================== */

static void setup_fp_acc_2(uint8_t *base)
{
    uint8_t *act = base + VTCM_ACT_OFF;
    uint8_t *wei = base + VTCM_WEI_OFF;
    uint16_t *act16;
    uint16_t *wei16 = (uint16_t *)wei;
    int i;

    memset(act, 0, 2048);
    memset(wei, 0, 128);

    act16 = (uint16_t *)(act + crouton_off_sm(0, 0));
    *act16 = 0x4000;  /* 2.0 */

    for (i = 0; i < 32; i++) {
        wei16[i * 2] = 0x3C00;      /* 1.0 */
        wei16[i * 2 + 1] = 0;
    }
}

static uint16_t run_fp_bias_test(uint8_t *base, uint64_t bias_val)
{
    uint8_t *out = base + VTCM_OUT_OFF;
    uint64_t bias_vals[32];
    uint16_t *out16;
    int i;

    setup_fp_acc_2(base);

    for (i = 0; i < 32; i++) {
        bias_vals[i] = bias_val;
    }
    write_bias_mxmem2(base + VTCM_BIAS_OFF, bias_vals, 32);

    Q6_mxclracc_hf();
    Q6_bias_mxmem2_A(base + VTCM_BIAS_OFF);
    hmx_matmul_fp16((uintptr_t)(base + VTCM_ACT_OFF), 0,
                    (uintptr_t)(base + VTCM_WEI_OFF), 0);
    memset(out, 0, 2048);
    Q6_mxmem_AR_after_hf(out, 0);

    out16 = (uint16_t *)(out + crouton_off_sm(0, 0));
    return *out16;
}

static void test_fp_bias_extended(uint8_t *base)
{
    uint16_t baseline, with_abias, with_scale2, with_obias;
    uint16_t with_negate, with_negabs;

    puts("O: FP bias extended");

    /* Baseline: scale=1.0, no bias, shape=0 -> output=2.0 */
    baseline = run_fp_bias_test(base,
        pack_fp_bias(0x3C00, 0, 0, 0, 0, 0, 0, 0));
    check_nonzero(baseline, "O baseline");

    /* O1: acc_bias = 2.0 -> output = 2.0 + 2.0 = 4.0 */
    with_abias = run_fp_bias_test(base,
        pack_fp_bias(0x3C00, 0, 0, 0, 0, 0, 0, 0x4000));
    check_ne(baseline, with_abias, "O1 acc_bias effect");

    /* O3: scale = 2.0 -> output = 2.0 * 2.0 = 4.0 */
    with_scale2 = run_fp_bias_test(base,
        pack_fp_bias(0x4000, 0, 0, 0, 0, 0, 0, 0));
    check_ne(baseline, with_scale2, "O3 scale effect");

    /* O5: out_bias = 1.0 -> output = 2.0 + 1.0 = 3.0 */
    with_obias = run_fp_bias_test(base,
        pack_fp_bias(0x3C00, 0x3C00, 0, 0, 0, 0, 0, 0));
    check_ne(baseline, with_obias, "O5 out_bias effect");

    /* O6: negate=1, shape=0 -> output = -(2.0) = -2.0 */
    with_negate = run_fp_bias_test(base,
        pack_fp_bias(0x3C00, 0, 0, 0, 0, 1, 0, 0));
    check_ne(baseline, with_negate, "O6 negate effect");
    /* Negate should flip sign bit */
    if (!((with_negate >> 15) & 1)) {
        printf("ERROR: O6 negate should be negative\n");
        err++;
    }

    /*
     * O7: negate=1, shape=3 (abs) -> -(|2.0|) = -2.0
     * abs_negate = (shape==3 && acc<0)
     * scale_neg = negate ^ abs_negate
     * With positive acc: scale_neg = 1^0 = 1 -> output is negative
     */
    with_negabs = run_fp_bias_test(base,
        pack_fp_bias(0x3C00, 0, 0, 0, 3, 1, 0, 0));
    /* negate + abs on positive acc -> always negative */
    if (!((with_negabs >> 15) & 1)) {
        printf("ERROR: O7 negabs should be negative\n");
        err++;
    }
}

/* ================================================================== */
/* Category Q: Non-Legacy Convert                                      */
/*                                                                     */
/* Two-step convert: cvt.ub=acc(Rs) then mxmem=cvt.                   */
/* Rs bits: [0]=retain (skip acc clear), [1]=relu.                     */
/* ================================================================== */

static void test_nonlegacy_convert(uint8_t *base)
{
    uint8_t *act = base + VTCM_ACT_OFF;
    uint8_t *wei = base + VTCM_WEI_OFF;
    uint8_t *out = base + VTCM_OUT_OFF;
    uint8_t *scratch = base + VTCM_SCRATCH_OFF;
    uint32_t *wei32 = (uint32_t *)wei;
    uint32_t rs;
    uint32_t val_basic, val_relu, val_retain1, val_retain2;
    int i;

    puts("Q: non-legacy convert");

    /* Setup: act[s0]=100, weight=0x50 (80), acc=8000 */
    memset(act, 0, 2048);
    act[crouton_off_sm(0, 0)] = 100;
    act[crouton_off_sm(1, 0)] = 200;

    memset(wei, 0, 128);
    for (i = 0; i < 32; i++) {
        wei32[i] = 0x00000050;
    }
    setup_fxp_bias(base + VTCM_BIAS_OFF, fxp_passthru_bias());

    /* Q1: Rs=0 (clear, no relu) */
    Q6_mxclracc();
    Q6_bias_mxmem2_A(base + VTCM_BIAS_OFF);
    hmx_matmul_byte((uintptr_t)act, 0, (uintptr_t)wei, 0);
    rs = 0;
    Q6_cvt_ub_acc_R(rs);
    memset(out, 0, 2048);
    Q6_mxmem_cvt_RR(out, 0);
    val_basic = out[crouton_off_sm(0, 0)];
    check_nonzero(val_basic, "Q1 basic nonzero");

    /*
     * Q2: Relu via bias shape=2 -- make acc negative, relu clamps to 0.
     * Shape=2 in the FXP bias applies max(0, x) after shift.
     * Use negative input_bias to force accumulator negative regardless
     * of weight signedness (v71+ treats byte weights as unsigned).
     */
    memset(wei, 0, 128);
    for (i = 0; i < 32; i++) {
        wei32[i] = 0x00000050;  /* positive weight */
    }
    setup_fxp_bias(base + VTCM_BIAS_OFF,
                   pack_fxp_bias(-100000, 20, 2, 0x080, 0xFF0));
    Q6_mxclracc();
    Q6_bias_mxmem2_A(base + VTCM_BIAS_OFF);
    hmx_matmul_byte((uintptr_t)act, 0, (uintptr_t)wei, 0);
    rs = 0;
    Q6_cvt_ub_acc_R(rs);
    memset(out, 0, 2048);
    Q6_mxmem_cvt_RR(out, 0);
    val_relu = out[crouton_off_sm(0, 0)];
    check_zero(val_relu, "Q2 relu neg->0");

    /* Q3: Rs=0x1 (retain) -- acc NOT cleared, double-accumulate */
    memset(wei, 0, 128);
    for (i = 0; i < 32; i++) {
        wei32[i] = 0x00000050;
    }
    setup_fxp_bias(base + VTCM_BIAS_OFF, fxp_passthru_bias());
    Q6_mxclracc();
    Q6_bias_mxmem2_A(base + VTCM_BIAS_OFF);
    hmx_matmul_byte((uintptr_t)act, 0, (uintptr_t)wei, 0);

    /* Convert with retain -- acc stays */
    rs = 0x1;
    Q6_cvt_ub_acc_R(rs);
    memset(out, 0, 2048);
    Q6_mxmem_cvt_RR(out, 0);
    val_retain1 = out[crouton_off_sm(0, 0)];
    check_nonzero(val_retain1, "Q3 retain1");

    /* Second matmul on retained acc, then convert with clear */
    hmx_matmul_byte((uintptr_t)act, 0, (uintptr_t)wei, 0);
    rs = 0x0;
    Q6_cvt_ub_acc_R(rs);
    memset(scratch, 0, 2048);
    Q6_mxmem_cvt_RR(scratch, 0);
    val_retain2 = scratch[crouton_off_sm(0, 0)];
    check_nonzero(val_retain2, "Q3 retain2");

    /*
     * Retained + second matmul should produce different output
     * than a single matmul (typically larger due to accumulation).
     */
    check_ne(val_retain1, val_retain2, "Q3 retain1!=retain2");
}

/* ================================================================== */
/* Main                                                                */
/* ================================================================== */

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

    puts("HMX ISA Comprehensive Test");

    /* A: FXP weight types */
    test_all_weight_types(base);

    /* B: FXP convert pipeline */
    test_cvt_exponent(base);
    test_cvt_scale(base);
    test_cvt_input_bias(base);
    test_cvt_shapes(base);

    /* C: FP matmul */
    test_fp16_matmul(base);
    test_fp_f8_weight(base);
    test_fp_weight_negate(base);

    /* D: Accumulator management */
    test_acc_alternation(base);
    test_swapacc(base);
    test_acc_alternation_fp(base);

    /* F: Seeded random */
    test_seeded_weight_types(base);

    /* H: Multi-vector FXP byte */
    test_multivec_fxp(base);

    /* I: Multi-vector FP16 */
    test_multivec_fp16(base);

    /* M: Weight modifiers */
    test_weight_modifiers(base);

    /* N: Activation modifiers */
    test_activation_modifiers(base);

    /* O: FP bias extended */
    test_fp_bias_extended(base);

    /* Q: Non-legacy convert */
    test_nonlegacy_convert(base);

    puts(err ? "FAIL" : "PASS");
#if defined(__linux__)
    free(base);
#endif
    return err;
}
