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

#endif /* HLX_GVEC_TABLES_H */
