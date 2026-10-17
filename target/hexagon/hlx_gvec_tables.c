/*
 * HLX GVecGen3/GVecGen2 descriptor tables for inline TCG IR generation.
 *
 * saturating add/sub and logical ops.
 * min/max (smin/umin/smax/umax) and abs.
 *
 * Design: the canonical GVecGen3 descriptors for ssadd/sssub/usadd/ussub,
 * and the GVecGen3 descriptors for smin/smax/umin/umax, and the GVecGen2
 * descriptor for abs, live as static locals in tcg-op-gvec.c.
 * Rather than exporting them from TCG core, we duplicate the small set we
 * need here (Option 1 from the plan).  The fniv pointers (tcg_gen_ssadd_vec,
 * tcg_gen_smin_vec, tcg_gen_abs_vec, etc.) are already public.
 * The fni8 helper for abs at MO_16 (tcg_gen_vec_abs16_i64) is static in
 * tcg-op-gvec.c, so we provide our own copy here.
 *
 * Copyright (c) 2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "tcg/tcg.h"
#include "tcg/tcg-temp-internal.h"
#include "tcg/tcg-op-common.h"
#include "tcg/tcg-op-gvec-common.h"
#include "hlx_gvec_tables.h"
#include "exec/helper-gen.h"  /* gen_helper_hlx_gvec_ushsat_{add,sub}16 etc. */

/* -----------------------------------------------------------------------
 * Local copies of static helpers from tcg-op-gvec.c
 * ----------------------------------------------------------------------- */

static void hlx_usadd_i32(TCGv_i32 d, TCGv_i32 a, TCGv_i32 b)
{
    TCGv_i32 max = tcg_constant_i32(-1);
    tcg_gen_add_i32(d, a, b);
    tcg_gen_movcond_i32(TCG_COND_LTU, d, d, a, max, d);
}

static void hlx_usadd_i64(TCGv_i64 d, TCGv_i64 a, TCGv_i64 b)
{
    TCGv_i64 max = tcg_constant_i64(-1);
    tcg_gen_add_i64(d, a, b);
    tcg_gen_movcond_i64(TCG_COND_LTU, d, d, a, max, d);
}

static void hlx_ussub_i32(TCGv_i32 d, TCGv_i32 a, TCGv_i32 b)
{
    TCGv_i32 min = tcg_constant_i32(0);
    tcg_gen_sub_i32(d, a, b);
    tcg_gen_movcond_i32(TCG_COND_LTU, d, a, b, min, d);
}

static void hlx_ussub_i64(TCGv_i64 d, TCGv_i64 a, TCGv_i64 b)
{
    TCGv_i64 min = tcg_constant_i64(0);
    tcg_gen_sub_i64(d, a, b);
    tcg_gen_movcond_i64(TCG_COND_LTU, d, a, b, min, d);
}

/* -----------------------------------------------------------------------
 * Saturating add descriptors (ssadd - signed)
 * ----------------------------------------------------------------------- */

static const TCGOpcode hlx_vecop_ssadd[] = { INDEX_op_ssadd_vec, 0 };

const GVecGen3 hlx_ssadd_op[4] = {
    { .fniv    = tcg_gen_ssadd_vec,
      .fno     = gen_helper_gvec_ssadd8,
      .opt_opc = hlx_vecop_ssadd,
      .vece    = MO_8 },
    { .fniv    = tcg_gen_ssadd_vec,
      .fno     = gen_helper_gvec_ssadd16,
      .opt_opc = hlx_vecop_ssadd,
      .vece    = MO_16 },
    { .fniv    = tcg_gen_ssadd_vec,
      .fno     = gen_helper_gvec_ssadd32,
      .opt_opc = hlx_vecop_ssadd,
      .vece    = MO_32 },
    { .fniv    = tcg_gen_ssadd_vec,
      .fno     = gen_helper_gvec_ssadd64,
      .opt_opc = hlx_vecop_ssadd,
      .vece    = MO_64 },
};

/* -----------------------------------------------------------------------
 * Saturating sub descriptors (sssub - signed)
 * ----------------------------------------------------------------------- */

static const TCGOpcode hlx_vecop_sssub[] = { INDEX_op_sssub_vec, 0 };

const GVecGen3 hlx_sssub_op[4] = {
    { .fniv    = tcg_gen_sssub_vec,
      .fno     = gen_helper_gvec_sssub8,
      .opt_opc = hlx_vecop_sssub,
      .vece    = MO_8 },
    { .fniv    = tcg_gen_sssub_vec,
      .fno     = gen_helper_gvec_sssub16,
      .opt_opc = hlx_vecop_sssub,
      .vece    = MO_16 },
    { .fniv    = tcg_gen_sssub_vec,
      .fno     = gen_helper_gvec_sssub32,
      .opt_opc = hlx_vecop_sssub,
      .vece    = MO_32 },
    { .fniv    = tcg_gen_sssub_vec,
      .fno     = gen_helper_gvec_sssub64,
      .opt_opc = hlx_vecop_sssub,
      .vece    = MO_64 },
};

/* -----------------------------------------------------------------------
 * Saturating add descriptors (usadd - unsigned)
 * ----------------------------------------------------------------------- */

static const TCGOpcode hlx_vecop_usadd[] = { INDEX_op_usadd_vec, 0 };

const GVecGen3 hlx_usadd_op[4] = {
    { .fniv    = tcg_gen_usadd_vec,
      .fno     = gen_helper_gvec_usadd8,
      .opt_opc = hlx_vecop_usadd,
      .vece    = MO_8 },
    { .fniv    = tcg_gen_usadd_vec,
      .fno     = gen_helper_gvec_usadd16,
      .opt_opc = hlx_vecop_usadd,
      .vece    = MO_16 },
    { .fni4    = hlx_usadd_i32,
      .fniv    = tcg_gen_usadd_vec,
      .fno     = gen_helper_gvec_usadd32,
      .opt_opc = hlx_vecop_usadd,
      .vece    = MO_32 },
    { .fni8    = hlx_usadd_i64,
      .fniv    = tcg_gen_usadd_vec,
      .fno     = gen_helper_gvec_usadd64,
      .opt_opc = hlx_vecop_usadd,
      .vece    = MO_64 },
};

/* -----------------------------------------------------------------------
 * Saturating sub descriptors (ussub - unsigned)
 * ----------------------------------------------------------------------- */

static const TCGOpcode hlx_vecop_ussub[] = { INDEX_op_ussub_vec, 0 };

const GVecGen3 hlx_ussub_op[4] = {
    { .fniv    = tcg_gen_ussub_vec,
      .fno     = gen_helper_gvec_ussub8,
      .opt_opc = hlx_vecop_ussub,
      .vece    = MO_8 },
    { .fniv    = tcg_gen_ussub_vec,
      .fno     = gen_helper_gvec_ussub16,
      .opt_opc = hlx_vecop_ussub,
      .vece    = MO_16 },
    { .fni4    = hlx_ussub_i32,
      .fniv    = tcg_gen_ussub_vec,
      .fno     = gen_helper_gvec_ussub32,
      .opt_opc = hlx_vecop_ussub,
      .vece    = MO_32 },
    { .fni8    = hlx_ussub_i64,
      .fniv    = tcg_gen_ussub_vec,
      .fno     = gen_helper_gvec_ussub64,
      .opt_opc = hlx_vecop_ussub,
      .vece    = MO_64 },
};

/* -----------------------------------------------------------------------
 * Logical op descriptors (vece-independent, prefer 64-bit scalar)
 * ----------------------------------------------------------------------- */

const GVecGen3 hlx_and_op = {
    .fni8      = tcg_gen_and_i64,
    .fniv      = tcg_gen_and_vec,
    .fno       = gen_helper_gvec_and,
    .prefer_i64 = true,
};

const GVecGen3 hlx_or_op = {
    .fni8      = tcg_gen_or_i64,
    .fniv      = tcg_gen_or_vec,
    .fno       = gen_helper_gvec_or,
    .prefer_i64 = true,
};

const GVecGen3 hlx_xor_op = {
    .fni8      = tcg_gen_xor_i64,
    .fniv      = tcg_gen_xor_vec,
    .fno       = gen_helper_gvec_xor,
    .prefer_i64 = true,
};

/* andc(a, b) = a & ~b - maps to LX_andn semantics */
const GVecGen3 hlx_andc_op = {
    .fni8      = tcg_gen_andc_i64,
    .fniv      = tcg_gen_andc_vec,
    .fno       = gen_helper_gvec_andc,
    .prefer_i64 = true,
};

/* orc(a, b) = a | ~b - maps to LX_orn semantics */
const GVecGen3 hlx_orc_op = {
    .fni8      = tcg_gen_orc_i64,
    .fniv      = tcg_gen_orc_vec,
    .fno       = gen_helper_gvec_orc,
    .prefer_i64 = true,
};

/* -----------------------------------------------------------------------
 * min/max descriptor arrays (GVecGen3[4])
 * ----------------------------------------------------------------------- */

/* smin: signed minimum */
static const TCGOpcode hlx_vecop_smin[] = { INDEX_op_smin_vec, 0 };

const GVecGen3 hlx_smin_op[4] = {
    { .fniv    = tcg_gen_smin_vec,
      .fno     = gen_helper_gvec_smin8,
      .opt_opc = hlx_vecop_smin,
      .vece    = MO_8 },
    { .fniv    = tcg_gen_smin_vec,
      .fno     = gen_helper_gvec_smin16,
      .opt_opc = hlx_vecop_smin,
      .vece    = MO_16 },
    { .fni4    = tcg_gen_smin_i32,
      .fniv    = tcg_gen_smin_vec,
      .fno     = gen_helper_gvec_smin32,
      .opt_opc = hlx_vecop_smin,
      .vece    = MO_32 },
    { .fni8    = tcg_gen_smin_i64,
      .fniv    = tcg_gen_smin_vec,
      .fno     = gen_helper_gvec_smin64,
      .opt_opc = hlx_vecop_smin,
      .vece    = MO_64 },
};

/* umin: unsigned minimum */
static const TCGOpcode hlx_vecop_umin[] = { INDEX_op_umin_vec, 0 };

const GVecGen3 hlx_umin_op[4] = {
    { .fniv    = tcg_gen_umin_vec,
      .fno     = gen_helper_gvec_umin8,
      .opt_opc = hlx_vecop_umin,
      .vece    = MO_8 },
    { .fniv    = tcg_gen_umin_vec,
      .fno     = gen_helper_gvec_umin16,
      .opt_opc = hlx_vecop_umin,
      .vece    = MO_16 },
    { .fni4    = tcg_gen_umin_i32,
      .fniv    = tcg_gen_umin_vec,
      .fno     = gen_helper_gvec_umin32,
      .opt_opc = hlx_vecop_umin,
      .vece    = MO_32 },
    { .fni8    = tcg_gen_umin_i64,
      .fniv    = tcg_gen_umin_vec,
      .fno     = gen_helper_gvec_umin64,
      .opt_opc = hlx_vecop_umin,
      .vece    = MO_64 },
};

/* smax: signed maximum */
static const TCGOpcode hlx_vecop_smax[] = { INDEX_op_smax_vec, 0 };

const GVecGen3 hlx_smax_op[4] = {
    { .fniv    = tcg_gen_smax_vec,
      .fno     = gen_helper_gvec_smax8,
      .opt_opc = hlx_vecop_smax,
      .vece    = MO_8 },
    { .fniv    = tcg_gen_smax_vec,
      .fno     = gen_helper_gvec_smax16,
      .opt_opc = hlx_vecop_smax,
      .vece    = MO_16 },
    { .fni4    = tcg_gen_smax_i32,
      .fniv    = tcg_gen_smax_vec,
      .fno     = gen_helper_gvec_smax32,
      .opt_opc = hlx_vecop_smax,
      .vece    = MO_32 },
    { .fni8    = tcg_gen_smax_i64,
      .fniv    = tcg_gen_smax_vec,
      .fno     = gen_helper_gvec_smax64,
      .opt_opc = hlx_vecop_smax,
      .vece    = MO_64 },
};

/* umax: unsigned maximum */
static const TCGOpcode hlx_vecop_umax[] = { INDEX_op_umax_vec, 0 };

const GVecGen3 hlx_umax_op[4] = {
    { .fniv    = tcg_gen_umax_vec,
      .fno     = gen_helper_gvec_umax8,
      .opt_opc = hlx_vecop_umax,
      .vece    = MO_8 },
    { .fniv    = tcg_gen_umax_vec,
      .fno     = gen_helper_gvec_umax16,
      .opt_opc = hlx_vecop_umax,
      .vece    = MO_16 },
    { .fni4    = tcg_gen_umax_i32,
      .fniv    = tcg_gen_umax_vec,
      .fno     = gen_helper_gvec_umax32,
      .opt_opc = hlx_vecop_umax,
      .vece    = MO_32 },
    { .fni8    = tcg_gen_umax_i64,
      .fniv    = tcg_gen_umax_vec,
      .fno     = gen_helper_gvec_umax64,
      .opt_opc = hlx_vecop_umax,
      .vece    = MO_64 },
};

/* -----------------------------------------------------------------------
 * abs descriptor array (GVecGen2[4])
 *
 * The fni8 helper for MO_8 (tcg_gen_vec_abs8_i64) and MO_16
 * (tcg_gen_vec_abs16_i64) are static in tcg-op-gvec.c, so we provide
 * our own copies here using the same algorithm.
 * ----------------------------------------------------------------------- */

/*
 * gen_absv_mask - produce abs of each element using mask trick.
 * Copied from tcg-op-gvec.c (static helper).
 *
 * Produces abs of each <nbit>-wide element packed in a 64-bit word:
 *   t = -1 for each negative element (broadcast from sign bit)
 *   d = (b ^ t) + (t & 1)  - which is  b ^ (-1) + 1  == -b if b < 0
 */
static void hlx_gen_absv_mask(TCGv_i64 d, TCGv_i64 b, unsigned vece)
{
    TCGv_i64 t = tcg_temp_new_i64();
    int nbit = 8 << vece;

    /* Create -1 for each negative element. */
    tcg_gen_shri_i64(t, b, nbit - 1);
    tcg_gen_andi_i64(t, t, dup_const(vece, 1));
    tcg_gen_muli_i64(t, t, (1 << nbit) - 1);

    /* Invert and add one; msb is cleared, so no carry into next element. */
    tcg_gen_xor_i64(d, b, t);
    tcg_gen_andi_i64(t, t, dup_const(vece, 1));
    tcg_gen_add_i64(d, d, t);

    tcg_temp_free_i64(t);
}

static void hlx_vec_abs16_i64(TCGv_i64 d, TCGv_i64 b)
{
    hlx_gen_absv_mask(d, b, MO_16);
}

static const TCGOpcode hlx_vecop_abs[] = { INDEX_op_abs_vec, 0 };

const GVecGen2 hlx_abs_op[4] = {
    { .fno     = gen_helper_gvec_abs8,
      .opt_opc = hlx_vecop_abs,
      .fniv    = tcg_gen_abs_vec,
      .vece    = MO_8 },
    { .fni8    = hlx_vec_abs16_i64,
      .fniv    = tcg_gen_abs_vec,
      .fno     = gen_helper_gvec_abs16,
      .opt_opc = hlx_vecop_abs,
      .vece    = MO_16 },
    { .fni4    = tcg_gen_abs_i32,
      .fniv    = tcg_gen_abs_vec,
      .fno     = gen_helper_gvec_abs32,
      .opt_opc = hlx_vecop_abs,
      .vece    = MO_32 },
    { .fni8    = tcg_gen_abs_i64,
      .fniv    = tcg_gen_abs_vec,
      .fno     = gen_helper_gvec_abs64,
      .opt_opc = hlx_vecop_abs,
      .prefer_i64 = true,
      .vece    = MO_64 },
};

/* -----------------------------------------------------------------------
 * signed-to-unsigned saturating add/sub and absolute saturating
 *
 * ushsat_add: d[i] = clamp(s16(a[i]) + s16(b[i]), 0, 65535)
 * ushsat_sub: d[i] = clamp(s16(a[i]) - s16(b[i]), 0, 65535)
 * abs_uh_sat: d[i] = min(abs(s16(a[i])), 32767)
 *             (i.e. INT16_MIN -> INT16_MAX, all others -> abs)
 *
 * Approach for ushsat_add/sub:
 *   The signed-saturating primitives (ssadd/sssub) clamp to [-32768, 32767],
 *   which when clamped to 0 via smax gives [0, 32767] - correct for most
 *   inputs but loses the range [32768, 65534] when both operands are positive
 *   and their true sum exceeds 32767.
 *
 *   Correct algorithm (fniv path for small vectors; fno fallback for HLX's
 *   2048-byte vectors which exceed gvec's MAX_UNROLL limit):
 *     wrap = a + b  (modular MO_16)
 *     ssat = max(ssadd(a, b), 0)           -> [0, 32767] for non-pos-ovf
 *     pos_ovf = (a >= 0) AND (b >= 0) AND (wrap < 0)
 *               - true sum in [32768, 65534], wrap as u16 == true sum
 *     result = pos_ovf ? wrap : ssat
 *
 *   The neg_ovf case (both a, b < 0, true sum < -32768) is handled by
 *   ssadd clamping the result to the negative range and smax to 0.
 *
 * Approach for abs_uh_sat:
 *   abs_vec(INT16_MIN) wraps to INT16_MIN (0x8000).
 *   umin(0x8000, 0x7FFF) = 0x7FFF.
 *   All other inputs produce abs in [0, 32767] <= 0x7FFF, unchanged by umin.
 * ----------------------------------------------------------------------- */

/* opt_opc lists for saturating descriptors */
static const TCGOpcode hlx_vecop_ushsat_add[] = {
    INDEX_op_add_vec,
    INDEX_op_ssadd_vec,
    INDEX_op_smax_vec,
    INDEX_op_cmp_vec,
    0
};

static const TCGOpcode hlx_vecop_ushsat_sub[] = {
    INDEX_op_sub_vec,
    INDEX_op_sssub_vec,
    INDEX_op_smax_vec,
    INDEX_op_cmp_vec,
    0
};

static const TCGOpcode hlx_vecop_abs_uh_sat[] = {
    INDEX_op_abs_vec,
    INDEX_op_umin_vec,
    0
};

static void hlx_fniv_ushsat_add(unsigned vece, TCGv_vec r,
                                 TCGv_vec a, TCGv_vec b)
{
    TCGv_vec wrap    = tcg_temp_new_vec_matching(r);
    TCGv_vec ssat    = tcg_temp_new_vec_matching(r);
    TCGv_vec pos_ovf = tcg_temp_new_vec_matching(r);
    TCGv_vec pa      = tcg_temp_new_vec_matching(r);
    TCGv_vec pb      = tcg_temp_new_vec_matching(r);
    TCGv_vec zero    = tcg_constant_vec_matching(r, vece, 0);

    /* wrap = a + b  (modular MO_16) */
    tcg_gen_add_vec(vece, wrap, a, b);

    /* ssat = max(ssadd(a, b), 0)  -> [0, 32767] when no positive overflow */
    tcg_gen_ssadd_vec(vece, ssat, a, b);
    tcg_gen_smax_vec(vece, ssat, ssat, zero);

    /* pos_ovf mask: a >= 0 AND b >= 0 AND wrap < 0
     * When true, the true sum is in [32768, 65534] and wrap (as u16) == true sum. */
    tcg_gen_cmp_vec(TCG_COND_GE, vece, pa, a, zero);         /* 0xFFFF if a >= 0 */
    tcg_gen_cmp_vec(TCG_COND_GE, vece, pb, b, zero);         /* 0xFFFF if b >= 0 */
    tcg_gen_cmp_vec(TCG_COND_LT, vece, pos_ovf, wrap, zero); /* 0xFFFF if wrap < 0 */
    tcg_gen_and_vec(vece, pos_ovf, pos_ovf, pa);
    tcg_gen_and_vec(vece, pos_ovf, pos_ovf, pb);

    /* select: pos_ovf ? wrap : ssat */
    tcg_gen_bitsel_vec(vece, r, pos_ovf, wrap, ssat);

    tcg_temp_free_vec(pb);
    tcg_temp_free_vec(pa);
    tcg_temp_free_vec(pos_ovf);
    tcg_temp_free_vec(ssat);
    tcg_temp_free_vec(wrap);
}

static void hlx_fniv_ushsat_sub(unsigned vece, TCGv_vec r,
                                 TCGv_vec a, TCGv_vec b)
{
    TCGv_vec wrap    = tcg_temp_new_vec_matching(r);
    TCGv_vec ssat    = tcg_temp_new_vec_matching(r);
    TCGv_vec pos_ovf = tcg_temp_new_vec_matching(r);
    TCGv_vec pa      = tcg_temp_new_vec_matching(r);
    TCGv_vec nb      = tcg_temp_new_vec_matching(r);
    TCGv_vec zero    = tcg_constant_vec_matching(r, vece, 0);

    /* wrap = a - b  (modular MO_16) */
    tcg_gen_sub_vec(vece, wrap, a, b);

    /* ssat = max(sssub(a, b), 0)  -> [0, 32767] when no positive overflow */
    tcg_gen_sssub_vec(vece, ssat, a, b);
    tcg_gen_smax_vec(vece, ssat, ssat, zero);

    /* pos_ovf mask: a >= 0 AND b < 0 AND wrap < 0
     * When true, the true diff is in [32768, 65535] and wrap (as u16) == true diff. */
    tcg_gen_cmp_vec(TCG_COND_GE, vece, pa, a, zero);         /* 0xFFFF if a >= 0 */
    tcg_gen_cmp_vec(TCG_COND_LT, vece, nb, b, zero);         /* 0xFFFF if b < 0  */
    tcg_gen_cmp_vec(TCG_COND_LT, vece, pos_ovf, wrap, zero); /* 0xFFFF if wrap < 0 */
    tcg_gen_and_vec(vece, pos_ovf, pos_ovf, pa);
    tcg_gen_and_vec(vece, pos_ovf, pos_ovf, nb);

    /* select: pos_ovf ? wrap : ssat */
    tcg_gen_bitsel_vec(vece, r, pos_ovf, wrap, ssat);

    tcg_temp_free_vec(nb);
    tcg_temp_free_vec(pa);
    tcg_temp_free_vec(pos_ovf);
    tcg_temp_free_vec(ssat);
    tcg_temp_free_vec(wrap);
}

static void hlx_fniv_abs_uh_sat(unsigned vece, TCGv_vec r, TCGv_vec a)
{
    /*
     * abs(INT16_MIN) wraps to INT16_MIN (0x8000) in MO_16 lanes.
     * Treating 0x8000 as unsigned (32768) and applying umin with
     * 0x7FFF (32767) gives 0x7FFF = INT16_MAX.
     * For all other inputs abs in [0, 32767] <= 0x7FFF, umin is a no-op.
     */
    TCGv_vec imax = tcg_constant_vec_matching(r, vece, 0x7FFF);
    tcg_gen_abs_vec(vece, r, a);
    tcg_gen_umin_vec(vece, r, r, imax);
}

const GVecGen3 hlx_ushsat_add_op = {
    .fniv    = hlx_fniv_ushsat_add,
    .opt_opc = hlx_vecop_ushsat_add,
    .fno     = gen_helper_hlx_gvec_ushsat_add16,
    .vece    = MO_16,
};

const GVecGen3 hlx_ushsat_sub_op = {
    .fniv    = hlx_fniv_ushsat_sub,
    .opt_opc = hlx_vecop_ushsat_sub,
    .fno     = gen_helper_hlx_gvec_ushsat_sub16,
    .vece    = MO_16,
};

const GVecGen2 hlx_abs_uh_sat_op = {
    .fniv    = hlx_fniv_abs_uh_sat,
    .opt_opc = hlx_vecop_abs_uh_sat,
    .fno     = gen_helper_hlx_gvec_abs_uh_sat16,
    .vece    = MO_16,
};

/* -----------------------------------------------------------------------
 * compare greater-than (signed and unsigned) - MO_16
 *
 * cmp.gt produces all-1s (0xFFFF) if a > b, else all-0s (0x0000) per lane.
 *
 * Implementation: use signed-LT ((b < a) == (a > b)) and unsigned-LT descriptors.
 * The gen_hlx_inline "cmp_gt" op type emits tcg_gen_gvec_3_var with the two
 * source arguments SWAPPED, so the LT descriptor computes (b < a) = (a > b).
 *
 * For the fniv path (vectorised): the wrapper calls cmp_vec(TCG_COND_LT, ...)
 * on the already-swapped inputs.
 * For the fno path (used for HLX's 2048-byte vectors): gen_helper_gvec_lt16
 * / gen_helper_gvec_ltu16 are passed swapped src pointers.
 * ----------------------------------------------------------------------- */

static void hlx_fniv_cmp_slt_h(unsigned vece, TCGv_vec r,
                                TCGv_vec a, TCGv_vec b)
{
    tcg_gen_cmp_vec(TCG_COND_LT, vece, r, a, b);
}

static void hlx_fniv_cmp_ult_h(unsigned vece, TCGv_vec r,
                                TCGv_vec a, TCGv_vec b)
{
    tcg_gen_cmp_vec(TCG_COND_LTU, vece, r, a, b);
}

static const TCGOpcode hlx_vecop_cmp[] = { INDEX_op_cmp_vec, 0 };

/*
 * Signed LT descriptor - used with swapped args to implement signed GT.
 * fno = gen_helper_gvec_lt16 computes d[i] = (a[i] < b[i]) ? ~0 : 0.
 * When called with (d, b, a) it computes (b < a) = (a > b).
 */
const GVecGen3 hlx_cmp_slt_h_op = {
    .fniv    = hlx_fniv_cmp_slt_h,
    .fno     = gen_helper_gvec_lt16,
    .opt_opc = hlx_vecop_cmp,
    .vece    = MO_16,
};

/*
 * Unsigned LT descriptor - used with swapped args to implement unsigned GT.
 * fno = gen_helper_gvec_ltu16 computes d[i] = (a[i] <u b[i]) ? ~0 : 0.
 * When called with (d, b, a) it computes (b <u a) = (a >u b).
 */
const GVecGen3 hlx_cmp_ult_h_op = {
    .fniv    = hlx_fniv_cmp_ult_h,
    .fno     = gen_helper_gvec_ltu16,
    .opt_opc = hlx_vecop_cmp,
    .vece    = MO_16,
};

/* -----------------------------------------------------------------------
 * halfword average ops (MO_16)
 *
 * Four variants: signed/unsigned x truncating/rounded.
 * Formulas (overflow-free):
 *   truncating: (a & b) + ((a ^ b) >> 1)   - >> is arithmetic for signed
 *   rounded:    (a | b) - ((a ^ b) >> 1)   - >> is arithmetic for signed
 *   unsigned variants use logical right shift
 *
 * HLX vectors are 2048 bytes - gvec always falls back to fno for HLX.
 * The fniv bodies are provided for completeness (used for smaller vectors
 * in other contexts), but the fno out-of-line helpers are what runs.
 * ----------------------------------------------------------------------- */

static void hlx_fniv_avg_sh(unsigned vece, TCGv_vec r,
                             TCGv_vec a, TCGv_vec b)
{
    /* (a & b) + ((a ^ b) >>a 1)  - signed truncating average */
    TCGv_vec ta = tcg_temp_new_vec_matching(r);
    TCGv_vec tb = tcg_temp_new_vec_matching(r);
    tcg_gen_and_vec(vece, ta, a, b);
    tcg_gen_xor_vec(vece, tb, a, b);
    tcg_gen_sari_vec(vece, tb, tb, 1);
    tcg_gen_add_vec(vece, r, ta, tb);
    tcg_temp_free_vec(tb);
    tcg_temp_free_vec(ta);
}

static void hlx_fniv_avg_sh_rnd(unsigned vece, TCGv_vec r,
                                 TCGv_vec a, TCGv_vec b)
{
    /* (a | b) - ((a ^ b) >>a 1)  - signed rounded average */
    TCGv_vec ta = tcg_temp_new_vec_matching(r);
    TCGv_vec tb = tcg_temp_new_vec_matching(r);
    tcg_gen_or_vec(vece, ta, a, b);
    tcg_gen_xor_vec(vece, tb, a, b);
    tcg_gen_sari_vec(vece, tb, tb, 1);
    tcg_gen_sub_vec(vece, r, ta, tb);
    tcg_temp_free_vec(tb);
    tcg_temp_free_vec(ta);
}

static void hlx_fniv_avg_uh(unsigned vece, TCGv_vec r,
                             TCGv_vec a, TCGv_vec b)
{
    /* (a & b) + ((a ^ b) >>l 1)  - unsigned truncating average */
    TCGv_vec ta = tcg_temp_new_vec_matching(r);
    TCGv_vec tb = tcg_temp_new_vec_matching(r);
    tcg_gen_and_vec(vece, ta, a, b);
    tcg_gen_xor_vec(vece, tb, a, b);
    tcg_gen_shri_vec(vece, tb, tb, 1);
    tcg_gen_add_vec(vece, r, ta, tb);
    tcg_temp_free_vec(tb);
    tcg_temp_free_vec(ta);
}

static void hlx_fniv_avg_uh_rnd(unsigned vece, TCGv_vec r,
                                  TCGv_vec a, TCGv_vec b)
{
    /* (a | b) - ((a ^ b) >>l 1)  - unsigned rounded average */
    TCGv_vec ta = tcg_temp_new_vec_matching(r);
    TCGv_vec tb = tcg_temp_new_vec_matching(r);
    tcg_gen_or_vec(vece, ta, a, b);
    tcg_gen_xor_vec(vece, tb, a, b);
    tcg_gen_shri_vec(vece, tb, tb, 1);
    tcg_gen_sub_vec(vece, r, ta, tb);
    tcg_temp_free_vec(tb);
    tcg_temp_free_vec(ta);
}

static const TCGOpcode hlx_vecop_avg_sh[] = {
    INDEX_op_and_vec,
    INDEX_op_xor_vec,
    INDEX_op_sari_vec,
    INDEX_op_add_vec,
    0
};

static const TCGOpcode hlx_vecop_avg_sh_rnd[] = {
    INDEX_op_or_vec,
    INDEX_op_xor_vec,
    INDEX_op_sari_vec,
    INDEX_op_sub_vec,
    0
};

static const TCGOpcode hlx_vecop_avg_uh[] = {
    INDEX_op_and_vec,
    INDEX_op_xor_vec,
    INDEX_op_shri_vec,
    INDEX_op_add_vec,
    0
};

static const TCGOpcode hlx_vecop_avg_uh_rnd[] = {
    INDEX_op_or_vec,
    INDEX_op_xor_vec,
    INDEX_op_shri_vec,
    INDEX_op_sub_vec,
    0
};

const GVecGen3 hlx_avg_sh_op = {
    .fniv    = hlx_fniv_avg_sh,
    .opt_opc = hlx_vecop_avg_sh,
    .fno     = gen_helper_hlx_gvec_avg_sh16,
    .vece    = MO_16,
};

const GVecGen3 hlx_avg_sh_rnd_op = {
    .fniv    = hlx_fniv_avg_sh_rnd,
    .opt_opc = hlx_vecop_avg_sh_rnd,
    .fno     = gen_helper_hlx_gvec_avg_sh16_rnd,
    .vece    = MO_16,
};

const GVecGen3 hlx_avg_uh_op = {
    .fniv    = hlx_fniv_avg_uh,
    .opt_opc = hlx_vecop_avg_uh,
    .fno     = gen_helper_hlx_gvec_avg_uh16,
    .vece    = MO_16,
};

const GVecGen3 hlx_avg_uh_rnd_op = {
    .fniv    = hlx_fniv_avg_uh_rnd,
    .opt_opc = hlx_vecop_avg_uh_rnd,
    .fno     = gen_helper_hlx_gvec_avg_uh16_rnd,
    .vece    = MO_16,
};
