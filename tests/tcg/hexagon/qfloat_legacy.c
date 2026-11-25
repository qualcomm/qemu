/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 * Test the QFloat arithmetic of the cores before v79: add, subtract and
 * multiply of IEEE and QF operands, including zeros, infinities and NaNs,
 * and the conversion of QF results back to IEEE.
 *
 * The expected values were produced by hexagon-sim for v73.  Each table of
 * results over all pairs of the input values is checked by its hash, and
 * the cases that were once wrong are also checked directly.
 *
 * Run with an argument to print the hashes.
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include <hexagon_types.h>
#include <hvx_hexagon_protos.h>

#define N 16

static const uint16_t hf_in[N] = {
    0x0000, 0x8000, 0x3c00, 0xbc00, 0x7bff, 0xfbff, 0x7c00, 0xfc00,
    0x7e00, 0xfe00, 0x7c01, 0x0001, 0x0400, 0x5640, 0x3555, 0x4248,
};
static const uint32_t sf_in[N] = {
    0x00000000, 0x80000000, 0x3f800000, 0xbf800000, 0x7f7fffff, 0xff7fffff,
    0x7f800000, 0xff800000, 0x7fc00000, 0xffc00000, 0x7f800001, 0x00000001,
    0x00800000, 0x40490fdb, 0x3eaaaaab, 0x42f6e979,
};
/* QF16 and QF32 encodings: zero, one, minus one, the extremes, small. */
static const uint16_t qf16_in[N] = {
    0x0000, 0xffe0, 0x4010, 0xbff0, 0x7ffe, 0x8000, 0x7fe1, 0x8021,
    0x0201, 0xfe3f, 0x3c1f, 0xc3e1, 0x1011, 0x0001, 0x7fff, 0x0020,
};
static const uint32_t qf32_in[N] = {
    0x00000000, 0xffffff00, 0x40000080, 0xbfffff80, 0x7ffffffe, 0x80000000,
    0x7fffff81, 0x80000181, 0x00000201, 0xfffe003f, 0x3c0001ff, 0xc3ffffe1,
    0x10000091, 0x00000001, 0x7fffffff, 0x00000020,
};

static int err;

static uint32_t fnv(uint32_t h, uint32_t x)
{
    return (h ^ x) * 16777619u;
}

static HVX_Vector splat16(uint16_t x)
{
    return Q6_Vh_vsplat_R(x | ((uint32_t)x << 16));
}

static HVX_Vector splat32(uint32_t x)
{
    return Q6_V_vsplat_R(x);
}

static uint32_t lane16(HVX_Vector v, int i)
{
    uint16_t out[64] __attribute__((aligned(128)));

    memcpy(out, &v, sizeof(v));
    return out[i];
}

static uint32_t lane32(HVX_Vector v, int i)
{
    uint32_t out[32] __attribute__((aligned(128)));

    memcpy(out, &v, sizeof(v));
    return out[i];
}

typedef HVX_Vector (*op16_fn)(HVX_Vector, HVX_Vector);
typedef HVX_Vector (*op32_fn)(HVX_Vector, HVX_Vector);

static HVX_Vector hf_add(HVX_Vector a, HVX_Vector b)
{
    return Q6_Vqf16_vadd_VhfVhf(a, b);
}
static HVX_Vector hf_sub(HVX_Vector a, HVX_Vector b)
{
    return Q6_Vqf16_vsub_VhfVhf(a, b);
}
static HVX_Vector hf_mpy(HVX_Vector a, HVX_Vector b)
{
    return Q6_Vqf16_vmpy_VhfVhf(a, b);
}
static HVX_Vector qf16_add(HVX_Vector a, HVX_Vector b)
{
    return Q6_Vqf16_vadd_Vqf16Vqf16(a, b);
}
static HVX_Vector qf16_mpy(HVX_Vector a, HVX_Vector b)
{
    return Q6_Vqf16_vmpy_Vqf16Vqf16(a, b);
}
static HVX_Vector qf16_hf_add(HVX_Vector a, HVX_Vector b)
{
    return Q6_Vqf16_vadd_Vqf16Vhf(a, b);
}
static HVX_Vector qf16_hf_sub(HVX_Vector a, HVX_Vector b)
{
    return Q6_Vqf16_vsub_Vqf16Vhf(a, b);
}
static HVX_Vector qf16_hf_mpy(HVX_Vector a, HVX_Vector b)
{
    return Q6_Vqf16_vmpy_Vqf16Vhf(a, b);
}
static HVX_Vector sf_add(HVX_Vector a, HVX_Vector b)
{
    return Q6_Vqf32_vadd_VsfVsf(a, b);
}
static HVX_Vector sf_sub(HVX_Vector a, HVX_Vector b)
{
    return Q6_Vqf32_vsub_VsfVsf(a, b);
}
static HVX_Vector sf_mpy(HVX_Vector a, HVX_Vector b)
{
    return Q6_Vqf32_vmpy_VsfVsf(a, b);
}
static HVX_Vector qf32_add(HVX_Vector a, HVX_Vector b)
{
    return Q6_Vqf32_vadd_Vqf32Vqf32(a, b);
}
static HVX_Vector qf32_mpy(HVX_Vector a, HVX_Vector b)
{
    return Q6_Vqf32_vmpy_Vqf32Vqf32(a, b);
}
static HVX_Vector qf32_sf_add(HVX_Vector a, HVX_Vector b)
{
    return Q6_Vqf32_vadd_Vqf32Vsf(a, b);
}
static HVX_Vector qf32_sf_sub(HVX_Vector a, HVX_Vector b)
{
    return Q6_Vqf32_vsub_Vqf32Vsf(a, b);
}
/* Widening multiplies: QF32 from 16 bit operands. */
static HVX_Vector whf_mpy(HVX_Vector a, HVX_Vector b)
{
    return Q6_V_lo_W(Q6_Wqf32_vmpy_VhfVhf(a, b));
}
static HVX_Vector wqf16_hf_mpy(HVX_Vector a, HVX_Vector b)
{
    return Q6_V_lo_W(Q6_Wqf32_vmpy_Vqf16Vhf(a, b));
}
static HVX_Vector wqf16_mpy(HVX_Vector a, HVX_Vector b)
{
    return Q6_V_lo_W(Q6_Wqf32_vmpy_Vqf16Vqf16(a, b));
}

/* Hash lane 0 of the result for every pair of the two tables. */
static uint32_t table16(op16_fn op, const uint16_t *a, const uint16_t *b)
{
    uint32_t h = 2166136261u;

    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            h = fnv(h, lane16(op(splat16(a[i]), splat16(b[j])), 0));
        }
    }
    return h;
}

static uint32_t table32(op32_fn op, const uint32_t *a, const uint32_t *b)
{
    uint32_t h = 2166136261u;

    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            h = fnv(h, lane32(op(splat32(a[i]), splat32(b[j])), 0));
        }
    }
    return h;
}

/* The widening multiplies take 16 bit operands, give 32 bit results. */
static uint32_t table_w(op16_fn op, const uint16_t *a, const uint16_t *b)
{
    uint32_t h = 2166136261u;

    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            h = fnv(h, lane32(op(splat16(a[i]), splat16(b[j])), 0));
        }
    }
    return h;
}

static uint32_t table_conv(void)
{
    uint32_t h = 2166136261u;

    for (int i = 0; i < N; i++) {
        h = fnv(h, lane16(Q6_Vhf_equals_Vqf16(splat16(qf16_in[i])), 0));
        h = fnv(h, lane32(Q6_Vsf_equals_Vqf32(splat32(qf32_in[i])), 0));
        for (int j = 0; j < N; j++) {
            HVX_VectorPair w = Q6_W_vcombine_VV(splat32(qf32_in[i]),
                                                splat32(qf32_in[j]));

            h = fnv(h, lane16(Q6_Vhf_equals_Wqf32(w), 0));
        }
    }
    return h;
}

#define CHECK_HASH(name, got, want) \
    do { \
        if (verbose) { \
            printf("    %-14s 0x%08x\n", name, (unsigned)(got)); \
        } else if ((got) != (want)) { \
            printf("ERROR %s: 0x%08x != 0x%08x\n", name, \
                   (unsigned)(got), (unsigned)(want)); \
            err++; \
        } \
    } while (0)

#define CHECK_VAL(what, got, want) \
    do { \
        if ((got) != (want)) { \
            printf("ERROR %s: 0x%x != 0x%x\n", what, (unsigned)(got), \
                   (unsigned)(want)); \
            err++; \
        } \
    } while (0)

int main(int argc, char **argv)
{
    int verbose = argc > 1;

    CHECK_HASH("hf+hf", table16(hf_add, hf_in, hf_in), 0x4d1feb8b);
    CHECK_HASH("hf-hf", table16(hf_sub, hf_in, hf_in), 0x5d4f01c7);
    CHECK_HASH("hf*hf", table16(hf_mpy, hf_in, hf_in), 0x66403f8d);
    CHECK_HASH("qf16+qf16", table16(qf16_add, qf16_in, qf16_in), 0x531e5755);
    CHECK_HASH("qf16*qf16", table16(qf16_mpy, qf16_in, qf16_in), 0x52933120);
    CHECK_HASH("qf16+hf", table16(qf16_hf_add, qf16_in, hf_in), 0xedc9a9d1);
    CHECK_HASH("qf16-hf", table16(qf16_hf_sub, qf16_in, hf_in), 0xd62a0086);
    CHECK_HASH("qf16*hf", table16(qf16_hf_mpy, qf16_in, hf_in), 0xad3b816a);
    CHECK_HASH("sf+sf", table32(sf_add, sf_in, sf_in), 0xb5b30301);
    CHECK_HASH("sf-sf", table32(sf_sub, sf_in, sf_in), 0x6968ff17);
    CHECK_HASH("sf*sf", table32(sf_mpy, sf_in, sf_in), 0x0a13d3b0);
    CHECK_HASH("qf32+qf32", table32(qf32_add, qf32_in, qf32_in), 0xca64c424);
    CHECK_HASH("qf32*qf32", table32(qf32_mpy, qf32_in, qf32_in), 0x89d9cc04);
    CHECK_HASH("qf32+sf", table32(qf32_sf_add, qf32_in, sf_in), 0x6539e7d7);
    CHECK_HASH("qf32-sf", table32(qf32_sf_sub, qf32_in, sf_in), 0xe6b38ba0);
    CHECK_HASH("w=hf*hf", table_w(whf_mpy, hf_in, hf_in), 0x165365f1);
    CHECK_HASH("w=qf16*hf", table_w(wqf16_hf_mpy, qf16_in, hf_in), 0x9e4cc638);
    CHECK_HASH("w=qf16*qf16", table_w(wqf16_mpy, qf16_in, qf16_in), 0x35a66934);
    CHECK_HASH("convert", table_conv(), 0x570ef678);

    /* Infinity and NaN operands saturate: they are not special values. */
    CHECK_VAL("0 + inf",
              lane16(hf_add(splat16(0x0000), splat16(0x7c00)), 0), 0x401f);
    CHECK_VAL("0 + -inf",
              lane16(hf_add(splat16(0x0000), splat16(0xfc00)), 0), 0xc01f);
    CHECK_VAL("-1 + -inf",
              lane16(hf_add(splat16(0xbc00), splat16(0xfc00)), 0), 0xbfff);
    CHECK_VAL("-1.0 * -inf",
              lane32(sf_mpy(splat32(0xbf800000), splat32(0xff800000)), 0),
              0x400000ff);
    /* A negated zero has the all-ones significand. */
    CHECK_VAL("0 * -1",
              lane16(hf_mpy(splat16(0x0000), splat16(0xbc00)), 0), 0xffe0);
    CHECK_VAL("-0 + -0",
              lane32(sf_add(splat32(0x80000000), splat32(0x80000000)), 0),
              0xffffff00);
    /* IEEE signs negate the rounded magnitude, as a one's complement. */
    CHECK_VAL("-1 * 1",
              lane16(hf_mpy(splat16(0xbc00), splat16(0x3c00)), 0), 0xbfef);
    CHECK_VAL("-1 + -1",
              lane16(hf_add(splat16(0xbc00), splat16(0xbc00)), 0), 0xbff0);

    if (err) {
        printf("FAIL\n");
        return 1;
    }
    puts(verbose ? "" : "PASS");
    return 0;
}
