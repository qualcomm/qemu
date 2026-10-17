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
