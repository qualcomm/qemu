/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "cpu.h"
#include "reg_fields.h"
#include "mmvec_qfloat.h"

#define BIAS_QF32 127
#define BIAS_EXTQF32 383
#define BIAS_QF16 15
#define BIAS_SF 127
#define BIAS_HF 15

#define E_MAX_QF16 16
#define E_MIN_QF16 -15
#define E_MAX_EXTQF16 E_MAX_QF16
#define E_MAX_EXTQF32 256
#define E_MAX_SF 128
#define E_MIN_SF -126
#define E_MAX_HF 16
#define E_MIN_HF -14

#define sf_MANTBITS 23
#define hf_MANTBITS 10
#define bf_MANTBITS 7

#define EXTQF32_BITMASK 0xFFFFFFFFFULL
#define EXTQF16_BITMASK 0x3FFFFULL

#define MAX_SIG_QF16 0x3ff
#define MAX_SIG_QF32 0x7fffff

#define ieee_pos_NaN_32 0x7FFFFFFF
#define ieee_pos_NaN_16 0x7FFF
#define ieee_pos_inf_32 0x7f800000
#define ieee_pos_inf_16 0x7C00
#define BF_POS_NAN 0x7FFF
#define BF_POS_INF 0x7f80

#define QF32_ILOG2_ZERO_EXP 0x80000000
#define QF32_ILOG2_NAN_EXP  0x7FFFFFFF
#define QF32_ILOG2_INF_EXP  0x7FFFFFFE
#define QF16_ILOG2_ZERO_EXP 0x8000
#define QF16_ILOG2_NAN_EXP  0x7FFF
#define QF16_ILOG2_INF_EXP  0x7FFE

typedef uint8_t LREQ_t;

enum {
    LREQ_Q = 1 << 0,
    LREQ_E = 1 << 1,
    LREQ_R = 1 << 2,
    LREQ_L = 1 << 3,
};

/*
 * Common indicators
 */

bool is_unfloat_neg(unfloat u)
{
    return u.parts.sign ^ u.sign;
}

/* One's complement the LRQ bits, but leave E (exponent extension) alone */
static LREQ_t negate_lreq(LREQ_t lreq)
{
    LREQ_t neg_lreq = ~lreq & EXT32_BITMASK;

    return (neg_lreq & ~LREQ_E) | (lreq & LREQ_E);
}

static bool is_extqf32_nan(uint64_t in)
{
    return in == extqf32_pos_nan || in == extqf32_neg_nan ||
           in == extqf32_pos_nan_inexact || in == extqf32_neg_nan_inexact;
}

static bool is_extqf32_inf(uint64_t in)
{
    return in == extqf32_pos_inf_exact || in == extqf32_pos_inf_inexact ||
           in == extqf32_neg_inf_exact || in == extqf32_neg_inf_inexact;
}

static bool is_extqf16_nan(uint32_t in)
{
    return in == extqf16_pos_nan || in == extqf16_neg_nan;
}

static bool is_extqf16_inf(uint32_t in)
{
    return in == extqf16_pos_inf || in == extqf16_neg_inf;
}

static FloatParts64 qfloat_unpack_ext_parts(unsigned size, uint32_t value,
                                            uint8_t ext)
{
    int32_t sig;
    int exp;
    unsigned precision;
    uint64_t frac;
    bool sign;

    if (size == 32) {
        LREQ_t lreq = ext & EXT32_BITMASK;

        /* L is the low bit of the signed 24-bit significand. */
        sig = (sextract32(value, 7, 25) & ~1) | !!(lreq & LREQ_L);
        /* The R bit is encoded as a deferred negation. */
        sig += !!(lreq & LREQ_R);
        sign = sig < 0;
        exp = ((((value & 0xff) | (!!(lreq & LREQ_E) << 8)) - 128) &
                0x1ff) - 255;
        precision = 23;
    } else {
        LREQ_t lreq = (ext & EXT16_BITMASK) << 2;

        sig = (sextract32(value, 4, 12) & ~1) | !!(lreq & LREQ_L);
        sig += !!(lreq & LREQ_R);
        sign = sig < 0;
        exp = (value & 0x1f) - BIAS_QF16;
        precision = 10;
    }

    if (sig == 0) {
        return (FloatParts64) { .cls = float_class_zero };
    }

    frac = sig < 0 ? -(int64_t)sig : sig;
    return (FloatParts64) {
        .cls = float_class_normal,
        .sign = sign,
        .exp = exp + 63 - clz64(frac) - precision,
        .frac = frac << clz64(frac),
    };
}

FloatParts64 qfloat_unfloat_parts(unfloat u)
{
    FloatParts64 parts = u.parts;

    parts.sign ^= u.sign;
    return parts;
}

static void qfloat_apply_inexact(FloatParts64 *parts, uint8_t ext)
{
    LREQ_t lreq = ext & EXT32_BITMASK;

    if (parts->cls != float_class_normal ||
        !!(lreq & LREQ_R) == !!(lreq & LREQ_Q)) {
        return;
    }
    if (!parts->sign) {
        parts->frac |= 1;
    } else if (--parts->frac < (UINT64_C(1) << 63)) {
        parts->frac = parts->frac << 1 | 1;
        parts->exp--;
    }
}

static uint64_t qfloat_pack_ext(unsigned size, int64_t sig, int biased_exp,
                                LREQ_t lreq)
{
    lreq = (lreq & ~LREQ_E) | (((biased_exp >> 8) & 1) ? LREQ_E : 0);
    if (size == 32) {
        uint64_t value = ((uint64_t)sig << 8) | (biased_exp & 0xff);

        return ((value & QF32_BITMASK) << 4) |
               (lreq & EXT32_BITMASK);
    }
    return (((uint64_t)sig << 5 | (biased_exp & 0x1f)) & QF16_BITMASK) << 2 |
           ((lreq >> 2) & EXT16_BITMASK);
}

static uint64_t qfloat_ext_overflow(unsigned size, bool sign,
                                    qfrnd_mode_enum_t mode)
{
    uint32_t sig = size == 32 ? MAX_SIG_QF32 : MAX_SIG_QF16;
    int exp = size == 32 ? E_MAX_EXTQF32 : E_MAX_EXTQF16;
    int bias = size == 32 ? BIAS_EXTQF32 : BIAS_QF16;
    LREQ_t lreq = 0;

    if (mode == RND_TO_ZERO ||
        (mode == RND_TOWARDS_NEG_INF && !sign) ||
        (mode == RND_TOWARDS_POS_INF && sign)) {
        exp--;
    }
    lreq |= LREQ_L | LREQ_Q;
    if (sign) {
        sig = ~sig;
        lreq = negate_lreq(lreq);
    }
    return qfloat_pack_ext(size, sig, exp + bias, lreq);
}

static uint64_t qfloat_ext_underflow(unsigned size, bool sign,
                                     qfrnd_mode_enum_t mode)
{
    int exp = size == 32 ? E_MIN_EXTQF32 : E_MIN_EXTQF16;
    int bias = size == 32 ? BIAS_EXTQF32 : BIAS_QF16;
    LREQ_t lreq = 0;
    uint32_t sig = 0;

    if (mode == RND_TOWARDS_POS_INF) {
        lreq |= LREQ_R;
    } else if (mode == RND_TOWARDS_NEG_INF) {
        lreq |= LREQ_Q;
    } else {
        lreq |= LREQ_Q;
    }
    if (sign) {
        sig = ~sig;
        lreq = negate_lreq(lreq);
    }
    return qfloat_pack_ext(size, sig, exp + bias, lreq);
}

uint64_t qfloat_round_ext_parts(unsigned size, FloatParts64 parts, int exp,
                                bool deferred_sign,
                                qfrnd_mode_enum_t mode)
{
    unsigned precision = size == 32 ? 23 : 10;
    int emax = size == 32 ? E_MAX_EXTQF32 : E_MAX_EXTQF16;
    int emin = size == 32 ? E_MIN_EXTQF32 : E_MIN_EXTQF16;
    int bias = size == 32 ? BIAS_EXTQF32 : BIAS_QF16;
    int shift;
    uint64_t integer, discarded, mask;
    bool half, sticky, increment;
    uint64_t quarters, rq1, even;
    int64_t sig;
    LREQ_t lreq = 0;

    if (parts.cls == float_class_zero) {
        exp = emin + bias;
        if (size == 32) {
            return (uint64_t)(exp & 0xff) << 4 |
                   (((exp >> 8) & 1) << 1) | 7;
        }
        return (uint64_t)(exp & 0x1f) << 2 | 3;
    }
    if (parts.exp - exp >= 1) {
        exp++;
    }
    if (exp > emax ||
        (exp == emax && parts.frac > (UINT64_C(1) << 63))) {
        return qfloat_ext_overflow(size, parts.sign, mode);
    }
    if (exp < emin) {
        return qfloat_ext_underflow(size, parts.sign, mode);
    }

    shift = 63 - precision - (parts.exp - exp);
    if (shift > 64) {
        integer = 0;
        half = false;
        sticky = true;
    } else if (shift == 64) {
        integer = 0;
        half = parts.frac >> 63;
        sticky = parts.frac << 1;
    } else if (shift > 0) {
        integer = parts.frac >> shift;
        mask = (UINT64_C(1) << shift) - 1;
        discarded = parts.frac & mask;
        half = discarded & (UINT64_C(1) << (shift - 1));
        sticky = discarded & ((UINT64_C(1) << (shift - 1)) - 1);
    } else {
        integer = parts.frac << -shift;
        half = false;
        sticky = false;
    }
    quarters = integer * 4 + (half ? 2 : 0) + (sticky ? 1 : 0);

    switch (mode) {
    case RND_TO_NEAREST_EVEN:
        increment = (quarters & 3) >= ((quarters >> 2 & 1) ? 2 : 3);
        break;
    case RND_TO_ZERO:
        increment = false;
        break;
    case RND_TOWARDS_NEG_INF:
        increment = parts.sign && (quarters & 3);
        break;
    case RND_TOWARDS_POS_INF:
        increment = !parts.sign && (quarters & 3);
        break;
    default:
        g_assert_not_reached();
    }

    rq1 = (quarters >> 2) * 8 + increment * 4 +
          (increment ^ ((quarters & 3) != 0)) * 2 + 1;
    even = (rq1 >> 3) & ~UINT64_C(1);
    rq1 -= even * 8 + 1;
    sig = even >> 1;
    lreq |= (rq1 >> 3) ? LREQ_L : 0;
    lreq |= ((rq1 >> 2) & 1) ? LREQ_R : 0;
    lreq |= ((rq1 >> 1) & 1) ? LREQ_Q : 0;

    if (parts.sign) {
        sig = ~sig;
        lreq = negate_lreq(lreq);
    }
    if (deferred_sign) {
        lreq = negate_lreq(lreq);
    }

    return qfloat_pack_ext(size, sig, exp + bias, lreq);
}

/*
 * NaN/Inf input behavior
 */

uint64_t handle_infinity_nan_add(unfloat u, unfloat v, uint64_t pos_nan_val,
                                 uint64_t neg_nan_val, uint64_t pos_inf_val,
                                 uint64_t neg_inf_val)
{
    bool u_neg = is_unfloat_neg(u);
    bool v_neg = is_unfloat_neg(v);

    if (u.nan || v.nan) {
        /* +NaN has the highest priority */
        if ((!u_neg && u.nan) || (!v_neg && v.nan)) {
            return pos_nan_val;
        }
        return neg_nan_val;
    }

    /* Infinities of opposing sign added together produce a NaN */
    if (u.inf && v.inf && (u_neg ^ v_neg)) {
        return pos_nan_val;
    }
    if ((u_neg && u.inf) || (v_neg && v.inf)) {
        return neg_inf_val;
    }
    return pos_inf_val;
}

uint64_t handle_infinity_nan_mpy(unfloat u, unfloat v, uint64_t pos_nan_val,
                                 uint64_t neg_nan_val, uint64_t pos_inf_val,
                                 uint64_t neg_inf_val)
{
    bool u_neg = is_unfloat_neg(u);
    bool v_neg = is_unfloat_neg(v);

    if (u.nan || v.nan) {
        return (u_neg ^ v_neg) ? neg_nan_val : pos_nan_val;
    }
    /* Implicitly, one of the operands is infinite: 0 * inf is a NaN */
    if (u.zero || v.zero) {
        return (u_neg ^ v_neg) ? neg_nan_val : pos_nan_val;
    }
    return (u_neg ^ v_neg) ? neg_inf_val : pos_inf_val;
}

/*
 * Parse SF/HF/QF16/QF32 into an unfloat
 */

unfloat parse_hf(int16_t in)
{
    INIT_UNFLOAT(out)
    float_status status = { 0 };
    uint16_t raw = in;

    out.exp = (raw >> hf_MANTBITS) & 0x1f;
    out.exp -= BIAS_HF;
    if (out.exp < E_MIN_HF) {
        out.exp = E_MIN_HF;
    }

    out.sign = raw >> 15;
    out.ieee = true;
    out.lfrac = hf_MANTBITS;
    out.lsig = (raw & 0x3ff) | (((raw >> hf_MANTBITS) & 0x1f) ? 0x400 : 0);
    out.parts = float16_unpack_canonical(make_float16(in), &status);
    out.inf = out.parts.cls == float_class_inf;
    out.nan = out.parts.cls == float_class_qnan ||
              out.parts.cls == float_class_snan;
    out.zero = out.parts.cls == float_class_zero;
    out.parts.sign = false;
    return out;
}

static unfloat parse_sf(int32_t in)
{
    INIT_UNFLOAT(out)
    float_status status = { 0 };
    uint32_t raw = in;

    out.exp = (raw >> sf_MANTBITS) & 0xff;
    out.exp -= BIAS_SF;
    if (out.exp < E_MIN_SF) {
        out.exp = E_MIN_SF;
    }

    out.sign = raw >> 31;
    out.ieee = true;
    out.lfrac = sf_MANTBITS;
    out.lsig = (raw & 0x7fffff) |
               (((raw >> sf_MANTBITS) & 0xff) ? 0x800000 : 0);
    out.parts = float32_unpack_canonical(make_float32(in), &status);
    out.inf = out.parts.cls == float_class_inf;
    out.nan = out.parts.cls == float_class_qnan ||
              out.parts.cls == float_class_snan;
    out.zero = out.parts.cls == float_class_zero;
    out.parts.sign = false;
    return out;
}

unfloat parse_sf_daz(uint32_t in, bool daz_mode)
{
    unfloat a = parse_sf(in);

    if (daz_mode && (in & 0x7f800000) == 0 && (in & 0x007fffff)) {
        a.zero = true;
        a.lsig = 0;
        a.parts = (FloatParts64) { .cls = float_class_zero };
    }
    return a;
}

unfloat parse_extqf16(uint16_t in, uint8_t in_ext)
{
    INIT_UNFLOAT(out)
    LREQ_t lreq = (in_ext & EXT16_BITMASK) << 2;
    uint32_t raw = ((uint32_t)in << 4) | (in_ext & EXT16_BITMASK);

    out.exp = (in & 0x1f) - BIAS_QF16;
    out.sign = !!(lreq & LREQ_R);
    out.inf = is_extqf16_inf(raw);
    out.nan = is_extqf16_nan(raw);
    out.parts = qfloat_unpack_ext_parts(16, in, in_ext);
    out.parts.sign ^= out.sign;
    out.zero = out.parts.cls == float_class_zero;
    return out;
}

unfloat parse_extqf32(uint32_t in, uint8_t in_ext)
{
    INIT_UNFLOAT(out)
    LREQ_t lreq = in_ext & EXT32_BITMASK;
    uint64_t raw = ((uint64_t)in << 4) | (in_ext & EXT32_BITMASK);
    int exponent = (in & 0xff) | (!!(lreq & LREQ_E) << 8);

    /* First step, sub 128 to get to IEEE-like; then sub 255 for qf32 bias */
    out.exp = ((exponent - 128) & 0x1ff) - 255;
    out.sign = !!(lreq & LREQ_R);
    out.inf = is_extqf32_inf(raw);
    out.nan = is_extqf32_nan(raw);
    out.parts = qfloat_unpack_ext_parts(32, in, in_ext);
    out.parts.sign ^= out.sign;
    out.zero = out.parts.cls == float_class_zero;
    return out;
}

/*
 * Convert QF16/QF32 to HF/SF/BF
 */

static FloatRoundMode qfrnd_to_float_round(qfrnd_mode_enum_t qfrnd)
{
    switch (qfrnd) {
    case RND_TO_NEAREST_EVEN:
        return float_round_nearest_even;
    case RND_TO_ZERO:
        return float_round_to_zero;
    case RND_TOWARDS_NEG_INF:
        return float_round_down;
    case RND_TOWARDS_POS_INF:
        return float_round_up;
    default:
        g_assert_not_reached();
    }
}

int32_t conv_sf_extqf32(uint32_t a, uint8_t a_ext, qfrnd_mode_enum_t qfrnd)
{
    uint64_t raw = ((uint64_t)a << 4) | (a_ext & EXT32_BITMASK);
    FloatParts64 parts;
    float_status status = { 0 };

    if (is_extqf32_nan(raw)) {
        return raw == extqf32_neg_nan || raw == extqf32_neg_nan_inexact ?
               ieee_pos_NaN_32 | 0x80000000 : ieee_pos_NaN_32;
    }
    if (is_extqf32_inf(raw)) {
        return raw == extqf32_neg_inf_exact ||
               raw == extqf32_neg_inf_inexact ?
               ieee_pos_inf_32 | 0x80000000 : ieee_pos_inf_32;
    }
    set_float_rounding_mode(qfrnd_to_float_round(qfrnd), &status);
    parts = qfloat_unpack_ext_parts(32, a, a_ext);
    qfloat_apply_inexact(&parts, a_ext);
    return float32_val(float32_round_pack_canonical(&parts, &status));
}

int16_t conv_hf_extqf32(uint32_t a, uint8_t a_ext, qfrnd_mode_enum_t qfrnd)
{
    uint64_t raw = ((uint64_t)a << 4) | (a_ext & EXT32_BITMASK);
    FloatParts64 parts;
    float_status status = { 0 };

    if (is_extqf32_nan(raw)) {
        return (int16_t)(raw == extqf32_neg_nan ||
                         raw == extqf32_neg_nan_inexact ?
                         ieee_pos_NaN_16 | 0x8000 : ieee_pos_NaN_16);
    }
    if (is_extqf32_inf(raw)) {
        return (int16_t)(raw == extqf32_neg_inf_exact ||
                         raw == extqf32_neg_inf_inexact ?
                         ieee_pos_inf_16 | 0x8000 : ieee_pos_inf_16);
    }
    set_float_rounding_mode(qfrnd_to_float_round(qfrnd), &status);
    parts = qfloat_unpack_ext_parts(32, a, a_ext);
    qfloat_apply_inexact(&parts, a_ext);
    return float16_val(float16_round_pack_canonical(&parts, &status));
}

int16_t conv_hf_extqf16(uint16_t a, uint8_t a_ext, qfrnd_mode_enum_t qfrnd)
{
    uint32_t raw = ((uint32_t)a << 4) | (a_ext & EXT16_BITMASK);
    FloatParts64 parts;
    float_status status = { 0 };

    if (is_extqf16_nan(raw)) {
        return (int16_t)(raw == extqf16_neg_nan ?
                         ieee_pos_NaN_16 | 0x8000 : ieee_pos_NaN_16);
    }
    if (is_extqf16_inf(raw)) {
        return (int16_t)(raw == extqf16_neg_inf ?
                         ieee_pos_inf_16 | 0x8000 : ieee_pos_inf_16);
    }
    set_float_rounding_mode(qfrnd_to_float_round(qfrnd), &status);
    parts = qfloat_unpack_ext_parts(16, a, a_ext);
    return float16_val(float16_round_pack_canonical(&parts, &status));
}

uint16_t conv_qf32_to_bf(uint32_t a, uint8_t a_ext, qfrnd_mode_enum_t qfrnd)
{
    uint64_t raw = ((uint64_t)a << 4) | (a_ext & EXT32_BITMASK);
    FloatParts64 parts;
    float_status status = { 0 };

    if (is_extqf32_nan(raw)) {
        return raw == extqf32_neg_nan || raw == extqf32_neg_nan_inexact ?
               BF_POS_NAN | 0x8000 : BF_POS_NAN;
    }
    if (is_extqf32_inf(raw)) {
        return raw == extqf32_neg_inf_exact ||
               raw == extqf32_neg_inf_inexact ? BF_POS_INF | 0x8000 :
                                                BF_POS_INF;
    }
    set_float_rounding_mode(qfrnd_to_float_round(qfrnd), &status);
    parts = qfloat_unpack_ext_parts(32, a, a_ext);
    qfloat_apply_inexact(&parts, a_ext);
    return bfloat16_round_pack_canonical(&parts, &status);
}

int qf_vilog2(f_type type, uint32_t a, uint8_t a_ext)
{
    unfloat u = { 0 };

    if (type == EXTQF32) {
        uint64_t raw = ((uint64_t)a << 4) | (a_ext & EXT32_BITMASK);
        FloatParts64 p;

        if (is_extqf32_nan(raw)) {
            return QF32_ILOG2_NAN_EXP;
        }
        if (is_extqf32_inf(raw)) {
            return QF32_ILOG2_INF_EXP;
        }
        p = qfloat_unpack_ext_parts(32, a, a_ext);
        return p.cls == float_class_zero ? QF32_ILOG2_ZERO_EXP : p.exp;
    }
    if (type == EXTQF16) {
        uint32_t raw = (a << 4) | (a_ext & EXT16_BITMASK);
        FloatParts64 p;

        if (is_extqf16_nan(raw)) {
            return QF16_ILOG2_NAN_EXP;
        }
        if (is_extqf16_inf(raw)) {
            return QF16_ILOG2_INF_EXP;
        }
        p = qfloat_unpack_ext_parts(16, a, a_ext);
        return p.cls == float_class_zero ? QF16_ILOG2_ZERO_EXP : p.exp;
    }

    switch (type) {
    case SF:
        u = parse_sf(a);
        break;
    case HF:
        u = parse_hf(a);
        break;
    default:
        g_assert_not_reached();
    }

    if (type == SF) {
        if (u.zero) {
            return QF32_ILOG2_ZERO_EXP;
        } else if (u.nan) {
            return QF32_ILOG2_NAN_EXP;
        } else if (u.inf) {
            return QF32_ILOG2_INF_EXP;
        }
    } else {
        if (u.zero) {
            return QF16_ILOG2_ZERO_EXP;
        } else if (u.nan) {
            return QF16_ILOG2_NAN_EXP;
        } else if (u.inf) {
            return QF16_ILOG2_INF_EXP;
        }
    }
    return u.parts.exp;
}

/*
 * USR field accessors
 */

static uint32_t get_usr_field(const CPUHexagonState *env, int field)
{
    return extract32(env->gpr[HEX_REG_USR], reg_field_info[field].offset,
                      reg_field_info[field].width);
}

bool is_daz_mode(const CPUHexagonState *env)
{
    const HexagonCPU *cpu = env_archcpu(env);

    return cpu->cfg.hex_def->hex_version >= HEX_VER_V81 &&
           get_usr_field(env, USR_FPDAZ);
}

bool qfloat_is_extended(const CPUHexagonState *env)
{
    const HexagonCPU *cpu = env_archcpu(env);

    return cpu->cfg.hex_def->hex_version >= HEX_VER_V79;
}

unfloat legacy_parse_qf32(int32_t in)
{
    unfloat out = { 0 };
    int32_t signif;
    uint32_t magnitude;

    out.sign = (uint32_t)in >> 31;
    out.exp = (in & 0xff) - BIAS_QF32;
    signif = sextract32(in, 7, 25) | 1;
    magnitude = signif < 0 ? -(uint32_t)signif : signif;
    out.lsig = magnitude;
    out.lfrac = sf_MANTBITS;
    out.parts = (FloatParts64) {
        .cls = float_class_normal,
        .exp = out.exp + 31 - clz32(magnitude) - sf_MANTBITS,
        .frac = (uint64_t)magnitude << (32 + clz32(magnitude)),
    };
    return out;
}

unfloat legacy_parse_qf16(int16_t in)
{
    unfloat out = { 0 };
    int16_t signif;
    uint16_t magnitude;

    out.sign = (uint16_t)in >> 15;
    out.exp = (in & 0x1f) - BIAS_QF16;
    signif = sextract32((uint16_t)in, 4, 12) | 1;
    magnitude = signif < 0 ? -(uint16_t)signif : signif;
    out.lsig = magnitude;
    out.lfrac = hf_MANTBITS;
    out.parts = (FloatParts64) {
        .cls = float_class_normal,
        .exp = out.exp + 31 - clz32(magnitude) - hf_MANTBITS,
        .frac = (uint64_t)magnitude << (32 + clz32(magnitude)),
    };
    return out;
}

/*
 * The pre-v79 QFloat add, multiply and conversion to IEEE.  They work on
 * the integer significand of each operand as it is encoded, and use no
 * host floating point: the exact result is a fixed point integer, and what
 * is too small to be in it only has its sign kept.
 *
 * The result of an add or multiply is the exact result rounded to a QF
 * significand whose low bit is always set, and saturated.  The significand
 * is a signed (two's complement) quantity for QF operands.  IEEE operands
 * are handled as magnitudes and, when the result is negative only because
 * of their signs, the result is negated afterwards by taking the one's
 * complement of the significand.  IEEE infinities and NaNs are ordinary
 * numbers with the maximum exponent, which saturate.
 */

/* Fraction bits of a fixed point value beyond those of the QF significand. */
#define LEGACY_EXTRA_BITS 32

/* The fixed point value of an exact result, and the sign of what is below it */
typedef struct LegacyFixed {
    int64_t v;
    int low;
} LegacyFixed;

static int legacy_ilog2(uint32_t x)
{
    return 63 - clz64(x);
}

/*
 * The value lsig * 2^(shift - lfrac), negated if neg, as fixed point with
 * fb fraction bits.  Bits that do not fit are not in v, and low is their
 * sign.  The magnitude of the value is less than 4.
 */
static LegacyFixed legacy_align(uint32_t lsig, int lfrac, int shift, int fb,
                                bool neg)
{
    int pos = fb + shift - lfrac;
    uint64_t mag;
    bool inexact = false;

    if (pos >= 0) {
        mag = (uint64_t)lsig << pos;
    } else if (-pos >= 32) {
        mag = 0;
        inexact = lsig != 0;
    } else {
        mag = lsig >> -pos;
        inexact = (lsig & MAKE_64BIT_MASK(0, -pos)) != 0;
    }
    return (LegacyFixed) {
        .v = neg ? -(int64_t)mag : (int64_t)mag,
        .low = inexact ? (neg ? -1 : 1) : 0,
    };
}

/* The QF encoding of the magnitude significand's one's complement. */
static uint32_t legacy_qf_negate(int size, uint32_t qf)
{
    if (size == 32) {
        return (~(qf >> 8) << 8) | (qf & 0xff);
    }
    return ((~(qf >> 5) << 5) & 0xffe0) | (qf & 0x1f);
}

/*
 * Round and saturate (x.v + x.low * epsilon) * 2^exp, where x.v has frac +
 * LEGACY_EXTRA_BITS fraction bits.  The low sign only breaks ties.
 */
static uint32_t legacy_rnd_sat_qf(int size, int exp, LegacyFixed x)
{
    const bool is32 = size == 32;
    const int e_min = is32 ? -127 : E_MIN_QF16;
    const int e_max = is32 ? 128 : E_MAX_QF16;
    const int bias = is32 ? BIAS_QF32 : BIAS_QF16;
    const int frac = is32 ? sf_MANTBITS : hf_MANTBITS;
    const int fb = frac + LEGACY_EXTRA_BITS;
    const uint32_t mask = is32 ? 0x7fffff : 0x3ff;
    const int64_t two = INT64_C(2) << fb;
    const bool sign = x.v < 0 || (x.v == 0 && x.low < 0);
    const bool prod_ovf = x.v > two || (x.v == two && x.low >= 0) ||
                          x.v < -two;
    /*
     * The result is rounded to the odd multiples of 2^-frac.  Work out
     * which, from the quotient and remainder of the value by four of them,
     * after halving the value where the result has one more bit.
     */
    const bool halve = exp == e_min - 1 || (prod_ovf && exp < e_max);
    const int sh = LEGACY_EXTRA_BITS + halve;
    const int64_t q = x.v >> (sh + 2);
    const int64_t rem = x.v & ((INT64_C(4) << sh) - 1);
    const int64_t unit = INT64_C(1) << sh;
    int exp_adj = 0;
    int64_t u;
    uint32_t sig_out;

    if (exp >= e_max + 1 || (prod_ovf && exp == e_max)) {
        sig_out = (sign - 1) & mask;
        exp = e_max + bias;
        goto pack;
    }
    if (exp <= e_min - 2) {
        sig_out = -sign & mask;
        exp = e_min + bias;
        goto pack;
    }

    if (exp == e_max || exp == e_min - 1 || prod_ovf) {
        /* The edges of the range, and an overflowing product, keep the bit. */
        exp_adj = exp != e_max;
        if (rem == 0 && x.low < 0) {
            u = 4 * q - 1;
        } else if (rem < 2 * unit || (rem == 2 * unit && x.low <= 0)) {
            u = 4 * q + 1;
        } else {
            u = 4 * q + 3;
        }
    } else if (rem == 0 && x.low < 0) {
        u = 4 * q - 1;
    } else if (rem < unit + unit / 2 ||
               (rem == unit + unit / 2 && x.low <= 0)) {
        u = 4 * q + 1;
    } else if (rem < 2 * unit + unit / 2 ||
               (rem == 2 * unit + unit / 2 && x.low <= 0)) {
        u = 2 * q + 1;
        exp_adj = 1;
    } else {
        u = 4 * q + 3;
    }

    sig_out = ((u < 0 ? -u : u) >> 1) & mask;
    if (sign) {
        sig_out = ~sig_out;
    }
    exp += bias + exp_adj;

pack:
    sig_out |= (uint32_t)sign << frac;
    if (x.v == 0 && x.low == 0) {
        /* An exact zero has the exponent of zero. */
        exp = 0;
    }
    return is32 ? (sig_out << 8) | (exp & 0xff) :
                  ((sig_out << 5) & 0xffe0) | (exp & 0x1f);
}

uint32_t legacy_qf_add(int size, unfloat a, unfloat b)
{
    const int frac = size == 32 ? sf_MANTBITS : hf_MANTBITS;
    const int fb = frac + LEGACY_EXTRA_BITS;
    bool neg_a = a.sign;
    bool neg_b = b.sign;
    bool negate = false;
    LegacyFixed x, y;
    int exp;
    uint32_t result;

    /* Only IEEE operands that are both negative defer their negation. */
    if (a.ieee && b.ieee && a.sign && b.sign) {
        negate = true;
        neg_a = neg_b = false;
    }

    /* The exponent of the larger operand, or of a zero, less its fraction. */
    if (a.exp > b.exp) {
        exp = a.exp + (a.lsig ? legacy_ilog2(a.lsig) - a.lfrac : -(frac + 1));
        exp = MAX(exp, b.exp);
    } else {
        exp = b.exp + (b.lsig ? legacy_ilog2(b.lsig) - b.lfrac : -(frac + 1));
        exp = MAX(exp, a.exp);
    }

    x = legacy_align(a.lsig, a.lfrac, a.exp - exp, fb, neg_a);
    y = legacy_align(b.lsig, b.lfrac, b.exp - exp, fb, neg_b);
    x.v += y.v;
    x.low += y.low;
    x.low = (x.low > 0) - (x.low < 0);

    result = legacy_rnd_sat_qf(size, exp, x);
    return negate ? legacy_qf_negate(size, result) : result;
}

uint32_t legacy_qf_mpy(int size, unfloat a, unfloat b)
{
    const int frac = size == 32 ? sf_MANTBITS : hf_MANTBITS;
    const int fb = frac + LEGACY_EXTRA_BITS;
    bool neg = false;
    bool neg_prod = false;
    uint64_t prod = (uint64_t)a.lsig * b.lsig;
    int pos = fb - a.lfrac - b.lfrac;
    LegacyFixed x = { };
    uint32_t result;

    /* IEEE signs negate the result; QF signs are part of the significand. */
    if (a.ieee) {
        neg ^= a.sign;
    } else {
        neg_prod ^= a.sign;
    }
    if (b.ieee) {
        neg ^= b.sign;
    } else {
        neg_prod ^= b.sign;
    }

    /* The product has at most 52 bits and the result is less than 4. */
    g_assert(pos >= 0);
    x.v = prod << pos;
    if (neg_prod) {
        x.v = -x.v;
    }

    result = legacy_rnd_sat_qf(size, a.exp + b.exp, x);
    return neg ? legacy_qf_negate(size, result) : result;
}

/*
 * Conversion of a legacy QF value to IEEE.  The result is rounded to nearest
 * even.  The exponent range goes one binade past IEEE's, so that the largest
 * finite results are encoded as infinities and NaNs, and values at the
 * saturation limit give an all-ones significand.
 *
 * The value is lsig * 2^(exp - lfrac).  Return the significand of the
 * result, and in *exp its exponent.
 */
static uint32_t legacy_ieee_sig(int *exp, uint32_t lsig, int lfrac,
                                bool is_sf)
{
    const int e_min = is_sf ? E_MIN_SF : E_MIN_HF;
    const int e_max = is_sf ? E_MAX_SF : E_MAX_HF;
    const int mant = is_sf ? sf_MANTBITS : hf_MANTBITS;
    const int dshift = 52 - mant;
    const uint32_t mask = (1u << mant) - 1;
    const int exp_org = *exp;
    const int lg = lsig ? legacy_ilog2(lsig) : 0;
    /* The value as a 53 bit significand, with the leading bit set. */
    uint64_t sig_53 = lsig ? (uint64_t)lsig << (52 - lg) : 0;
    int exp_d = lsig ? exp_org + lg - lfrac : -1023;
    uint64_t rem;
    uint32_t sig_32;
    bool saturate;
    int shift;

    /* The value is at least the largest one that is not saturated. */
    saturate = exp_d > e_max ||
               (exp_d == e_max && (sig_53 >> dshift) == (2u << mant) - 1);

    /* Denormal results are shifted into place. */
    shift = e_min - exp_d;
    if (shift >= 53) {
        sig_53 = 0;
    } else if (shift > 0) {
        sig_53 >>= shift;
    }

    sig_32 = (sig_53 >> dshift) & mask;
    rem = sig_53 & ((UINT64_C(2) << dshift) - 1);

    if (saturate) {
        sig_32 = mask;
    } else if ((rem > (UINT64_C(1) << (dshift - 1)) &&
                rem < (UINT64_C(1) << dshift)) ||
               rem >= (UINT64_C(3) << (dshift - 1))) {
        if (sig_32 == mask) {
            sig_32 = 0;
            exp_d++;
        } else {
            sig_32++;
        }
    }

    if (lsig == 0 && exp_org == e_min - 1) {
        exp_d = 0;
        sig_32 = 0;
    }
    *exp = exp_d;
    return sig_32;
}

static uint32_t legacy_rnd_sat_ieee(bool is_sf, unfloat u)
{
    const bool sign = u.sign;
    int exp = u.exp;
    const uint32_t sig_32 = legacy_ieee_sig(&exp, u.lsig, u.lfrac, is_sf);

    if (is_sf) {
        if (exp == 0 && u.lsig == 0) {
            return 0;
        }
        exp = MIN(MAX(exp + BIAS_SF, 0), 255);
        return ((uint32_t)sign << 31) | (exp << sf_MANTBITS) | sig_32;
    }
    if (exp == E_MIN_HF - 1 && u.lsig == 0) {
        return 0;
    }
    if (exp > E_MAX_HF) {
        return ((uint32_t)sign << 15) | (0x1f << hf_MANTBITS) | 0x3ff;
    }
    exp = MIN(MAX(exp + BIAS_HF, 0), 31);
    return ((uint32_t)sign << 15) | (exp << hf_MANTBITS) | sig_32;
}

int32_t legacy_conv_sf_qf32(uint32_t a)
{
    return legacy_rnd_sat_ieee(true, legacy_parse_qf32(a));
}

int16_t legacy_conv_hf_qf32(uint32_t a)
{
    return legacy_rnd_sat_ieee(false, legacy_parse_qf32(a));
}

int16_t legacy_conv_hf_qf16(uint16_t a)
{
    return legacy_rnd_sat_ieee(false, legacy_parse_qf16(a));
}
