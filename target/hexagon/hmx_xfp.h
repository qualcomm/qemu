/*
 * Hexagon HMX (Matrix eXtensions) XFP arithmetic
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Hexagon HMX XFP arithmetic: generic width-carrying HexagonXfp
 * primitives (FP convert path) plus flat v81-fixed HmxXfp
 * primitives (v81 FP MAC path).
 */

#ifndef HEXAGON_HMX_XFP_H
#define HEXAGON_HMX_XFP_H

struct HmxConfig;

/* xfp status type */
typedef union HexagonXfpStatus {
    struct {
        uint32_t zero:1;
        uint32_t inf:2;        /* 0 finite, 1 +Inf, 2 -Inf, 3 NaN */
        uint32_t negative:1;
        uint32_t under:1;
        uint32_t in0_zero:1;   /* product-side all-zero detection */
        uint32_t in1_zero:1;
        uint32_t reserved:25;
    };
    uint32_t val;
} HexagonXfpStatus;

/* xfp data type */
typedef struct HexagonXfp {
    HexagonXfpStatus status;
    int32_t exp;        /* unbiased exponent */
    int64_t sig;        /* signed significand, Q INT.FRAC */
    uint8_t bits_int;   /* INT width */
    uint8_t bits_frac;  /* FRAC width */
    uint8_t bits_exp;   /* EXP width */
    uint8_t lza;
} HexagonXfp;

/*
 * Flat XFP operand/accumulator cell. Same numeric meaning as
 * HexagonXfp (sig is a signed Q<bits_int>.<bits_frac> significand,
 * exp is the unbiased exponent, status matches HexagonXfpStatus's bit
 * layout exactly so hmx_xfp_to_xfp() is lossless) -- but the
 * int/frac/exp bit-width fields are dropped because every function
 * that uses this type is specialized for exactly one fixed shape at
 * each pipeline stage (v81's known constants: operand int=3 frac=9
 * exp=8; product int=5 frac=18 exp=9; accumulator int=8 frac=22
 * exp=9), so there is nothing to carry at runtime.
 */
typedef struct HmxXfp {
    int64_t sig;
    int32_t exp;
    HexagonXfpStatus status;
} HmxXfp;

/*
 * USR FP control bits.
 * inf_nan_enable = USR[20], nan_propagate = USR[21].
 */
typedef struct HexagonXfpUsr {
    uint32_t inf_nan_enable;
    uint32_t nan_propagate;
} HexagonXfpUsr;

/* cvt Rs fields consumed by the XFP-to-FP convert path. */
typedef struct HexagonXfpCvtRs {
    uint32_t relu;
    uint32_t fp_rnd;
    uint32_t fp_maxnorm;
    uint32_t fb_dst;       /* 0=none, 1=outbias, 2=scale */
    uint32_t fb_limit;
    uint32_t is_bf16;
} HexagonXfpCvtRs;

/* Convert destination type */
#define HEXAGON_XFP_FB_OUTBIAS 1
#define HEXAGON_XFP_FB_SCALE   2

/* Decoded FP bias-register fields. */
typedef struct HexagonXfpBias {
    uint32_t scale;
    uint32_t out_bias;
    uint32_t scale_extra;
    uint32_t out_bias_extra;
    uint32_t shape;
    uint32_t negate;
    uint32_t acc_bias_extra;
    uint32_t acc_bias;
} HexagonXfpBias;

#define HEXAGON_XFP_MIN 0
#define HEXAGON_XFP_MAX 1

/* True-zero cell. */
void hexagon_xfp_zero(const struct HmxConfig *hmx_cfg, HexagonXfp *out);

/* fp -> xfp. */
HexagonXfp hexagon_xfp_from_fp(const struct HmxConfig *hmx_cfg,
                               HexagonXfpUsr usr, uint32_t in,
                               uint32_t frac_in, uint32_t exp_in,
                               uint32_t int_out, uint32_t frac_out,
                               uint32_t exp_out, uint32_t normalize);

/* xfp -> fp output. */
uint32_t hexagon_xfp_to_fp(const struct HmxConfig *hmx_cfg, int is_f8,
                           HexagonXfpUsr usr, HexagonXfp in,
                           uint32_t fp_frac, uint32_t fp_exp,
                           HexagonXfpCvtRs rs);

/* convert-normalize. */
HexagonXfp hexagon_xfp_cvt_normalize(const struct HmxConfig *hmx_cfg,
                                     HexagonXfpUsr usr, HexagonXfp in,
                                     uint32_t int_out, uint32_t frac_out,
                                     int32_t exp_out);

/* add. */
HexagonXfp hexagon_xfp_add(const struct HmxConfig *hmx_cfg,
                           HexagonXfpUsr usr, HexagonXfp in_a, HexagonXfp in_b);

/* multiply. */
HexagonXfp hexagon_xfp_mult(const struct HmxConfig *hmx_cfg,
                            HexagonXfpUsr usr, HexagonXfp in_a,
                            HexagonXfp in_b, uint32_t exp_out);

/* compare. */
HexagonXfp hexagon_xfp_cmp(const struct HmxConfig *hmx_cfg,
                           HexagonXfpUsr usr, HexagonXfp a, HexagonXfp b,
                           int32_t min_max);

/* 20-bit convert result split/combine. */
static inline uint16_t hexagon_xfp_cvt_out_lo(uint32_t in)
{
    return (in >> 0) & 0xFFF;
}
static inline uint16_t hexagon_xfp_cvt_out_hi(uint32_t in)
{
    return (in >> 8) & 0xFF0;
}
static inline uint32_t hexagon_xfp_cvt_combine_feedback(uint16_t hi,
                                                        uint16_t lo)
{
    uint32_t result = (uint32_t)(hi & 0xFF0) << 8;
    return result | ((uint32_t)lo & 0xFFF);
}

/*
 * v81 flat XFP MAC path bit widths, fixed at compile time:
 *   operand (post-decode):  int=3  frac=9  exp=8
 *   product (post-mult):    int=5  frac=18 exp=9
 *   accumulator:            int=8  frac=22 exp=9  (mx_fp_acc_int/frac/exp)
 *   normalize shift cap:    3                      (mx_fp_acc_norm)
 */

/* FP16 -> flat operand (int3.frac9 exp8). */
HmxXfp hmx_xfp_decode_fp16(HexagonXfpUsr usr, uint16_t in);

/* BF16 (1-8-7) -> flat operand. */
HmxXfp hmx_xfp_decode_bf16(HexagonXfpUsr usr, uint16_t in);

/* Exact product of two decoded operands, matching
 * hexagon_xfp_mult(hmx_cfg, usr, a, b, exp_out=9) bit-for-bit
 * (int=5 frac=18 exp=9 shape). */
HmxXfp hmx_xfp_mult(HexagonXfpUsr usr, HmxXfp a, HmxXfp b);

/*
 * Raw bit masks for HexagonXfpStatus's fields, used throughout this
 * path to build a whole status word with plain integer ops and store
 * it once via status.val instead of writing bitfields one at a time.
 *
 * xfp_status_layout_assert() in hmx_xfp.c pins these masks to
 * the actual bitfield layout at compile time: reorder or resize any
 * field in HexagonXfpStatus and the build fails there rather than
 * silently computing wrong status words.
 */
#define XFP_ST_ZERO        0x01u
#define XFP_ST_INF         0x06u
#define XFP_ST_INF_SHIFT   1
#define XFP_ST_NEG         0x08u
#define XFP_ST_UNDER       0x10u
#define XFP_ST_IN0_ZERO    0x20u
#define XFP_ST_IN1_ZERO    0x40u

/*
 * Shortcut for hmx_xfp_mult(usr, a, b) when b is a decoded zero.
 *
 * PRECONDITION (caller's responsibility, not checked here):
 * b.status.zero == 1 && a.status.inf == 0. Get either wrong and this
 * silently returns the wrong product -- call hmx_xfp_mult()
 * itself if there is any doubt.
 */
static inline HmxXfp hmx_xfp_mult_zero_weight(HmxXfp a)
{
    const uint32_t sa = a.status.val;
    const uint32_t in0_zero = ((a.sig == 0) || (sa & XFP_ST_ZERO))
                               ? XFP_ST_IN0_ZERO : 0;

    HmxXfp out;
    out.sig = 0;
    out.exp = -256;
    out.status.val = XFP_ST_ZERO | XFP_ST_IN1_ZERO
                    | (sa & XFP_ST_NEG) | in0_zero;
    return out;
}

/*
 * True-zero cell. All-zero memory is NOT this value (status.zero=1,
 * exp=-256).
 */
static inline HmxXfp hmx_xfp_zero(void)
{
    HmxXfp z = {0};
    z.exp = -256;
    z.status.zero = 1;
    return z;
}

/*
 * Rate-8 reduce: 8 flat products + accumulator -> accumulator, with
 * the all-zero skip and the deliberate dead-ovf behaviour (see
 * hmx_xfp.c).
 */
HmxXfp hmx_xfp_batch8(const struct HmxConfig *hmx_cfg,
                                HexagonXfpUsr usr, HmxXfp *products,
                                HmxXfp acc);

/* Widen a flat cell to HexagonXfp at the accumulator/convert
 * boundary. */
HexagonXfp hmx_xfp_to_xfp(HmxXfp in, uint8_t bits_int,
                                uint8_t bits_frac, uint8_t bits_exp);

#endif /* HEXAGON_HMX_XFP_H */
