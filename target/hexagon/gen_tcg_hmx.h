/*
 * Hexagon HMX TCG Code Generation
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Simple HMX operations use pure TCG inline code.
 * Complex operations (matmul, activation load, convert, bias) call
 * C helper functions defined in hmx_helper.c.
 */

#ifndef HEXAGON_GEN_TCG_HMX_H
#define HEXAGON_GEN_TCG_HMX_H

#include "tcg/tcg-op.h"
#include "hmx_state.h"
#include "gen_tcg_hmx_matmul.h"

/*
 * Simple ops - call C helpers (accumulators are too large for gvec)
 */

/*
 * M8_mxclracc - Clear both fixed-point accumulators in both sets
 */
#define fGEN_TCG_M8_mxclracc(SHORTCODE) \
    gen_helper_hmx_clracc(tcg_env)

/*
 * M8_mxclracc_hf - Clear both floating-point accumulators in both sets
 */
#define fGEN_TCG_M8_mxclracc_hf(SHORTCODE) \
    gen_helper_hmx_clracc_hf(tcg_env)

/*
 * M8_mxswap / M8_mxswap_hf - Toggle accumulator set index (0 <-> 1)
 */
#define fGEN_TCG_M8_mxswap(SHORTCODE) \
    do { \
        TCGv_ptr hmx_ptr = tcg_temp_new_ptr(); \
        TCGv_i32 acc_set = tcg_temp_new_i32(); \
        tcg_gen_ld_ptr(hmx_ptr, tcg_env, HMX_STATE_PTR_OFS); \
        tcg_gen_ld_i32(acc_set, hmx_ptr, \
                       offsetof(HmxState, current_acc_set)); \
        tcg_gen_xori_i32(acc_set, acc_set, 1); \
        tcg_gen_st_i32(acc_set, hmx_ptr, \
                       offsetof(HmxState, current_acc_set)); \
    } while (0)

#define fGEN_TCG_M8_mxswap_hf(SHORTCODE) fGEN_TCG_M8_mxswap(SHORTCODE)

/*
 * M8_mxaccshl - Shift primary FXP accumulators left by 16
 */
#define fGEN_TCG_M8_mxaccshl(SHORTCODE) \
    gen_helper_hmx_accshl(tcg_env)

/*
 * Debug print instructions - no-ops
 */
#define fGEN_TCG_M8_pv64(SHORTCODE)     do { } while (0)
#define fGEN_TCG_M8_pv64d(SHORTCODE)    do { } while (0)
#define fGEN_TCG_M8_pv64fp(SHORTCODE)   do { } while (0)
#define fGEN_TCG_M8_pv64dfp(SHORTCODE)  do { } while (0)

/*
 * Bias load/store - call C helpers
 */

#define fGEN_TCG_M8_mxmem_bias(SHORTCODE) \
    gen_helper_hmx_bias_load(tcg_env, RsV, tcg_constant_i32(0))
#define fGEN_TCG_M8_mxmem2_bias(SHORTCODE) \
    gen_helper_hmx_bias_load(tcg_env, RsV, tcg_constant_i32(1))
#define fGEN_TCG_M8_mxmem_st_bias(SHORTCODE) \
    gen_helper_hmx_bias_store(tcg_env, RsV, tcg_constant_i32(0))
#define fGEN_TCG_M8_mxmem2_st_bias(SHORTCODE) \
    gen_helper_hmx_bias_store(tcg_env, RsV, tcg_constant_i32(1))

/*
 * Convert-to-scalar (cvt_rs) - call C helper
 */

#define fGEN_TCG_M8_cvt_rs_ub(SHORTCODE) \
    gen_helper_hmx_cvt_rs(RsV, tcg_env, RsV, \
        tcg_constant_i32(HMX_CVT_RS_UB))
#define fGEN_TCG_M8_cvt_rs_ub_sc0(SHORTCODE) \
    gen_helper_hmx_cvt_rs(RsV, tcg_env, RsV, \
        tcg_constant_i32(HMX_CVT_RS_UB_SC0))
#define fGEN_TCG_M8_cvt_rs_ub_sc1(SHORTCODE) \
    gen_helper_hmx_cvt_rs(RsV, tcg_env, RsV, \
        tcg_constant_i32(HMX_CVT_RS_UB_SC1))
#define fGEN_TCG_M8_cvt_rs_uh_2x1(SHORTCODE) \
    gen_helper_hmx_cvt_rs(RsV, tcg_env, RsV, \
        tcg_constant_i32(HMX_CVT_RS_UH_2X1))
#define fGEN_TCG_M8_cvt_rs_uh_2x2(SHORTCODE) \
    gen_helper_hmx_cvt_rs(RsV, tcg_env, RsV, \
        tcg_constant_i32(HMX_CVT_RS_UH_2X2))
#define fGEN_TCG_M8_cvt_rs_hf(SHORTCODE) \
    gen_helper_hmx_cvt_rs(RsV, tcg_env, RsV, \
        tcg_constant_i32(HMX_CVT_RS_HF))
#define fGEN_TCG_M8_cvt_rs_f8(SHORTCODE) \
    gen_helper_hmx_cvt_rs(RsV, tcg_env, RsV, \
        tcg_constant_i32(HMX_CVT_RS_F8))

/*
 * Convert store operations - call C helper
 */

#define fGEN_TCG_M8_mxmem(SHORTCODE) \
    gen_helper_hmx_cvt_store(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_CVTST(HMX_CVTST_NORMAL, HMX_CVTST_AGE0)))
#define fGEN_TCG_M8_mxmem_deep(SHORTCODE) \
    gen_helper_hmx_cvt_store(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_CVTST(HMX_CVTST_NORMAL, HMX_CVTST_AGE1)))
#define fGEN_TCG_M8_mxmem_cm(SHORTCODE) \
    gen_helper_hmx_cvt_store(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_CVTST(HMX_CVTST_CM, HMX_CVTST_AGE0)))
#define fGEN_TCG_M8_mxmem_2x2(SHORTCODE) \
    gen_helper_hmx_cvt_store(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_CVTST(HMX_CVTST_2X2, HMX_CVTST_AGE0)))
#define fGEN_TCG_M8_mxmem_cm_deep(SHORTCODE) \
    gen_helper_hmx_cvt_store(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_CVTST(HMX_CVTST_CM, HMX_CVTST_AGE1)))
#define fGEN_TCG_M8_mxmem_f8(SHORTCODE) \
    gen_helper_hmx_cvt_store(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_CVTST(HMX_CVTST_F8, HMX_CVTST_AGE0)))
#define fGEN_TCG_M8_mxmem_deep_f8(SHORTCODE) \
    gen_helper_hmx_cvt_store(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_CVTST(HMX_CVTST_F8, HMX_CVTST_AGE1)))

/*
 * Legacy convert (mxcvt*) - call C helper
 */

/* UB DM format: :sat sets relu=0, no :sat sets relu=1 (matches reference) */
#define fGEN_TCG_M8_mxcvtl_dm_sat_ub(SHORTCODE) \
    gen_helper_hmx_cvt_transfer(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_CVT(HMX_CVT_LEFT, HMX_CVT_FMT_UB_DM, 0, 0)))
#define fGEN_TCG_M8_mxcvtl_dm_ub(SHORTCODE) \
    gen_helper_hmx_cvt_transfer(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_CVT(HMX_CVT_LEFT, HMX_CVT_FMT_UB_DM, 1, 0)))
#define fGEN_TCG_M8_mxcvtl_dm_sat_ub_r(SHORTCODE) \
    gen_helper_hmx_cvt_transfer(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_CVT(HMX_CVT_LEFT, HMX_CVT_FMT_UB_DM, 0, 1)))
#define fGEN_TCG_M8_mxcvtl_dm_ub_r(SHORTCODE) \
    gen_helper_hmx_cvt_transfer(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_CVT(HMX_CVT_LEFT, HMX_CVT_FMT_UB_DM, 1, 1)))
#define fGEN_TCG_M8_mxcvtr_dm_sat_ub(SHORTCODE) \
    gen_helper_hmx_cvt_transfer(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_CVT(HMX_CVT_RIGHT, HMX_CVT_FMT_UB_DM, 0, 0)))
#define fGEN_TCG_M8_mxcvtr_dm_ub(SHORTCODE) \
    gen_helper_hmx_cvt_transfer(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_CVT(HMX_CVT_RIGHT, HMX_CVT_FMT_UB_DM, 1, 0)))
#define fGEN_TCG_M8_mxcvtr_dm_sat_ub_r(SHORTCODE) \
    gen_helper_hmx_cvt_transfer(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_CVT(HMX_CVT_RIGHT, HMX_CVT_FMT_UB_DM, 0, 1)))
#define fGEN_TCG_M8_mxcvtr_dm_ub_r(SHORTCODE) \
    gen_helper_hmx_cvt_transfer(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_CVT(HMX_CVT_RIGHT, HMX_CVT_FMT_UB_DM, 1, 1)))

/* UB SM format: :sat sets relu=0, no :sat sets relu=1 */
#define fGEN_TCG_M8_mxcvtl_sat_ub(SHORTCODE) \
    gen_helper_hmx_cvt_transfer(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_CVT(HMX_CVT_LEFT, HMX_CVT_FMT_UB_SM, 0, 0)))
#define fGEN_TCG_M8_mxcvtl_ub(SHORTCODE) \
    gen_helper_hmx_cvt_transfer(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_CVT(HMX_CVT_LEFT, HMX_CVT_FMT_UB_SM, 1, 0)))
#define fGEN_TCG_M8_mxcvtl_sat_ub_r(SHORTCODE) \
    gen_helper_hmx_cvt_transfer(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_CVT(HMX_CVT_LEFT, HMX_CVT_FMT_UB_SM, 0, 1)))
#define fGEN_TCG_M8_mxcvtl_ub_r(SHORTCODE) \
    gen_helper_hmx_cvt_transfer(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_CVT(HMX_CVT_LEFT, HMX_CVT_FMT_UB_SM, 1, 1)))
#define fGEN_TCG_M8_mxcvtr_sat_ub(SHORTCODE) \
    gen_helper_hmx_cvt_transfer(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_CVT(HMX_CVT_RIGHT, HMX_CVT_FMT_UB_SM, 0, 0)))
#define fGEN_TCG_M8_mxcvtr_ub(SHORTCODE) \
    gen_helper_hmx_cvt_transfer(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_CVT(HMX_CVT_RIGHT, HMX_CVT_FMT_UB_SM, 1, 0)))
#define fGEN_TCG_M8_mxcvtr_sat_ub_r(SHORTCODE) \
    gen_helper_hmx_cvt_transfer(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_CVT(HMX_CVT_RIGHT, HMX_CVT_FMT_UB_SM, 0, 1)))
#define fGEN_TCG_M8_mxcvtr_ub_r(SHORTCODE) \
    gen_helper_hmx_cvt_transfer(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_CVT(HMX_CVT_RIGHT, HMX_CVT_FMT_UB_SM, 1, 1)))

/* UH 2x1 format: :sat sets relu=0, no :sat sets relu=1 */
#define fGEN_TCG_M8_mxcvtb_sat_uh(SHORTCODE) \
    gen_helper_hmx_cvt_transfer(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_CVT(HMX_CVT_BOTTOM, HMX_CVT_FMT_UH, 0, 0)))
#define fGEN_TCG_M8_mxcvtb_uh(SHORTCODE) \
    gen_helper_hmx_cvt_transfer(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_CVT(HMX_CVT_BOTTOM, HMX_CVT_FMT_UH, 1, 0)))
#define fGEN_TCG_M8_mxcvtb_sat_uh_r(SHORTCODE) \
    gen_helper_hmx_cvt_transfer(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_CVT(HMX_CVT_BOTTOM, HMX_CVT_FMT_UH, 0, 1)))
#define fGEN_TCG_M8_mxcvtb_uh_r(SHORTCODE) \
    gen_helper_hmx_cvt_transfer(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_CVT(HMX_CVT_BOTTOM, HMX_CVT_FMT_UH, 1, 1)))
#define fGEN_TCG_M8_mxcvta_sat_uh(SHORTCODE) \
    gen_helper_hmx_cvt_transfer(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_CVT(HMX_CVT_ABOVE, HMX_CVT_FMT_UH, 0, 0)))
#define fGEN_TCG_M8_mxcvta_uh(SHORTCODE) \
    gen_helper_hmx_cvt_transfer(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_CVT(HMX_CVT_ABOVE, HMX_CVT_FMT_UH, 1, 0)))
#define fGEN_TCG_M8_mxcvta_sat_uh_r(SHORTCODE) \
    gen_helper_hmx_cvt_transfer(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_CVT(HMX_CVT_ABOVE, HMX_CVT_FMT_UH, 0, 1)))
#define fGEN_TCG_M8_mxcvta_uh_r(SHORTCODE) \
    gen_helper_hmx_cvt_transfer(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_CVT(HMX_CVT_ABOVE, HMX_CVT_FMT_UH, 1, 1)))

/* UH 2x2 format: :sat sets relu=0, no :sat sets relu=1 */
#define fGEN_TCG_M8_mxcvtb_sat_uh2x2(SHORTCODE) \
    gen_helper_hmx_cvt_transfer(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_CVT(HMX_CVT_BOTTOM, HMX_CVT_FMT_UH2X2, 0, 0)))
#define fGEN_TCG_M8_mxcvtb_uh2x2(SHORTCODE) \
    gen_helper_hmx_cvt_transfer(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_CVT(HMX_CVT_BOTTOM, HMX_CVT_FMT_UH2X2, 1, 0)))
#define fGEN_TCG_M8_mxcvtb_sat_uh2x2_r(SHORTCODE) \
    gen_helper_hmx_cvt_transfer(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_CVT(HMX_CVT_BOTTOM, HMX_CVT_FMT_UH2X2, 0, 1)))
#define fGEN_TCG_M8_mxcvtb_uh2x2_r(SHORTCODE) \
    gen_helper_hmx_cvt_transfer(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_CVT(HMX_CVT_BOTTOM, HMX_CVT_FMT_UH2X2, 1, 1)))
#define fGEN_TCG_M8_mxcvta_sat_uh2x2(SHORTCODE) \
    gen_helper_hmx_cvt_transfer(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_CVT(HMX_CVT_ABOVE, HMX_CVT_FMT_UH2X2, 0, 0)))
#define fGEN_TCG_M8_mxcvta_uh2x2(SHORTCODE) \
    gen_helper_hmx_cvt_transfer(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_CVT(HMX_CVT_ABOVE, HMX_CVT_FMT_UH2X2, 1, 0)))
#define fGEN_TCG_M8_mxcvta_sat_uh2x2_r(SHORTCODE) \
    gen_helper_hmx_cvt_transfer(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_CVT(HMX_CVT_ABOVE, HMX_CVT_FMT_UH2X2, 0, 1)))
#define fGEN_TCG_M8_mxcvta_uh2x2_r(SHORTCODE) \
    gen_helper_hmx_cvt_transfer(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_CVT(HMX_CVT_ABOVE, HMX_CVT_FMT_UH2X2, 1, 1)))

/* HF (half-float) convert */
#define fGEN_TCG_M8_mxcvtl_sat_hf(SHORTCODE) \
    gen_helper_hmx_cvt_transfer(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_CVT(HMX_CVT_LEFT, HMX_CVT_FMT_HF, 0, 0)))
#define fGEN_TCG_M8_mxcvtl_sat_pos_hf(SHORTCODE) \
    gen_helper_hmx_cvt_transfer(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_CVT(HMX_CVT_LEFT, HMX_CVT_FMT_HF, 1, 0)))
#define fGEN_TCG_M8_mxcvtl_sat_hf_r(SHORTCODE) \
    gen_helper_hmx_cvt_transfer(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_CVT(HMX_CVT_LEFT, HMX_CVT_FMT_HF, 0, 1)))
#define fGEN_TCG_M8_mxcvtl_sat_pos_hf_r(SHORTCODE) \
    gen_helper_hmx_cvt_transfer(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_CVT(HMX_CVT_LEFT, HMX_CVT_FMT_HF, 1, 1)))
#define fGEN_TCG_M8_mxcvtr_sat_hf(SHORTCODE) \
    gen_helper_hmx_cvt_transfer(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_CVT(HMX_CVT_RIGHT, HMX_CVT_FMT_HF, 0, 0)))
#define fGEN_TCG_M8_mxcvtr_sat_pos_hf(SHORTCODE) \
    gen_helper_hmx_cvt_transfer(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_CVT(HMX_CVT_RIGHT, HMX_CVT_FMT_HF, 1, 0)))
#define fGEN_TCG_M8_mxcvtr_sat_hf_r(SHORTCODE) \
    gen_helper_hmx_cvt_transfer(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_CVT(HMX_CVT_RIGHT, HMX_CVT_FMT_HF, 0, 1)))
#define fGEN_TCG_M8_mxcvtr_sat_pos_hf_r(SHORTCODE) \
    gen_helper_hmx_cvt_transfer(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_CVT(HMX_CVT_RIGHT, HMX_CVT_FMT_HF, 1, 1)))

/*
 * Activation load operations - call C helper
 */

/* Helper macro for activation load */
#define GEN_ACT_LOAD(type, fmt, mod) \
    do { \
        gen_helper_hmx_act_load(tcg_env, RsV, RtV, \
            tcg_constant_i32(HMX_PACK_ACT(type, fmt, mod))); \
        ctx->hmx_pkt.act_valid = true; \
        ctx->hmx_pkt.format_offset = \
            ((fmt) == HMX_ACT_FMT_SM) ? 2 : 0; \
        ctx->hmx_pkt.act_mod = (mod); \
        ctx->hmx_pkt.act_type = (type); \
    } while (0)

/* UB SM (unsigned byte, spatial major) */
#define fGEN_TCG_M8_mxmem_blk_sm_act_ub(SHORTCODE) \
    GEN_ACT_LOAD(HMX_ACT_UB, HMX_ACT_FMT_SM, HMX_ACT_BLK)
#define fGEN_TCG_M8_mxmem_sm_act_ub(SHORTCODE) \
    GEN_ACT_LOAD(HMX_ACT_UB, HMX_ACT_FMT_SM, HMX_ACT_NOBLK)
#define fGEN_TCG_M8_mxmemu_blk_sm_act_ub(SHORTCODE) \
    GEN_ACT_LOAD(HMX_ACT_UB, HMX_ACT_FMT_SM, HMX_ACT_BLK_U)
#define fGEN_TCG_M8_mxmems_blk_sm_act_ub(SHORTCODE) \
    GEN_ACT_LOAD(HMX_ACT_UB, HMX_ACT_FMT_SM, HMX_ACT_BLK_S)
#define fGEN_TCG_M8_mxmemd_blk_sm_act_ub(SHORTCODE) \
    GEN_ACT_LOAD(HMX_ACT_UB, HMX_ACT_FMT_SM, HMX_ACT_BLK_D)

/* UB DM (unsigned byte, depth major) */
#define fGEN_TCG_M8_mxmem_blk_dm_act_ub(SHORTCODE) \
    GEN_ACT_LOAD(HMX_ACT_UB, HMX_ACT_FMT_DM, HMX_ACT_BLK)
#define fGEN_TCG_M8_mxmem_dm_act_ub(SHORTCODE) \
    GEN_ACT_LOAD(HMX_ACT_UB, HMX_ACT_FMT_DM, HMX_ACT_NOBLK)
#define fGEN_TCG_M8_mxmemu_blk_dm_act_ub(SHORTCODE) \
    GEN_ACT_LOAD(HMX_ACT_UB, HMX_ACT_FMT_DM, HMX_ACT_BLK_U)
#define fGEN_TCG_M8_mxmems_blk_dm_act_ub(SHORTCODE) \
    GEN_ACT_LOAD(HMX_ACT_UB, HMX_ACT_FMT_DM, HMX_ACT_BLK_S)
#define fGEN_TCG_M8_mxmemd_blk_dm_act_ub(SHORTCODE) \
    GEN_ACT_LOAD(HMX_ACT_UB, HMX_ACT_FMT_DM, HMX_ACT_BLK_D)

/* HF (half-float) activations */
#define fGEN_TCG_M8_mxmem_blk_sm_act_hf(SHORTCODE) \
    GEN_ACT_LOAD(HMX_ACT_HF, HMX_ACT_FMT_SM, HMX_ACT_BLK)
#define fGEN_TCG_M8_mxmem_sm_act_hf(SHORTCODE) \
    GEN_ACT_LOAD(HMX_ACT_HF, HMX_ACT_FMT_SM, HMX_ACT_NOBLK)
#define fGEN_TCG_M8_mxmemu_blk_sm_act_hf(SHORTCODE) \
    GEN_ACT_LOAD(HMX_ACT_HF, HMX_ACT_FMT_SM, HMX_ACT_BLK_U)
#define fGEN_TCG_M8_mxmems_blk_sm_act_hf(SHORTCODE) \
    GEN_ACT_LOAD(HMX_ACT_HF, HMX_ACT_FMT_SM, HMX_ACT_BLK_S)
#define fGEN_TCG_M8_mxmemd_blk_sm_act_hf(SHORTCODE) \
    GEN_ACT_LOAD(HMX_ACT_HF, HMX_ACT_FMT_SM, HMX_ACT_BLK_D)

/* F8 (FP8) activations */
#define fGEN_TCG_M8_mxmem_blk_sm_act_f8(SHORTCODE) \
    GEN_ACT_LOAD(HMX_ACT_F8, HMX_ACT_FMT_SM, HMX_ACT_BLK)
#define fGEN_TCG_M8_mxmem_sm_act_f8(SHORTCODE) \
    GEN_ACT_LOAD(HMX_ACT_F8, HMX_ACT_FMT_SM, HMX_ACT_NOBLK)
#define fGEN_TCG_M8_mxmemu_blk_sm_act_f8(SHORTCODE) \
    GEN_ACT_LOAD(HMX_ACT_F8, HMX_ACT_FMT_SM, HMX_ACT_BLK_U)
#define fGEN_TCG_M8_mxmems_blk_sm_act_f8(SHORTCODE) \
    GEN_ACT_LOAD(HMX_ACT_F8, HMX_ACT_FMT_SM, HMX_ACT_BLK_S)
#define fGEN_TCG_M8_mxmemd_blk_sm_act_f8(SHORTCODE) \
    GEN_ACT_LOAD(HMX_ACT_F8, HMX_ACT_FMT_SM, HMX_ACT_BLK_D)

/* Weight load + multiply FXP - TCG inline path with C helper fallback */
#define GEN_MATMUL_FXP(wei_type, mod) \
    gen_hmx_matmul_fxp(ctx, RsV, RtV, wei_type, mod)

/* Byte weights (6 modifiers) */
#define fGEN_TCG_M8_mxmem_wei_b(SHORTCODE) \
    GEN_MATMUL_FXP(HMX_WEI_B, HMX_MOD_NORMAL)
#define fGEN_TCG_M8_mxmems_wei_b(SHORTCODE) \
    GEN_MATMUL_FXP(HMX_WEI_B, HMX_MOD_SINGLE)
#define fGEN_TCG_M8_mxmemdr_wei_b(SHORTCODE) \
    GEN_MATMUL_FXP(HMX_WEI_B, HMX_MOD_DR)
#define fGEN_TCG_M8_mxmemdp_wei_b(SHORTCODE) \
    GEN_MATMUL_FXP(HMX_WEI_B, HMX_MOD_DP)
#define fGEN_TCG_M8_mxmema_wei_b(SHORTCODE) \
    GEN_MATMUL_FXP(HMX_WEI_B, HMX_MOD_ABOVE)
#define fGEN_TCG_M8_mxmemdi_wei_b(SHORTCODE) \
    GEN_MATMUL_FXP(HMX_WEI_B, HMX_MOD_DI)

/* Sign-magnitude weights */
#define fGEN_TCG_M8_mxmem_wei_sm(SHORTCODE) \
    GEN_MATMUL_FXP(HMX_WEI_SM, HMX_MOD_NORMAL)
#define fGEN_TCG_M8_mxmems_wei_sm(SHORTCODE) \
    GEN_MATMUL_FXP(HMX_WEI_SM, HMX_MOD_SINGLE)
#define fGEN_TCG_M8_mxmemdr_wei_sm(SHORTCODE) \
    GEN_MATMUL_FXP(HMX_WEI_SM, HMX_MOD_DR)
#define fGEN_TCG_M8_mxmemdp_wei_sm(SHORTCODE) \
    GEN_MATMUL_FXP(HMX_WEI_SM, HMX_MOD_DP)
#define fGEN_TCG_M8_mxmema_wei_sm(SHORTCODE) \
    GEN_MATMUL_FXP(HMX_WEI_SM, HMX_MOD_ABOVE)
#define fGEN_TCG_M8_mxmemdi_wei_sm(SHORTCODE) \
    GEN_MATMUL_FXP(HMX_WEI_SM, HMX_MOD_DI)

/* Nibble weights */
#define fGEN_TCG_M8_mxmem_wei_n(SHORTCODE) \
    GEN_MATMUL_FXP(HMX_WEI_N, HMX_MOD_NORMAL)
#define fGEN_TCG_M8_mxmems_wei_n(SHORTCODE) \
    GEN_MATMUL_FXP(HMX_WEI_N, HMX_MOD_SINGLE)
#define fGEN_TCG_M8_mxmemdr_wei_n(SHORTCODE) \
    GEN_MATMUL_FXP(HMX_WEI_N, HMX_MOD_DR)
#define fGEN_TCG_M8_mxmemdp_wei_n(SHORTCODE) \
    GEN_MATMUL_FXP(HMX_WEI_N, HMX_MOD_DP)
#define fGEN_TCG_M8_mxmema_wei_n(SHORTCODE) \
    GEN_MATMUL_FXP(HMX_WEI_N, HMX_MOD_ABOVE)
#define fGEN_TCG_M8_mxmemdi_wei_n(SHORTCODE) \
    GEN_MATMUL_FXP(HMX_WEI_N, HMX_MOD_DI)

/* Crumb weights */
#define fGEN_TCG_M8_mxmem_wei_c(SHORTCODE) \
    GEN_MATMUL_FXP(HMX_WEI_C, HMX_MOD_NORMAL)
#define fGEN_TCG_M8_mxmems_wei_c(SHORTCODE) \
    GEN_MATMUL_FXP(HMX_WEI_C, HMX_MOD_SINGLE)
#define fGEN_TCG_M8_mxmemdr_wei_c(SHORTCODE) \
    GEN_MATMUL_FXP(HMX_WEI_C, HMX_MOD_DR)
#define fGEN_TCG_M8_mxmemdp_wei_c(SHORTCODE) \
    GEN_MATMUL_FXP(HMX_WEI_C, HMX_MOD_DP)
#define fGEN_TCG_M8_mxmema_wei_c(SHORTCODE) \
    GEN_MATMUL_FXP(HMX_WEI_C, HMX_MOD_ABOVE)
#define fGEN_TCG_M8_mxmemdi_wei_c(SHORTCODE) \
    GEN_MATMUL_FXP(HMX_WEI_C, HMX_MOD_DI)

/* Signed crumb weights */
#define fGEN_TCG_M8_mxmem_wei_sc(SHORTCODE) \
    GEN_MATMUL_FXP(HMX_WEI_SC, HMX_MOD_NORMAL)
#define fGEN_TCG_M8_mxmems_wei_sc(SHORTCODE) \
    GEN_MATMUL_FXP(HMX_WEI_SC, HMX_MOD_SINGLE)
#define fGEN_TCG_M8_mxmemdr_wei_sc(SHORTCODE) \
    GEN_MATMUL_FXP(HMX_WEI_SC, HMX_MOD_DR)
#define fGEN_TCG_M8_mxmemdp_wei_sc(SHORTCODE) \
    GEN_MATMUL_FXP(HMX_WEI_SC, HMX_MOD_DP)
#define fGEN_TCG_M8_mxmema_wei_sc(SHORTCODE) \
    GEN_MATMUL_FXP(HMX_WEI_SC, HMX_MOD_ABOVE)
#define fGEN_TCG_M8_mxmemdi_wei_sc(SHORTCODE) \
    GEN_MATMUL_FXP(HMX_WEI_SC, HMX_MOD_DI)

/* 1-bit unsigned weights */
#define fGEN_TCG_M8_mxmem_wei_b1(SHORTCODE) \
    GEN_MATMUL_FXP(HMX_WEI_B1, HMX_MOD_NORMAL)
#define fGEN_TCG_M8_mxmems_wei_b1(SHORTCODE) \
    GEN_MATMUL_FXP(HMX_WEI_B1, HMX_MOD_SINGLE)
#define fGEN_TCG_M8_mxmemdr_wei_b1(SHORTCODE) \
    GEN_MATMUL_FXP(HMX_WEI_B1, HMX_MOD_DR)
#define fGEN_TCG_M8_mxmemdp_wei_b1(SHORTCODE) \
    GEN_MATMUL_FXP(HMX_WEI_B1, HMX_MOD_DP)
#define fGEN_TCG_M8_mxmema_wei_b1(SHORTCODE) \
    GEN_MATMUL_FXP(HMX_WEI_B1, HMX_MOD_ABOVE)
#define fGEN_TCG_M8_mxmemdi_wei_b1(SHORTCODE) \
    GEN_MATMUL_FXP(HMX_WEI_B1, HMX_MOD_DI)

/* 1-bit signed weights */
#define fGEN_TCG_M8_mxmem_wei_sb1(SHORTCODE) \
    GEN_MATMUL_FXP(HMX_WEI_SB1, HMX_MOD_NORMAL)
#define fGEN_TCG_M8_mxmems_wei_sb1(SHORTCODE) \
    GEN_MATMUL_FXP(HMX_WEI_SB1, HMX_MOD_SINGLE)
#define fGEN_TCG_M8_mxmemdr_wei_sb1(SHORTCODE) \
    GEN_MATMUL_FXP(HMX_WEI_SB1, HMX_MOD_DR)
#define fGEN_TCG_M8_mxmemdp_wei_sb1(SHORTCODE) \
    GEN_MATMUL_FXP(HMX_WEI_SB1, HMX_MOD_DP)
#define fGEN_TCG_M8_mxmema_wei_sb1(SHORTCODE) \
    GEN_MATMUL_FXP(HMX_WEI_SB1, HMX_MOD_ABOVE)
#define fGEN_TCG_M8_mxmemdi_wei_sb1(SHORTCODE) \
    GEN_MATMUL_FXP(HMX_WEI_SB1, HMX_MOD_DI)

/* Nibble 2x weights */
#define fGEN_TCG_M8_mxmem_wei_n_2x(SHORTCODE) \
    GEN_MATMUL_FXP(HMX_WEI_N_2X, HMX_MOD_NORMAL)
#define fGEN_TCG_M8_mxmems_wei_n_2x(SHORTCODE) \
    GEN_MATMUL_FXP(HMX_WEI_N_2X, HMX_MOD_SINGLE)
#define fGEN_TCG_M8_mxmemdr_wei_n_2x(SHORTCODE) \
    GEN_MATMUL_FXP(HMX_WEI_N_2X, HMX_MOD_DR)
#define fGEN_TCG_M8_mxmemdp_wei_n_2x(SHORTCODE) \
    GEN_MATMUL_FXP(HMX_WEI_N_2X, HMX_MOD_DP)
#define fGEN_TCG_M8_mxmema_wei_n_2x(SHORTCODE) \
    GEN_MATMUL_FXP(HMX_WEI_N_2X, HMX_MOD_ABOVE)
#define fGEN_TCG_M8_mxmemdi_wei_n_2x(SHORTCODE) \
    GEN_MATMUL_FXP(HMX_WEI_N_2X, HMX_MOD_DI)

/*
 * Weight load + multiply FP - call C helper
 */

#define GEN_MATMUL_FP(wei_type, mod) \
    gen_helper_hmx_matmul_fp(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_WEI(wei_type, mod)))

/* Half-float weights */
#define fGEN_TCG_M8_mxmem_wei_hf(SHORTCODE) \
    GEN_MATMUL_FP(HMX_WEI_HF, HMX_MOD_NORMAL)
#define fGEN_TCG_M8_mxmems_wei_hf(SHORTCODE) \
    GEN_MATMUL_FP(HMX_WEI_HF, HMX_MOD_SINGLE)
#define fGEN_TCG_M8_mxmemdr_wei_hf(SHORTCODE) \
    GEN_MATMUL_FP(HMX_WEI_HF, HMX_MOD_DR)
#define fGEN_TCG_M8_mxmemdp_wei_hf(SHORTCODE) \
    GEN_MATMUL_FP(HMX_WEI_HF, HMX_MOD_DP)
#define fGEN_TCG_M8_mxmema_wei_hf(SHORTCODE) \
    GEN_MATMUL_FP(HMX_WEI_HF, HMX_MOD_ABOVE)
#define fGEN_TCG_M8_mxmemdi_wei_hf(SHORTCODE) \
    GEN_MATMUL_FP(HMX_WEI_HF, HMX_MOD_DI)

/* FP8 weights */
#define fGEN_TCG_M8_mxmem_wei_f8(SHORTCODE) \
    GEN_MATMUL_FP(HMX_WEI_F8, HMX_MOD_NORMAL)
#define fGEN_TCG_M8_mxmems_wei_f8(SHORTCODE) \
    GEN_MATMUL_FP(HMX_WEI_F8, HMX_MOD_SINGLE)
#define fGEN_TCG_M8_mxmemdr_wei_f8(SHORTCODE) \
    GEN_MATMUL_FP(HMX_WEI_F8, HMX_MOD_DR)
#define fGEN_TCG_M8_mxmemdp_wei_f8(SHORTCODE) \
    GEN_MATMUL_FP(HMX_WEI_F8, HMX_MOD_DP)
#define fGEN_TCG_M8_mxmema_wei_f8(SHORTCODE) \
    GEN_MATMUL_FP(HMX_WEI_F8, HMX_MOD_ABOVE)
#define fGEN_TCG_M8_mxmemdi_wei_f8(SHORTCODE) \
    GEN_MATMUL_FP(HMX_WEI_F8, HMX_MOD_DI)

#endif /* HEXAGON_GEN_TCG_HMX_H */
