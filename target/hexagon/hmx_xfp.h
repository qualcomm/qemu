/*
 * Hexagon HMX (Matrix eXtensions) XFP arithmetic
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Integer-only model of the hardware's internal XFP floating-
 * point accumulator format, used for the HMX FP MAC and convert path.
 *
 * It models the hardware XFP MAC and convert arithmetic operations
 * and defines the associated helper types.
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

/*
 * Full FP16/BF16/F8 convert (hmx_xfp_cvt / hmx_xfp_fp_cvt,
 * Returns the 20-bit (FP16/BF16) or 8-bit
 * (F8) convert result.
 */
uint32_t hexagon_xfp_convert(const struct HmxConfig *hmx_cfg, int is_f8,
                             HexagonXfpUsr usr, HexagonXfp acc,
                             HexagonXfpBias bias, uint32_t cvt_feedback,
                             HexagonXfpCvtRs rs);

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

/* Widen a flat cell to HexagonXfp at the accumulator/convert
 * boundary. */
HexagonXfp hmx_xfp_to_xfp(HmxXfp in, uint8_t bits_int,
                                uint8_t bits_frac, uint8_t bits_exp);

#endif /* HEXAGON_HMX_XFP_H */
