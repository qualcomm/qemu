/*
 * Hexagon HMX (Matrix eXtensions) XFP arithmetic
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Integer-only implementation of the hardware XFP arithmetic helpers
 * used by the HMX FP MAC and convert path.
 * No <math.h> dependency.
 */

#include "qemu/osdep.h"
#include "hmx_config.h"
#include "hmx_xfp.h"

#define FP16_NAN 0xFFFFF
#define FP8_NAN  0x80

typedef struct {
    int32_t min;
    int32_t max;
} HexagonXfpExpRange;

/* Decoded IEEE-style fields. */
typedef struct {
    uint32_t exp;
    uint64_t frac;
    uint8_t sign;
    uint32_t bits_frac;
    uint32_t bits_exp;
} HexagonHmxFp;

#define COUNT_LEADING_ZEROS_8(X) ((X) ? __builtin_clzll(X) : 64)

/* Unbiased exponent range */
static inline HexagonXfpExpRange get_exp_range_unbiased(int32_t exp_bits)
{
    HexagonXfpExpRange out;
    out.min = -(1 << (exp_bits - 1));
    out.max = -out.min - 1;
    return out;
}

/* fp -> hmx-fp conversion */
static HexagonHmxFp fp_to_hmx_fp(uint32_t in, uint32_t exp, uint32_t frac)
{
    HexagonHmxFp out = {0};
    uint32_t exp_mask = ((1u << exp) - 1) << frac;
    uint32_t sign_mask = 1u << (exp + frac);
    uint32_t frac_mask = (1u << frac) - 1;

    out.exp = (in & exp_mask) >> frac;
    out.sign = (in & sign_mask) >> (exp + frac);
    out.frac = in & frac_mask;
    out.bits_exp = exp;
    out.bits_frac = frac;
    return out;
}

/* fp -> xfp */
HexagonXfp hexagon_xfp_from_fp(const HmxConfig *hmx_cfg,
                               HexagonXfpUsr usr, uint32_t in,
                               uint32_t frac_in, uint32_t exp_in,
                               uint32_t int_out, uint32_t frac_out,
                               uint32_t exp_out, uint32_t normalize)
{
    HexagonXfp out_xfp = {0};
    out_xfp.bits_int = (uint8_t)int_out;
    out_xfp.bits_exp = (uint8_t)exp_out;
    out_xfp.bits_frac = (uint8_t)frac_out;

    const int32_t in_exp_min = 1;
    const int32_t in_exp_max = (1 << exp_in) - 1;
    const int32_t input_expo_bias = (1 << (exp_in - 1));

    HexagonXfpExpRange out_exp_range = get_exp_range_unbiased(exp_out);

    HexagonHmxFp in_fp = fp_to_hmx_fp(in, exp_in, frac_in);
    const int32_t in_denorm = (in_fp.exp < (uint32_t)in_exp_min);

    int32_t exp_unbiased = (int32_t)in_fp.exp - input_expo_bias;
    out_xfp.exp = exp_unbiased += (in_fp.exp == 0) ? 1 : 0;

    uint64_t int_bit = in_denorm ? 0 : 1;
    uint64_t mag_out = (int_bit << frac_in) | in_fp.frac;

    if (normalize && in_denorm) {
        uint64_t tmp = mag_out << (64 - (frac_in + 1));
        int32_t normalizing_shift = COUNT_LEADING_ZEROS_8(tmp);
        mag_out <<= normalizing_shift;
        out_xfp.exp -= normalizing_shift;
    }

    int64_t sig_out = in_fp.sign ? -(int32_t)mag_out : (int32_t)mag_out;
    int32_t shift = (int32_t)(int_out + frac_out - 2 - frac_in);
    shift = (shift < 0) ? 0 : shift;
    sig_out = sig_out << shift;
    out_xfp.sig = sig_out;

    uint8_t inf = ((in_fp.exp == (uint32_t)in_exp_max) & (in_fp.frac == 0)) *
                  (in_fp.sign ? 2 : 1);
    uint8_t nan = ((in_fp.exp == (uint32_t)in_exp_max) & (in_fp.frac != 0))
                  ? 3 : 0;

    out_xfp.status.zero = ((in_fp.exp == 0) & (in_fp.frac == 0));
    out_xfp.status.inf = usr.inf_nan_enable ? (inf | nan) : 0;
    out_xfp.status.negative = out_xfp.status.zero ? 0 : in_fp.sign;
    out_xfp.exp = out_xfp.status.zero ? out_exp_range.min : out_xfp.exp;

    return out_xfp;
}

void hexagon_xfp_zero(const HmxConfig *hmx_cfg, HexagonXfp *out)
{
    HexagonXfpUsr usr = {0};
    *out = hexagon_xfp_from_fp(hmx_cfg, usr, 0x0000, 10, 5,
                               hmx_cfg->mx_fp_acc_int, hmx_cfg->mx_fp_acc_frac,
                               hmx_cfg->mx_fp_acc_exp, 0);
}

static uint32_t get_max_value(int is_f8, uint32_t sign, uint32_t fp_exp,
                              uint32_t fp_frac, HexagonXfpCvtRs rs,
                              HexagonXfpUsr usr)
{
    const uint32_t max_ieee_exp = ((1u << fp_exp) - 1);
    uint32_t max_exp = max_ieee_exp;

    if (!is_f8) {
        fp_frac += 4 * (rs.fp_rnd == 0);
        max_exp = max_ieee_exp - rs.fp_maxnorm;
    }
    const uint32_t max_mantissa =
        ((rs.fp_maxnorm || !usr.inf_nan_enable) ? ((1u << fp_frac) - 1) : 0);
    return (sign << (fp_exp + fp_frac)) | (max_exp << fp_frac) | max_mantissa;
}

/* xfp -> fp output */
uint32_t hexagon_xfp_to_fp(const HmxConfig *hmx_cfg, int is_f8,
                           HexagonXfpUsr usr, HexagonXfp in,
                           uint32_t fp_frac, uint32_t fp_exp,
                           HexagonXfpCvtRs rs)
{
    uint32_t output = 0;

    int32_t sign_bit = fp_exp + fp_frac + 4 * (rs.fp_rnd == 0);
    if (is_f8) {
        sign_bit = fp_exp + fp_frac;
    }

    int64_t tmp_sig = in.sig << (64 - (in.bits_int + in.bits_frac));
    uint64_t mag_in = (tmp_sig < 0) ? (uint64_t)-tmp_sig : (uint64_t)tmp_sig;
    int32_t exp_normalized = in.exp;
    uint8_t zero = 0;

    if (mag_in != 0) {
        int32_t normalizing_shift = COUNT_LEADING_ZEROS_8(mag_in);
        if (normalizing_shift) {
            mag_in <<= normalizing_shift;
            exp_normalized -= normalizing_shift;
        }
        exp_normalized += (in.bits_int - 2);
    } else {
        zero = 1;
    }

    HexagonXfpExpRange fp_exp_range = get_exp_range_unbiased(fp_exp);
    uint8_t exp_overflow =
        (exp_normalized >= (fp_exp_range.max + (usr.inf_nan_enable == 0)));
    uint8_t exp_underflow = (exp_normalized <= fp_exp_range.min);
    uint8_t unrecoverable =
        (uint8_t)((exp_normalized < (fp_exp_range.min - (int32_t)fp_frac)));

    uint16_t sign_in = (in.status.zero) ? in.status.negative
                                        : (in.sig < 0) || (in.status.negative);
    uint16_t is_neg;

    if (is_f8 && (in.status.inf != 0 || (exp_overflow && !zero))) {
        if (in.status.inf == 0x3) {
            if (usr.inf_nan_enable && !rs.fp_maxnorm) {
                output = FP8_NAN;
            } else {
                output = get_max_value(is_f8, 1, fp_exp, fp_frac, rs, usr);
            }
            if (!rs.relu && !usr.nan_propagate) {
                output = 0;
            }
            return output;
        }

        is_neg = (in.status.inf != 0) ? (in.status.inf == 2) : sign_in;

        if (is_neg && !rs.relu) {
            return 0;
        }
        if (!usr.inf_nan_enable) {
            return get_max_value(is_f8, is_neg, fp_exp, fp_frac, rs, usr);
        } else {
            if (!rs.fp_maxnorm) {
                return FP8_NAN;
            } else {
                return get_max_value(is_f8, is_neg, fp_exp, fp_frac, rs, usr);
            }
        }
    } else {
        if (in.status.inf == 0x3) {
            output = FP16_NAN;
            if (rs.fp_maxnorm && !usr.nan_propagate) {
                output = get_max_value(is_f8, 1, fp_exp, fp_frac, rs, usr);
            }
            if (!rs.relu && !usr.nan_propagate) {
                output = 0;
            }
            return output;
        } else if (in.status.inf != 0) {
            output = ((in.status.inf == 2) && !rs.relu)
                ? 0
                : get_max_value(is_f8, (in.status.inf == 2), fp_exp, fp_frac,
                                rs, usr);
            return output;
        } else if (exp_overflow && !zero) {
            output = (sign_in && !rs.relu)
                ? 0
                : get_max_value(is_f8, sign_in, fp_exp, fp_frac, rs, usr);
            return output;
        } else if (in.status.zero || zero || in.status.under) {
            output = (!rs.relu) ? 0 : (sign_in << sign_bit);
            return output;
        }
    }

    if (exp_underflow) {
        mag_in >>= (fp_exp_range.min - exp_normalized + 1);
    }

    int32_t ulp_bit = 64 - (1 + fp_frac);
    int32_t grd_bit = ulp_bit - 1;
    uint64_t sticky_mask = ((1ll << grd_bit) - 1);
    uint64_t sticky = (sticky_mask & mag_in) != 0ll;
    uint64_t grd = (mag_in >> grd_bit) & 0x1;
    uint64_t ulp = (mag_in >> ulp_bit) & 0x1;
    uint64_t rnd = (ulp) ? (grd) : (sticky & grd);
    uint16_t fp_mantissa = (uint16_t)((mag_in + (rnd << grd_bit)) >> ulp_bit);

    int32_t exp_bias = (1 << (fp_exp - 1));
    int32_t fp_exponent = (exp_underflow) ? -exp_bias : exp_normalized;
    int32_t set_max_mantissa = 0;
    if (exp_underflow) {
        if ((fp_mantissa & (1 << fp_frac)) && !unrecoverable) {
            fp_exponent = -exp_bias + 1;
        } else if (unrecoverable) {
            output = (!rs.relu) ? 0 : (sign_in << sign_bit);
            return output;
        }
    } else if (fp_mantissa == 0) {
        fp_exponent++;
        if (fp_exponent >= (fp_exp_range.max + (usr.inf_nan_enable == 0))) {
            if (is_f8) {
                fp_exponent = fp_exp_range.max;
                if (in.status.negative && !rs.relu) {
                    return 0;
                }
                if (usr.inf_nan_enable && !rs.fp_maxnorm) {
                    return FP8_NAN;
                }
            } else {
                fp_exponent = fp_exp_range.max - rs.fp_maxnorm;
            }
            if (!usr.inf_nan_enable || rs.fp_maxnorm) {
                fp_mantissa = (1 << fp_frac) - 1;
                set_max_mantissa = 1;
            }
        }
    }

    fp_mantissa &= (1 << fp_frac) - 1;
    fp_exponent += exp_bias;
    fp_exponent &= ((1 << fp_exp) - 1);

    if (is_f8) {
        output = (sign_in << sign_bit) | (fp_exponent << fp_frac) | fp_mantissa;
    } else {
        output = (sign_in << sign_bit) |
                 (((fp_exponent << fp_frac) | fp_mantissa) <<
                  ((rs.fp_rnd) ? 0 : 4));
    }

    if (sign_in && !rs.relu) {
        output = 0;
    } else if (set_max_mantissa) {
        if (is_f8) {
            output |= 0x7;
        } else {
            output |= 0xF;
        }
    }
    return output;
}

/* Right shift with inexact */
static int64_t right_shift_with_inexact(const HmxConfig *hmx_cfg,
                                        int64_t in, int32_t shift)
{
    int64_t inexact_mask =
        hmx_cfg->xfp_inexact_enable ? ((1ll << shift) - 1) : 0;
    int64_t inexact = (inexact_mask & in) != 0;
    return (in >> shift) | inexact;
}

/* normalize */
/* convert normalize */
HexagonXfp hexagon_xfp_cvt_normalize(const HmxConfig *hmx_cfg,
                                     HexagonXfpUsr usr, HexagonXfp in,
                                     uint32_t int_out, uint32_t frac_out,
                                     int32_t exp_out)
{
    HexagonXfp tmp = {0};
    tmp.bits_exp = in.bits_exp;
    tmp.bits_frac = in.bits_frac;
    tmp.bits_int = in.bits_int;
    HexagonXfp out = {0};
    out.bits_exp = (uint8_t)exp_out;
    out.bits_frac = (uint8_t)frac_out;
    out.bits_int = (uint8_t)int_out;
    HexagonXfpExpRange out_exp_range = get_exp_range_unbiased(exp_out);

    int32_t out_bit_count = out.bits_frac + out.bits_int;
    int32_t in_bit_count = in.bits_frac + in.bits_int;

    uint8_t neg_sig = (in.sig < 0) && (in.sig != 0);

    if (in.sig == 0) {
        out.sig = 0;
        tmp.exp = in.exp - (in.bits_frac + in.bits_int - 1);
        out.exp = tmp.exp + (tmp.bits_int - out.bits_int);
        out.status.zero = in.status.zero;
        out.status.under = (in.status.inf == 0) ? 1 : 0;
        out.status.inf = in.status.inf;
        out.status.negative = in.status.negative;
        return out;
    }

    tmp.sig = in.sig << (64 - in_bit_count);
    tmp.sig = (neg_sig) ? ~tmp.sig : tmp.sig;
    int32_t normalizing_shift = COUNT_LEADING_ZEROS_8(tmp.sig) - 1;
    uint8_t ovf = (normalizing_shift == 0) && (out.exp == out_exp_range.max);
    normalizing_shift = (normalizing_shift < 0) ? 0 : normalizing_shift;

    tmp.sig = (neg_sig) ? ~tmp.sig : tmp.sig;
    tmp.sig = (tmp.sig << normalizing_shift);

    tmp.sig >>= (64 - in_bit_count);
    tmp.exp = in.exp - normalizing_shift;
    out.sig = right_shift_with_inexact(hmx_cfg, tmp.sig,
                                       (in_bit_count - out_bit_count));
    out.exp = tmp.exp + (tmp.bits_int - out.bits_int);

    out.status.zero = in.status.zero;
    out.status.inf = in.status.inf;
    out.status.negative = in.status.negative;

    if ((out.exp > out_exp_range.max) || (ovf)) {
        if ((out.status.inf == 0) && usr.inf_nan_enable) {
            out.status.inf |= out.status.negative ? 2 : 1;
        }
        out.exp = out_exp_range.max;
    } else if ((out.exp < out_exp_range.min) && (out.status.inf == 0)) {
        out.exp = out_exp_range.min;
        out.status.under = 1;
    }
    return out;
}

HexagonXfp hexagon_xfp_add(const HmxConfig *hmx_cfg, HexagonXfpUsr usr,
                           HexagonXfp in_a, HexagonXfp in_b)
{
    HexagonXfp out = {0};
    out.bits_exp = in_a.bits_exp;
    out.bits_frac = in_a.bits_frac;
    out.bits_int = (uint8_t)(in_a.bits_int + 1);

    uint8_t a_neg = (in_a.status.zero | in_a.status.inf) ? in_a.status.negative
                                                         : in_a.sig < 0;
    uint8_t b_neg = (in_b.status.zero | in_b.status.inf) ? in_b.status.negative
                                                         : in_b.sig < 0;
    uint8_t z_plus_z_sign = ((in_a.sig == 0) && (in_b.sig == 0))
        ? (in_a.status.negative && in_b.status.negative) : 0;
    uint8_t inf = in_a.status.inf | in_b.status.inf;

    if (in_a.status.under) {
        in_a.exp = -(1 << (in_a.bits_exp - 1));
    }
    if (in_b.status.under) {
        in_b.exp = -(1 << (in_b.bits_exp - 1));
    }

    int32_t delta_exp = in_a.exp - in_b.exp;
    uint8_t delta_exp_neg = (delta_exp <= 0);
    int64_t max_sig = delta_exp_neg ? in_b.sig : in_a.sig;
    int64_t min_sig = delta_exp_neg ? in_a.sig : in_b.sig;

    out.exp = delta_exp_neg ? in_b.exp : in_a.exp;

    delta_exp = delta_exp_neg ? -delta_exp : delta_exp;
    if (delta_exp > 63) {
        delta_exp = 63;
    }

    min_sig = right_shift_with_inexact(hmx_cfg, min_sig, delta_exp);
    out.sig = (max_sig + min_sig);
    uint8_t neg_out = out.sig < 0;

    out.status.zero = 0;
    out.status.inf = inf;
    out.status.under = (in_a.status.under && in_b.status.under) && !inf;

    if (out.status.inf == 0) {
        out.status.negative = (a_neg & b_neg) | z_plus_z_sign | neg_out;
    } else {
        out.status.negative = (inf >> 1);
    }
    return out;
}

/* 8-way 2-stage add */
/* multiply */
HexagonXfp hexagon_xfp_mult(const HmxConfig *hmx_cfg, HexagonXfpUsr usr,
                            HexagonXfp in_a, HexagonXfp in_b, uint32_t exp_out)
{
    HexagonXfp out = {0};
    out.bits_exp = (uint8_t)exp_out;
    out.bits_frac = (uint8_t)(2 * in_a.bits_frac);
    out.bits_int = (uint8_t)(2 * in_a.bits_int - 1);

    out.sig = (int64_t)in_a.sig * (int64_t)in_b.sig;
    out.exp = in_a.exp + in_b.exp;
    out.status.zero = in_a.status.zero || in_b.status.zero;
    out.status.negative =
        (in_a.status.negative && !in_b.status.negative) ||
        (!in_a.status.negative && in_b.status.negative);
    out.status.under = in_a.status.under | in_b.status.under;

    uint8_t inf_a = ((in_a.status.inf == 1) || (in_a.status.inf == 2));
    uint8_t inf_b = ((in_b.status.inf == 1) || (in_b.status.inf == 2));

    uint8_t z_a = in_a.sig == 0;
    uint8_t z_b = in_b.sig == 0;

    out.status.in0_zero = z_a || in_a.status.zero;
    out.status.in1_zero = z_b || in_b.status.zero;

    if ((in_a.status.inf == 3) || (in_b.status.inf == 3)) {
        out.status.inf = usr.nan_propagate ? 3 : 0;
    } else if (out.status.under && (inf_a || inf_b)) {
        out.status.inf = usr.nan_propagate ? 3 : 0;
    } else if (inf_a && inf_b) {
        out.status.inf = (out.status.negative) ? 2 : 1;
    } else if ((inf_a && z_b) || (inf_b && z_a)) {
        out.status.inf = usr.nan_propagate ? 3 : 0;
        out.status.zero = usr.nan_propagate ? 0 : 1;
    } else if (inf_a || inf_b) {
        out.status.inf = (out.status.negative) ? 2 : 1;
    }

    if (out.status.zero) {
        out.exp = -(1 << (out.bits_exp - 1));
        out.sig = 0;
    } else if (z_a || z_b) {
        out.exp = -(1 << (out.bits_exp - 1));
        out.sig = 0;
        out.status.negative = 0;
    }
    return out;
}

/* FP16/BF16 product */
/* MAC reduction */
/* compare */
HexagonXfp hexagon_xfp_cmp(const HmxConfig *hmx_cfg, HexagonXfpUsr usr,
                           HexagonXfp a, HexagonXfp b, int32_t min_max)
{
    HexagonXfp x = (min_max) ? b : a;
    HexagonXfp y = (min_max) ? a : b;

    if (a.status.inf | b.status.inf) {
        if (a.status.inf == 3) {
            return usr.nan_propagate ? a : b;
        } else if (b.status.inf == 3) {
            return usr.nan_propagate ? b : a;
        }
        if (a.status.inf == 2) {
            return x;
        } else if (b.status.inf == 2) {
            return y;
        }
        return (a.status.inf == 1) ? y : x;
    }

    uint8_t neg_a = (a.sig < 0);
    uint8_t neg_b = (b.sig < 0);
    if (neg_a ^ neg_b) {
        return neg_a ? x : y;
    }

    if (a.sig == 0) {
        return (neg_b) ? y : x;
    }
    if (b.sig == 0) {
        return (neg_a) ? x : y;
    }
    if (a.exp != b.exp) {
        return (neg_a) ? ((a.exp < b.exp) ? y : x)
                       : ((a.exp < b.exp) ? x : y);
    }
    return (a.sig <= b.sig) ? x : y;
}

/* Accumulator shaping */
static HexagonXfp hmx_xfp_acc_shaping(const HmxConfig *hmx_cfg,
                                      HexagonXfpUsr usr, HexagonXfpBias bias,
                                      HexagonXfp acc)
{
    HexagonXfp zero;
    hexagon_xfp_zero(hmx_cfg, &zero);
    HexagonXfp out = acc;
    switch (bias.shape) {
    case 1:
        out = hexagon_xfp_cmp(hmx_cfg, usr, acc, zero, HEXAGON_XFP_MIN);
        break;
    case 2:
        out = hexagon_xfp_cmp(hmx_cfg, usr, acc, zero, HEXAGON_XFP_MAX);
        break;
    }
    return out;
}

/* xfp -> fp convert */
static uint32_t hmx_xfp_fp_cvt(const HmxConfig *hmx_cfg, int is_f8,
                               HexagonXfpUsr usr, HexagonXfp acc,
                               HexagonXfpBias bias_reg, uint32_t cvt_feedback,
                               HexagonXfpCvtRs rs, uint32_t fp_frac,
                               uint32_t fp_exp)
{
    const uint32_t exp_out = hmx_cfg->xfp_cvt_exp;
    const uint32_t frac_out = hmx_cfg->xfp_cvt_frac;
    const uint32_t int_out = hmx_cfg->xfp_cvt_int;
    const uint32_t input_norm = 0;

    uint32_t extra = 5;
    uint32_t val = ((uint32_t)bias_reg.acc_bias << extra) |
                   (uint32_t)bias_reg.acc_bias_extra;

    HexagonXfpUsr usr_internal = usr;
    usr_internal.inf_nan_enable = 1;
    usr_internal.nan_propagate = 1;

    HexagonXfp acc_bias =
        hexagon_xfp_from_fp(hmx_cfg, usr, val, fp_frac + extra, fp_exp, int_out,
                            hmx_cfg->mx_fp_acc_frac, hmx_cfg->mx_fp_acc_exp,
                            input_norm);
    HexagonXfp acc_biased =
        hexagon_xfp_add(hmx_cfg, usr_internal, acc, acc_bias);
    HexagonXfp acc_normalized =
        hexagon_xfp_cvt_normalize(hmx_cfg, usr_internal, acc_biased, int_out,
                                  frac_out, exp_out);

    const uint32_t abs_negate =
        (bias_reg.shape == 0x3) ? acc_normalized.status.negative : 0;
    const uint32_t scale_negate = (bias_reg.negate ^ abs_negate) << 19;

    extra = 4;
    val = ((uint32_t)bias_reg.scale << extra) | (uint32_t)bias_reg.scale_extra;
    val ^= scale_negate;
    HexagonXfp scale =
        hexagon_xfp_from_fp(hmx_cfg, usr, val, fp_frac + extra, fp_exp, int_out,
                            frac_out + 1, exp_out, input_norm);

    if (rs.fb_dst == HEXAGON_XFP_FB_SCALE) {
        cvt_feedback ^= scale_negate;
        HexagonXfp feedback =
            hexagon_xfp_from_fp(hmx_cfg, usr, cvt_feedback, fp_frac + extra,
                                fp_exp, int_out, frac_out + 1, exp_out,
                                input_norm);
        scale = hexagon_xfp_cmp(hmx_cfg, usr, scale, feedback,
                                rs.fb_limit ^ (bias_reg.negate ^ abs_negate));
    }

    scale.bits_frac--;
    scale.sig >>= 1;

    HexagonXfp acc_shaped =
        hmx_xfp_acc_shaping(hmx_cfg, usr, bias_reg, acc_normalized);

    scale.bits_exp++;
    HexagonXfp acc_scaled =
        hexagon_xfp_mult(hmx_cfg, usr_internal, acc_shaped, scale,
                         scale.bits_exp);
    acc_scaled.bits_exp = scale.bits_exp;

    val = ((uint32_t)bias_reg.out_bias << extra) |
          (uint32_t)bias_reg.out_bias_extra;
    HexagonXfp out_bias =
        hexagon_xfp_from_fp(hmx_cfg, usr, val, fp_frac + extra, fp_exp, int_out,
                            frac_out, exp_out, input_norm);

    if (rs.fb_dst == HEXAGON_XFP_FB_OUTBIAS) {
        HexagonXfp feedback =
            hexagon_xfp_from_fp(hmx_cfg, usr, cvt_feedback, fp_frac + extra,
                                fp_exp, int_out, frac_out, exp_out,
                                input_norm);
        out_bias = hexagon_xfp_cmp(hmx_cfg, usr, out_bias, feedback,
                                   rs.fb_limit);
    }
    out_bias.sig <<= (acc_scaled.bits_frac - out_bias.bits_frac);

    HexagonXfp acc_final =
        hexagon_xfp_add(hmx_cfg, usr_internal, acc_scaled, out_bias);

    if (is_f8) {
        const uint32_t mantissa_bits = 3;
        const uint32_t exp_bits = 4;
        return hexagon_xfp_to_fp(hmx_cfg, is_f8, usr, acc_final, mantissa_bits,
                                 exp_bits, rs);
    } else {
        return hexagon_xfp_to_fp(hmx_cfg, is_f8, usr, acc_final,
                                 (fp_frac + rs.fp_rnd * 4), fp_exp, rs);
    }
}

/* xfp convert */
uint32_t hexagon_xfp_convert(const HmxConfig *hmx_cfg, int is_f8,
                             HexagonXfpUsr usr, HexagonXfp acc,
                             HexagonXfpBias bias, uint32_t cvt_feedback,
                             HexagonXfpCvtRs rs)
{
    if (!hmx_cfg->mx_fp_present) {
        return 0;
    }
    /*
     * Both FP16/BF16 and F8 decode the
     * bias/scale registers with the FP16/BF16 frac/exp; is_f8 only changes
     * the final output format inside hmx_xfp_fp_cvt.  F8 forces is_bf16=0.
     */
    const uint32_t mantissa_bits = (rs.is_bf16 ? 7 : 10);
    const uint32_t exp_bits = rs.is_bf16 ? 8 : 5;
    return hmx_xfp_fp_cvt(hmx_cfg, is_f8, usr, acc, bias, cvt_feedback, rs,
                          mantissa_bits, exp_bits);
}

/*
 * Widen a flat product/accumulator cell to the generic HexagonXfp
 * shape, used at the accumulator/product-cache boundary with
 * hmx_fp_convert_xfp(), which reads HmxAccFp.xfp_data (flat) and
 * needs a HexagonXfp to hand to the convert primitives above.
 */
HexagonXfp hmx_xfp_to_xfp(HmxXfp in, uint8_t bits_int,
                                uint8_t bits_frac, uint8_t bits_exp)
{
    HexagonXfp out = {0};
    out.status = in.status;
    out.exp = in.exp;
    out.sig = in.sig;
    out.bits_int = bits_int;
    out.bits_frac = bits_frac;
    out.bits_exp = bits_exp;
    out.lza = 0;
    return out;
}
