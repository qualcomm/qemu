/*
 * Hexagon HMX (Matrix eXtensions) XFP arithmetic
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Hexagon HMX XFP arithmetic: generic width-carrying HexagonXfp
 * primitives (FP convert path) plus flat v81-fixed HmxXfp
 * primitives (v81 FP MAC path). No <math.h> dependency.
 *
 * v75/v79 never use this file's MAC-shaped functions at all
 * (hmx_matmul_fp_dbl(), native double); they do use the generic
 * convert-path functions via hmx_fp_convert_dbl(), same as v81.
 *
 * IMPORTANT -- the pre-flat reference normalize step had a
 * load-bearing bug: an `ovf` flag read its output exponent before
 * that exponent was ever assigned, so `ovf` was unconditionally
 * false. The real overflow catch was a separate, correctly-computed
 * post-shift `out.exp > out_exp_range.max` check a few lines later.
 * hmx_xfp_batch8() below replicates that: no `ovf` variable
 * exists there at all, only the correct post-shift check. Do not
 * "fix" this by adding an overflow flag computed from the pre-shift
 * exponent -- that changes behavior at in.exp==254 && lza==0 and
 * other exponent/LZA edges. Random fuzzing does not reach those
 * edges; a directed exponent x LZA sweep does.
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

/*
 * The XFP_ST_* masks used below (and by
 * hmx_xfp_mult_zero_weight() in hmx_xfp.h) live in hmx_xfp.h,
 * shared by both -- see the comment there for why (packed status word
 * measurement) and xfp_status_layout_assert() immediately below
 * for the compile-time layout check that pins them to
 * HexagonXfpStatus's real bitfield order.
 */
static inline void xfp_status_layout_assert(void)
{
    qemu_build_assert(((HexagonXfpStatus){ .zero = 1 }).val ==
                      XFP_ST_ZERO);
    qemu_build_assert(((HexagonXfpStatus){ .inf = 3 }).val ==
                      XFP_ST_INF);
    qemu_build_assert(((HexagonXfpStatus){ .inf = 1 }).val ==
                      (1u << XFP_ST_INF_SHIFT));
    qemu_build_assert(((HexagonXfpStatus){ .negative = 1 }).val ==
                      XFP_ST_NEG);
    qemu_build_assert(((HexagonXfpStatus){ .under = 1 }).val ==
                      XFP_ST_UNDER);
    qemu_build_assert(((HexagonXfpStatus){ .in0_zero = 1 }).val ==
                      XFP_ST_IN0_ZERO);
    qemu_build_assert(((HexagonXfpStatus){ .in1_zero = 1 }).val ==
                      XFP_ST_IN1_ZERO);
}

/*
 * Sticky-bit-into-LSB right shift, matching right_shift_with_inexact()
 * above with xfp_inexact_enable hardcoded true (hmx_config.c sets
 * hmx_cfg->xfp_inexact_enable = 1 unconditionally for every revision, so
 * the hmx_cfg-> read there is always this value).
 */
static inline int64_t xfp_shift_inexact(int64_t in, int32_t shift)
{
    int64_t inexact_mask = ((int64_t)1 << shift) - 1;
    int64_t inexact = (inexact_mask & in) != 0;
    return (in >> shift) | inexact;
}

static inline uint32_t xfp_compute_shift(int32_t base_exp,
                                               int32_t relative_exp)
{
    int32_t shift = base_exp - relative_exp;
    if (shift < 0) {
        shift = -shift;
    }
    if (shift > 63) {
        shift = 63;
    }
    return (uint32_t)shift;
}

/* Leading-zero anticipation, flat-path shape (pure bit-trick, no
 * width-specialization possible). */
static uint8_t xfp_compute_lza(int64_t a, int64_t b, int32_t msb_bit)
{
    const int64_t p = (a ^ b);
    const int64_t g = (a & b);
    const int64_t z = ~(a | b);
    const int64_t msb = (int64_t)1 << msb_bit;
    const int64_t zeros = (p ^ ~(z << 1));
    const int64_t ones = (p ^ ~(g << 1));
    const int64_t z_msb = (msb & zeros) >> msb_bit;
    const int64_t o_msb = (msb & ones) >> msb_bit;
    const int32_t lza_z =
        COUNT_LEADING_ZEROS_8(z_msb ? ~zeros : zeros) - (64 - msb_bit);
    const int32_t lza_o =
        COUNT_LEADING_ZEROS_8(o_msb ? ~ones : ones) - (64 - msb_bit);

    uint8_t lza = 0;
    if (msb & z) {
        lza = (uint8_t)lza_z;
    } else if (msb & g) {
        lza = (uint8_t)lza_o;
    } else {
        lza = (uint8_t)(lza_z > lza_o ? lza_z : lza_o);
    }
    return lza;
}

/*
 * hexagon_xfp_from_fp(hmx_cfg, usr, in, frac_in=10, exp_in=5, int_out=3,
 * frac_out=9, exp_out=8, normalize=1), FP16 operand shape.
 */
HmxXfp hmx_xfp_decode_fp16(HexagonXfpUsr usr, uint16_t in)
{
    HmxXfp out = {0};

    uint32_t in_exp = (in >> 10) & 0x1F;
    uint32_t in_sign = (in >> 15) & 1;
    uint32_t in_frac = in & 0x3FF;

    const int32_t in_exp_min = 1;
    const int32_t in_exp_max = 31;
    const int32_t input_expo_bias = 16;

    int32_t in_denorm = ((int32_t)in_exp < in_exp_min);

    int32_t exp_unbiased = (int32_t)in_exp - input_expo_bias;
    int32_t out_exp = exp_unbiased + (in_exp == 0 ? 1 : 0);

    uint64_t int_bit = in_denorm ? 0 : 1;
    uint64_t mag_out = (int_bit << 10) | in_frac;

    if (in_denorm) {
        uint64_t tmp = mag_out << (64 - 11);
        int32_t normalizing_shift = COUNT_LEADING_ZEROS_8(tmp);
        mag_out <<= normalizing_shift;
        out_exp -= normalizing_shift;
    }

    /* shift = int_out+frac_out-2-frac_in = 3+9-2-10 = 0: no sig shift. */
    out.sig = in_sign ? -(int32_t)mag_out : (int32_t)mag_out;
    out.exp = out_exp;

    uint8_t inf = ((in_exp == (uint32_t)in_exp_max) && (in_frac == 0))
                  ? (in_sign ? 2 : 1) : 0;
    uint8_t nan = ((in_exp == (uint32_t)in_exp_max) && (in_frac != 0))
                  ? 3 : 0;

    out.status.zero = (in_exp == 0) && (in_frac == 0);
    out.status.inf = usr.inf_nan_enable ? (inf | nan) : 0;
    out.status.negative = out.status.zero ? 0 : in_sign;
    out.exp = out.status.zero ? -(1 << (8 - 1)) : out.exp;

    return out;
}

/*
 * hexagon_xfp_from_fp(hmx_cfg, usr, in, frac_in=7, exp_in=8, int_out=3,
 * frac_out=9, exp_out=8, normalize=0), BF16 operand shape.
 *
 * Differs from hmx_xfp_decode_fp16() above in exactly the ways
 * the generic decode's is_bf16 shape does: 7-bit fraction, 8-bit
 * exponent with bias 128, sig pre-shifted left by
 * (int_out+frac_out-2-frac_in = 3+9-2-7 = 3) to land in the same
 * fixed-point shape as the FP16 decode's unshifted output, and no
 * denormal renormalize block at all (BF16 denormals keep int_bit=0
 * and an unnormalized mag -- do not add the FP16 decode's
 * COUNT_LEADING_ZEROS_8 branch here, that is the one width difference
 * that changes sig/exp shape, not just widths).
 */
HmxXfp hmx_xfp_decode_bf16(HexagonXfpUsr usr, uint16_t in)
{
    HmxXfp out = {0};

    uint32_t in_exp = (in >> 7) & 0xFF;
    uint32_t in_sign = (in >> 15) & 1;
    uint32_t in_frac = in & 0x7F;

    const int32_t in_exp_min = 1;
    const int32_t in_exp_max = 255;
    const int32_t input_expo_bias = 128;

    int32_t in_denorm = ((int32_t)in_exp < in_exp_min);

    int32_t exp_unbiased = (int32_t)in_exp - input_expo_bias;
    int32_t out_exp = exp_unbiased + (in_exp == 0 ? 1 : 0);

    uint64_t int_bit = in_denorm ? 0 : 1;
    uint64_t mag_out = (int_bit << 7) | in_frac;

    /* input_norm=0 for BF16: no denormal renormalize block. */

    /* shift = int_out+frac_out-2-frac_in = 3+9-2-7 = 3. */
    int32_t mag_shifted = (int32_t)(mag_out << 3);
    out.sig = in_sign ? -mag_shifted : mag_shifted;
    out.exp = out_exp;

    uint8_t inf = ((in_exp == (uint32_t)in_exp_max) && (in_frac == 0))
                  ? (in_sign ? 2 : 1) : 0;
    uint8_t nan = ((in_exp == (uint32_t)in_exp_max) && (in_frac != 0))
                  ? 3 : 0;

    out.status.zero = (in_exp == 0) && (in_frac == 0);
    out.status.inf = usr.inf_nan_enable ? (inf | nan) : 0;
    out.status.negative = out.status.zero ? 0 : in_sign;
    out.exp = out.status.zero ? -(1 << (8 - 1)) : out.exp;

    return out;
}

/* hexagon_xfp_mult-equivalent for the flat shape, product shape
 * (int=5 frac=18 exp=9, implied -- never stored, see hmx_xfp.h).
 *
 * Structurally identical to hexagon_xfp_mult() above, with two purely
 * mechanical transformations (bit-exact for every possible input, not
 * just reachable ones):
 *
 * 1. The status word is built in a plain uint32_t and stored once,
 *    instead of six separate bitfield read-modify-writes. Sign is
 *    (sa ^ sb) & NEG, which is the same as the reference's
 *    (a.neg && !b.neg) || (!a.neg && b.neg).
 * 2. The whole Inf/NaN/zero-combination if/else chain is guarded by
 *    one `(sa | sb) & INF` test. Every arm of that chain requires
 *    a.status.inf != 0 or b.status.inf != 0, so when neither operand
 *    is Inf/NaN the chain provably leaves out.status.inf at 0 --
 *    which is what skipping it produces.
 */
HmxXfp hmx_xfp_mult(HexagonXfpUsr usr, HmxXfp a, HmxXfp b)
{
    xfp_status_layout_assert();

    const uint32_t sa = a.status.val;
    const uint32_t sb = b.status.val;
    const uint32_t z_a = (a.sig == 0);
    const uint32_t z_b = (b.sig == 0);

    uint32_t st = ((sa | sb) & (XFP_ST_ZERO | XFP_ST_UNDER))
                | ((sa ^ sb) & XFP_ST_NEG)
                | ((z_a | (sa & XFP_ST_ZERO)) ? XFP_ST_IN0_ZERO : 0)
                | ((z_b | (sb & XFP_ST_ZERO)) ? XFP_ST_IN1_ZERO : 0);

    HmxXfp out;
    out.sig = a.sig * b.sig;
    out.exp = a.exp + b.exp;

    if ((sa | sb) & XFP_ST_INF) {
        const uint32_t nan = 3u << XFP_ST_INF_SHIFT;
        const uint32_t pos_inf = 1u << XFP_ST_INF_SHIFT;
        const uint32_t neg_inf = 2u << XFP_ST_INF_SHIFT;
        const uint32_t ia = sa & XFP_ST_INF;
        const uint32_t ib = sb & XFP_ST_INF;
        /* inf (either sign), i.e. the reference's inf_a/inf_b */
        const uint32_t inf_a = ia && ia != nan;
        const uint32_t inf_b = ib && ib != nan;
        const uint32_t nan_out = usr.nan_propagate ? nan : 0;

        if (ia == nan || ib == nan) {
            st |= nan_out;
        } else if ((st & XFP_ST_UNDER) && (inf_a || inf_b)) {
            st |= nan_out;
        } else if (inf_a && inf_b) {
            st |= (st & XFP_ST_NEG) ? neg_inf : pos_inf;
        } else if ((inf_a && z_b) || (inf_b && z_a)) {
            st |= nan_out;
            st = usr.nan_propagate ? (st & ~XFP_ST_ZERO)
                                   : (st | XFP_ST_ZERO);
        } else if (inf_a || inf_b) {
            st |= (st & XFP_ST_NEG) ? neg_inf : pos_inf;
        }
    }

    if (st & XFP_ST_ZERO) {
        out.exp = -(1 << (9 - 1));
        out.sig = 0;
    } else if (z_a | z_b) {
        out.exp = -(1 << (9 - 1));
        out.sig = 0;
        st &= ~XFP_ST_NEG;
    }

    out.status.val = st;
    return out;
}

/*
 * Rate-8 batched reduce, accumulator shape (int=8 frac=22 exp=9),
 * rate hardcoded to 8 (hmx_cfg->mx_fp_rate, the only value used on
 * v75/v79/v81 -- see hmx_config.c).
 *
 * The 8-way stage walks products[] once: it reduces all five status
 * flags with two word-level accumulators over status.val (OR for
 * inf/negative, AND for zero/in0_zero/in1_zero -- same masks as
 * hmx_xfp_mult(), pinned by xfp_status_layout_assert()),
 * takes the max exponent, and records which lanes have a nonzero sig
 * in a bitmask, all in the same pass. The alignment/sum pass then
 * visits only the nonzero lanes via the bitmask instead of testing
 * every lane again, and shifts sig by 4 inline instead of
 * materializing a shifted copy of all 8 products in a scratch array.
 * Bit-exact: the per-lane &=/|= reductions of the generic path are
 * the same function as the word-level ones here, and zero-sig lanes
 * contribute nothing to the sum (xfp_shift_inexact(0, n) == 0
 * identically, the pre-existing skip documented below). The 2-way
 * stage and normalize carry their status the same packed way -- see
 * the comment at the 2-way stage below.
 */
HmxXfp hmx_xfp_batch8(const HmxConfig *hmx_cfg,
                                HexagonXfpUsr usr, HmxXfp *products,
                                HmxXfp acc)
{
    uint32_t or_status = 0;
    uint32_t and_status = 0xFFFFFFFFu;
    uint32_t nonzero_lanes = 0;
    int32_t sum8_exp = -(1 << (9 - 1));

    for (int i = 0; i < 8; i++) {
        const uint32_t s = products[i].status.val;
        or_status |= s;
        and_status &= s;
        if (products[i].sig != 0) {
            nonzero_lanes |= 1u << i;
        }
        if (sum8_exp < products[i].exp) {
            sum8_exp = products[i].exp;
        }
    }

    if (!usr.inf_nan_enable &&
        (and_status & (XFP_ST_IN0_ZERO | XFP_ST_IN1_ZERO))) {
        return acc;
    }

    /*
     * 8-way add (no LZA -- not requested for this stage), with all 8
     * products' frac aligned to the acc's (22) from the product's
     * native 18: acc_alignment = 22 - 18 = 4.
     *
     * Only nonzero_lanes are summed. xfp_shift_inexact(0, delta)
     * == 0 for every delta (0 shifted is 0; the sticky/inexact OR-bit
     * is (mask & 0) != 0, always false) -- not an approximation, an
     * identity. Skipping zero lanes matters because they are common:
     * grouped/depthwise convolution's column gating
     * (hmx_fp_spatial_mac_xfp()) produces exact-zero products for
     * most output columns in small-group layers.
     */
    int64_t sum8_sig = 0;
    for (uint32_t m = nonzero_lanes; m; m &= m - 1) {
        const int i = __builtin_ctz(m);
        const uint32_t delta = xfp_compute_shift(sum8_exp,
                                                      products[i].exp);
        sum8_sig += xfp_shift_inexact(products[i].sig << 4, delta);
    }

    /*
     * The 8-way result's status, and then the whole 2-way stage +
     * normalize, are carried as one packed status word rather than as
     * HmxXfp cells with bitfields written one at a time. The
     * generic path's per-field reductions over its 2-element input
     * array map exactly onto bitwise ops on the packed words here:
     *   all_true_zeros2 = a.zero & b.zero   -> (acc_st & sum8_st) & ZERO
     *   z_sign_or2      = a.neg  | b.neg    -> (acc_st | sum8_st) & NEG
     *   inf2            = a.inf  | b.inf    -> (acc_st | sum8_st) & INF
     * and the `inf >> 1` sign-of-Inf extraction becomes a shift of the
     * INF field's high bit.
     *
     * sum8's status only ever has ZERO/INF/NEG set (built from a
     * zero-initialized cell), so under/in0_zero/in1_zero stay 0 here.
     */
    const uint32_t inf8w = or_status & XFP_ST_INF;
    uint32_t neg8;
    if (inf8w == 0) {
        neg8 = ((nonzero_lanes == 0) && (or_status & XFP_ST_NEG)) ||
               (sum8_sig < 0);
    } else {
        neg8 = (inf8w >> (XFP_ST_INF_SHIFT + 1)) & 1;
    }
    const uint32_t sum8_st = (and_status & XFP_ST_ZERO) | inf8w |
                             (neg8 ? XFP_ST_NEG : 0);

    /*
     * 2-way add: [acc, sum8], with LZA requested. The generic path
     * seeds out.exp at -(1<<8) before maxing over both inputs;
     * sum8_exp is itself seeded there and only ever raised, so
     * max(acc.exp, sum8_exp) >= -256 already and the seed is
     * redundant.
     */
    const uint32_t acc_st = acc.status.val;
    const uint32_t or2 = acc_st | sum8_st;
    const uint32_t and2 = acc_st & sum8_st;
    const uint32_t inf2w = or2 & XFP_ST_INF;

    const int32_t sum2_exp = acc.exp > sum8_exp ? acc.exp : sum8_exp;
    /*
     * sig == 0 lanes: same xfp_shift_inexact(0, n) == 0 identity
     * as the 8-way loop above.
     */
    const int64_t aligned0 = acc.sig == 0 ? 0
        : xfp_shift_inexact(acc.sig,
                                 xfp_compute_shift(sum2_exp, acc.exp));
    const int64_t aligned1 = sum8_sig == 0 ? 0
        : xfp_shift_inexact(sum8_sig,
                                 xfp_compute_shift(sum2_exp, sum8_exp));
    const int64_t sum2_sig = aligned0 + aligned1;
    /* msb_bit = frac_bits(22) + int_bits(acc.bits_int+1=9) - 1 = 30 */
    const uint8_t lza2 = xfp_compute_lza(aligned0, aligned1, 30);

    uint32_t st;
    if (inf2w == 0) {
        st = (and2 & XFP_ST_ZERO) |
             ((((acc.sig == 0) && (sum8_sig == 0) &&
                (or2 & XFP_ST_NEG)) || (sum2_sig < 0))
              ? XFP_ST_NEG : 0);
    } else {
        st = (and2 & XFP_ST_ZERO) | inf2w |
             (((inf2w >> (XFP_ST_INF_SHIFT + 1)) & 1)
              ? XFP_ST_NEG : 0);
    }

    /*
     * Normalize equivalent, int_out=8, frac_out=22, exp_out=9,
     * use_lza=1. additional_exp = in.bits_int(9) - out.bits_int(8)
     * = 1, constant for this call shape.
     */
    int32_t norm_exp = sum2_exp + 1;
    int64_t norm_sig = xfp_shift_inexact(sum2_sig, 1);

    /* in_bit_count = out.bits_frac(22) + out.bits_int(8) = 30 */
    int64_t temp_sig = norm_sig << (64 - 30);
    /*
     * The generic normalize inverts temp_sig into temp_sig2 (for a
     * CLZ-based shift count), then inverts it right back before
     * using it -- a net no-op whenever use_lza is true, since the
     * CLZ branch (the only place the first inversion's result is
     * read) is not even evaluated. use_lza is always true on this
     * call path, so temp_sig2 == temp_sig always here; skip the
     * inversion dance entirely.
     */
    int32_t normalizing_shift = lza2;
    int32_t max_shift = hmx_cfg->mx_fp_acc_norm;
    int32_t normalizing_shift2 =
        (normalizing_shift > max_shift) ? max_shift : normalizing_shift;
    normalizing_shift2 = (normalizing_shift2 < 0) ? 0 : normalizing_shift2;

    /* out_exp_range.min for exp_out=9 is -(1<<8) = -256. */
    int32_t at_min_exp_adjust = (norm_exp - normalizing_shift2) - (-256);
    if (at_min_exp_adjust < 0) {
        normalizing_shift2 += at_min_exp_adjust;
    }

    int64_t normalized_sig = temp_sig << normalizing_shift2;
    int32_t out_exp = norm_exp - normalizing_shift2;

    /*
     * out_exp_range.max for exp_out=9 is 255. This is the correct
     * overflow check; the dead `ovf` variable the pre-flat reference
     * ORed with it was always false -- see this file's header
     * comment. Do not add it back.
     */
    if (out_exp > 255) {
        if ((st & XFP_ST_INF) == 0) {
            st |= (st & XFP_ST_NEG) ? (2u << XFP_ST_INF_SHIFT)
                                         : (1u << XFP_ST_INF_SHIFT);
        }
        out_exp = 255;
    }

    HmxXfp result;
    result.sig = normalized_sig >> (64 - 30);
    result.exp = out_exp;
    result.status.val = st;
    return result;
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

