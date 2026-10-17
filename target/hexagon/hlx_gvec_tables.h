/*
 * HLX GVecGen3/GVecGen2 descriptor tables - declarations for consumers.
 *
 * Copyright (c) 2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef HLX_GVEC_TABLES_H
#define HLX_GVEC_TABLES_H

#include "tcg/tcg-op-gvec-common.h"

/*
 * Saturating add/sub descriptor arrays, indexed by MemOp element size
 * (MO_8 .. MO_64).  Use e.g. &hlx_ssadd_op[MO_16].
 */
extern const GVecGen3 hlx_ssadd_op[4]; /* signed saturating add */
extern const GVecGen3 hlx_sssub_op[4]; /* signed saturating sub */
extern const GVecGen3 hlx_usadd_op[4]; /* unsigned saturating add */
extern const GVecGen3 hlx_ussub_op[4]; /* unsigned saturating sub */

/*
 * Logical op descriptors (element-size independent).
 */
extern const GVecGen3 hlx_and_op;  /* d = a & b        */
extern const GVecGen3 hlx_or_op;   /* d = a | b        */
extern const GVecGen3 hlx_xor_op;  /* d = a ^ b        */
extern const GVecGen3 hlx_andc_op; /* d = a & ~b  (LX_andn) */
extern const GVecGen3 hlx_orc_op;  /* d = a | ~b  (LX_orn)  */

/*
 * min/max descriptor arrays, indexed by MemOp element size.
 */
extern const GVecGen3 hlx_smin_op[4]; /* signed minimum */
extern const GVecGen3 hlx_umin_op[4]; /* unsigned minimum */
extern const GVecGen3 hlx_smax_op[4]; /* signed maximum */
extern const GVecGen3 hlx_umax_op[4]; /* unsigned maximum */

/*
 * abs descriptor array (GVecGen2[4]), indexed by MemOp.
 */
extern const GVecGen2 hlx_abs_op[4]; /* absolute value */

/*
 * compare greater-than (signed and unsigned halfword).
 * These produce all-1s (0xFFFF per lane) if a > b, else all-0s.
 *
 * Implementation: these are signed-LT / unsigned-LT descriptors used with
 * swapped src args.  The gen_hlx_inline "cmp_gt" op type passes b first
 * and a second so that (b < a) computes (a > b).
 */
extern const GVecGen3 hlx_cmp_slt_h_op; /* signed   LT, MO_16 (used for GT) */
extern const GVecGen3 hlx_cmp_ult_h_op; /* unsigned LT, MO_16 (used for GT) */

/*
 * signed-to-unsigned saturating add/sub and saturating absolute.
 *
 * hlx_ushsat_add_op (GVecGen3, MO_16):
 *   d[i] = clamp(s16(a[i]) + s16(b[i]), 0, 65535)
 *
 * hlx_ushsat_sub_op (GVecGen3, MO_16):
 *   d[i] = clamp(s16(a[i]) - s16(b[i]), 0, 65535)
 *
 * hlx_abs_uh_sat_op (GVecGen2, MO_16):
 *   d[i] = min(abs(s16(a[i])), 32767)   (INT16_MIN saturates to INT16_MAX)
 */
extern const GVecGen3 hlx_ushsat_add_op;
extern const GVecGen3 hlx_ushsat_sub_op;
extern const GVecGen2 hlx_abs_uh_sat_op;

/*
 * halfword average ops (MO_16).
 *
 * hlx_avg_sh_op (GVecGen3, MO_16):
 *   d[i] = trunc_avg(s16(a[i]), s16(b[i]))  - signed, truncating
 *
 * hlx_avg_sh_rnd_op (GVecGen3, MO_16):
 *   d[i] = rnd_avg(s16(a[i]), s16(b[i]))    - signed, rounded
 *
 * hlx_avg_uh_op (GVecGen3, MO_16):
 *   d[i] = trunc_avg(u16(a[i]), u16(b[i]))  - unsigned, truncating
 *
 * hlx_avg_uh_rnd_op (GVecGen3, MO_16):
 *   d[i] = rnd_avg(u16(a[i]), u16(b[i]))    - unsigned, rounded
 */
extern const GVecGen3 hlx_avg_sh_op;
extern const GVecGen3 hlx_avg_sh_rnd_op;
extern const GVecGen3 hlx_avg_uh_op;
extern const GVecGen3 hlx_avg_uh_rnd_op;

/*
 * halfword absdiff ops (GVecGen3, MO_16).
 *
 * hlx_absdiff_sh_op:
 *   d[i] = (uint16_t)(max(s16(a[i]), s16(b[i])) - min(s16(a[i]), s16(b[i])))
 *
 * hlx_absdiff_uh_op:
 *   d[i] = max(u16(a[i]), u16(b[i])) - min(u16(a[i]), u16(b[i]))
 */
extern const GVecGen3 hlx_absdiff_sh_op;
extern const GVecGen3 hlx_absdiff_uh_op;

/*
 * sari-by-15 on halfwords (GVecGen2, MO_16).
 *
 * hlx_sari15_h_op:
 *   d[i] = (int16_t)a[i] >> 15
 *   Result is 0x0000 for non-negative lanes, 0xFFFF for negative lanes.
 *   Used for the high L-reg of LX_conv_w_h (sxt halfword-to-word-pair).
 */
extern const GVecGen2 hlx_sari15_h_op;

#endif /* HLX_GVEC_TABLES_H */
