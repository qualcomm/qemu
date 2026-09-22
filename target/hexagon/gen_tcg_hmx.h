/*
 * Hexagon HMX TCG Code Generation
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Most HMX operations don't fit idef-parser's normal per-GPR codegen
 * model (they read/write hmx_state.h's HmxState via env->hmx_state,
 * bulk-load memory, or need helper functions that operate on more
 * than idef-parser's register/immediate operands). Those get an
 * fGEN_TCG_<tag>() override here, following the same mechanism HVX
 * uses in gen_tcg_hvx.h for its own non-idef-parser instructions.
 */

#ifndef HEXAGON_GEN_TCG_HMX_H
#define HEXAGON_GEN_TCG_HMX_H

#include "hmx_state.h"

#define HMX_STATE_PTR_OFS offsetof(CPUHexagonState, hmx_state)

/*
 * M8_mxclracc / M8_mxclracc_hf - clear both accumulator sets.
 */
#define fGEN_TCG_M8_mxclracc(SHORTCODE) \
    gen_helper_hmx_clracc(tcg_env)
#define fGEN_TCG_M8_mxclracc_hf(SHORTCODE) \
    gen_helper_hmx_clracc_hf(tcg_env)

/*
 * M8_mxswap / M8_mxswap_hf - toggle the current accumulator set index
 * (0 <-> 1). No memory access or arithmetic, so this is plain inline
 * TCG rather than a helper call.
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
 * M8_mxaccshl - shift both FXP accumulator sets left by 16.
 */
#define fGEN_TCG_M8_mxaccshl(SHORTCODE) \
    gen_helper_hmx_accshl(tcg_env)

/*
 * Debug-print instructions: no guest-visible effect.
 */
#define fGEN_TCG_M8_pv64(SHORTCODE)     do { } while (0)
#define fGEN_TCG_M8_pv64d(SHORTCODE)    do { } while (0)
#define fGEN_TCG_M8_pv64fp(SHORTCODE)   do { } while (0)
#define fGEN_TCG_M8_pv64dfp(SHORTCODE)  do { } while (0)

/*
 * M8_mxmem_bias / M8_mxmem2_bias / M8_mxmem_st_bias / M8_mxmem2_st_bias
 * - bias register load/store.
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
 * M8_mxmem*_act_ub - activation load (spatial-major, block mode).
 */
#define fGEN_TCG_M8_mxmem_blk_sm_act_ub(SHORTCODE) \
    gen_helper_hmx_act_load(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_ACT(HMX_ACT_UB, HMX_ACT_FMT_SM, \
                                      HMX_ACT_BLK)))

/*
 * M8_mxmem_wei_b - byte weight load + FXP matrix multiply against the
 * activation latched by the preceding act-load instruction.
 */
#define fGEN_TCG_M8_mxmem_wei_b(SHORTCODE) \
    gen_helper_hmx_matmul_fxp(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_WEI(HMX_WEI_B, HMX_MOD_NORMAL)))

/*
 * M8_mxcvtr_sat_ub - legacy convert-and-store (spatial-major, byte,
 * saturating, direction AFTER, clear accumulator).
 */
#define fGEN_TCG_M8_mxcvtr_sat_ub(SHORTCODE) \
    gen_helper_hmx_cvt_transfer(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_CVT(HMX_CVT_RIGHT, HMX_CVT_FMT_UB_SM, \
                                      0, 0)))

/*
 * M8_mxmem*_act_{ub,hf,f8} - activation load, all formats/modifiers.
 * HELPER(hmx_act_load) already handles every type/format/modifier
 * combination, so these are all straightforward.
 */
#define fGEN_TCG_M8_mxmem_sm_act_ub(SHORTCODE) \
    gen_helper_hmx_act_load(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_ACT(HMX_ACT_UB, HMX_ACT_FMT_SM, \
                                      HMX_ACT_NOBLK)))
#define fGEN_TCG_M8_mxmemu_blk_sm_act_ub(SHORTCODE) \
    gen_helper_hmx_act_load(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_ACT(HMX_ACT_UB, HMX_ACT_FMT_SM, \
                                      HMX_ACT_BLK_U)))
#define fGEN_TCG_M8_mxmems_blk_sm_act_ub(SHORTCODE) \
    gen_helper_hmx_act_load(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_ACT(HMX_ACT_UB, HMX_ACT_FMT_SM, \
                                      HMX_ACT_BLK_S)))
#define fGEN_TCG_M8_mxmemd_blk_sm_act_ub(SHORTCODE) \
    gen_helper_hmx_act_load(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_ACT(HMX_ACT_UB, HMX_ACT_FMT_SM, \
                                      HMX_ACT_BLK_D)))
#define fGEN_TCG_M8_mxmem_blk_sm_act_hf(SHORTCODE) \
    gen_helper_hmx_act_load(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_ACT(HMX_ACT_HF, HMX_ACT_FMT_SM, \
                                      HMX_ACT_BLK)))
#define fGEN_TCG_M8_mxmem_sm_act_hf(SHORTCODE) \
    gen_helper_hmx_act_load(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_ACT(HMX_ACT_HF, HMX_ACT_FMT_SM, \
                                      HMX_ACT_NOBLK)))
#define fGEN_TCG_M8_mxmemu_blk_sm_act_hf(SHORTCODE) \
    gen_helper_hmx_act_load(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_ACT(HMX_ACT_HF, HMX_ACT_FMT_SM, \
                                      HMX_ACT_BLK_U)))
#define fGEN_TCG_M8_mxmems_blk_sm_act_hf(SHORTCODE) \
    gen_helper_hmx_act_load(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_ACT(HMX_ACT_HF, HMX_ACT_FMT_SM, \
                                      HMX_ACT_BLK_S)))
#define fGEN_TCG_M8_mxmemd_blk_sm_act_hf(SHORTCODE) \
    gen_helper_hmx_act_load(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_ACT(HMX_ACT_HF, HMX_ACT_FMT_SM, \
                                      HMX_ACT_BLK_D)))
#define fGEN_TCG_M8_mxmem_blk_sm_act_f8(SHORTCODE) \
    gen_helper_hmx_act_load(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_ACT(HMX_ACT_F8, HMX_ACT_FMT_SM, \
                                      HMX_ACT_BLK)))
#define fGEN_TCG_M8_mxmem_sm_act_f8(SHORTCODE) \
    gen_helper_hmx_act_load(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_ACT(HMX_ACT_F8, HMX_ACT_FMT_SM, \
                                      HMX_ACT_NOBLK)))
#define fGEN_TCG_M8_mxmemu_blk_sm_act_f8(SHORTCODE) \
    gen_helper_hmx_act_load(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_ACT(HMX_ACT_F8, HMX_ACT_FMT_SM, \
                                      HMX_ACT_BLK_U)))
#define fGEN_TCG_M8_mxmems_blk_sm_act_f8(SHORTCODE) \
    gen_helper_hmx_act_load(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_ACT(HMX_ACT_F8, HMX_ACT_FMT_SM, \
                                      HMX_ACT_BLK_S)))
#define fGEN_TCG_M8_mxmemd_blk_sm_act_f8(SHORTCODE) \
    gen_helper_hmx_act_load(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_ACT(HMX_ACT_F8, HMX_ACT_FMT_SM, \
                                      HMX_ACT_BLK_D)))
#define fGEN_TCG_M8_mxmem_blk_dm_act_ub(SHORTCODE) \
    gen_helper_hmx_act_load(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_ACT(HMX_ACT_UB, HMX_ACT_FMT_DM, \
                                      HMX_ACT_BLK)))
#define fGEN_TCG_M8_mxmem_dm_act_ub(SHORTCODE) \
    gen_helper_hmx_act_load(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_ACT(HMX_ACT_UB, HMX_ACT_FMT_DM, \
                                      HMX_ACT_NOBLK)))
#define fGEN_TCG_M8_mxmemu_blk_dm_act_ub(SHORTCODE) \
    gen_helper_hmx_act_load(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_ACT(HMX_ACT_UB, HMX_ACT_FMT_DM, \
                                      HMX_ACT_BLK_U)))
#define fGEN_TCG_M8_mxmems_blk_dm_act_ub(SHORTCODE) \
    gen_helper_hmx_act_load(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_ACT(HMX_ACT_UB, HMX_ACT_FMT_DM, \
                                      HMX_ACT_BLK_S)))
#define fGEN_TCG_M8_mxmemd_blk_dm_act_ub(SHORTCODE) \
    gen_helper_hmx_act_load(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_ACT(HMX_ACT_UB, HMX_ACT_FMT_DM, \
                                      HMX_ACT_BLK_D)))

/*
 * M8_mxmem*_wei_{b,sm,n,c,sc,b1,sb1,n_2x} - FXP weight load +
 * matrix multiply, all modifiers. HELPER(hmx_matmul_fxp)'s
 * extraction table and modifier switch already handle every
 * combination. HF/F8 (XFP float weights) aren't here -- those
 * need HELPER(hmx_matmul_fp), which doesn't exist yet.
 */
#define fGEN_TCG_M8_mxmem_wei_sm(SHORTCODE) \
    gen_helper_hmx_matmul_fxp(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_WEI(HMX_WEI_SM, HMX_MOD_NORMAL)))
#define fGEN_TCG_M8_mxmem_wei_n(SHORTCODE) \
    gen_helper_hmx_matmul_fxp(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_WEI(HMX_WEI_N, HMX_MOD_NORMAL)))
#define fGEN_TCG_M8_mxmem_wei_c(SHORTCODE) \
    gen_helper_hmx_matmul_fxp(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_WEI(HMX_WEI_C, HMX_MOD_NORMAL)))
#define fGEN_TCG_M8_mxmem_wei_sc(SHORTCODE) \
    gen_helper_hmx_matmul_fxp(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_WEI(HMX_WEI_SC, HMX_MOD_NORMAL)))
#define fGEN_TCG_M8_mxmem_wei_b1(SHORTCODE) \
    gen_helper_hmx_matmul_fxp(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_WEI(HMX_WEI_B1, HMX_MOD_NORMAL)))
#define fGEN_TCG_M8_mxmem_wei_sb1(SHORTCODE) \
    gen_helper_hmx_matmul_fxp(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_WEI(HMX_WEI_SB1, HMX_MOD_NORMAL)))
#define fGEN_TCG_M8_mxmem_wei_n_2x(SHORTCODE) \
    gen_helper_hmx_matmul_fxp(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_WEI(HMX_WEI_N_2X, HMX_MOD_NORMAL)))
#define fGEN_TCG_M8_mxmems_wei_b(SHORTCODE) \
    gen_helper_hmx_matmul_fxp(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_WEI(HMX_WEI_B, HMX_MOD_SINGLE)))
#define fGEN_TCG_M8_mxmems_wei_sm(SHORTCODE) \
    gen_helper_hmx_matmul_fxp(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_WEI(HMX_WEI_SM, HMX_MOD_SINGLE)))
#define fGEN_TCG_M8_mxmems_wei_n(SHORTCODE) \
    gen_helper_hmx_matmul_fxp(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_WEI(HMX_WEI_N, HMX_MOD_SINGLE)))
#define fGEN_TCG_M8_mxmems_wei_c(SHORTCODE) \
    gen_helper_hmx_matmul_fxp(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_WEI(HMX_WEI_C, HMX_MOD_SINGLE)))
#define fGEN_TCG_M8_mxmems_wei_sc(SHORTCODE) \
    gen_helper_hmx_matmul_fxp(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_WEI(HMX_WEI_SC, HMX_MOD_SINGLE)))
#define fGEN_TCG_M8_mxmems_wei_b1(SHORTCODE) \
    gen_helper_hmx_matmul_fxp(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_WEI(HMX_WEI_B1, HMX_MOD_SINGLE)))
#define fGEN_TCG_M8_mxmems_wei_sb1(SHORTCODE) \
    gen_helper_hmx_matmul_fxp(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_WEI(HMX_WEI_SB1, HMX_MOD_SINGLE)))
#define fGEN_TCG_M8_mxmems_wei_n_2x(SHORTCODE) \
    gen_helper_hmx_matmul_fxp(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_WEI(HMX_WEI_N_2X, HMX_MOD_SINGLE)))
#define fGEN_TCG_M8_mxmemdr_wei_b(SHORTCODE) \
    gen_helper_hmx_matmul_fxp(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_WEI(HMX_WEI_B, HMX_MOD_DR)))
#define fGEN_TCG_M8_mxmemdr_wei_sm(SHORTCODE) \
    gen_helper_hmx_matmul_fxp(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_WEI(HMX_WEI_SM, HMX_MOD_DR)))
#define fGEN_TCG_M8_mxmemdr_wei_n(SHORTCODE) \
    gen_helper_hmx_matmul_fxp(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_WEI(HMX_WEI_N, HMX_MOD_DR)))
#define fGEN_TCG_M8_mxmemdr_wei_c(SHORTCODE) \
    gen_helper_hmx_matmul_fxp(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_WEI(HMX_WEI_C, HMX_MOD_DR)))
#define fGEN_TCG_M8_mxmemdr_wei_sc(SHORTCODE) \
    gen_helper_hmx_matmul_fxp(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_WEI(HMX_WEI_SC, HMX_MOD_DR)))
#define fGEN_TCG_M8_mxmemdr_wei_b1(SHORTCODE) \
    gen_helper_hmx_matmul_fxp(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_WEI(HMX_WEI_B1, HMX_MOD_DR)))
#define fGEN_TCG_M8_mxmemdr_wei_sb1(SHORTCODE) \
    gen_helper_hmx_matmul_fxp(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_WEI(HMX_WEI_SB1, HMX_MOD_DR)))
#define fGEN_TCG_M8_mxmemdr_wei_n_2x(SHORTCODE) \
    gen_helper_hmx_matmul_fxp(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_WEI(HMX_WEI_N_2X, HMX_MOD_DR)))
#define fGEN_TCG_M8_mxmemdp_wei_b(SHORTCODE) \
    gen_helper_hmx_matmul_fxp(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_WEI(HMX_WEI_B, HMX_MOD_DP)))
#define fGEN_TCG_M8_mxmemdp_wei_sm(SHORTCODE) \
    gen_helper_hmx_matmul_fxp(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_WEI(HMX_WEI_SM, HMX_MOD_DP)))
#define fGEN_TCG_M8_mxmemdp_wei_n(SHORTCODE) \
    gen_helper_hmx_matmul_fxp(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_WEI(HMX_WEI_N, HMX_MOD_DP)))
#define fGEN_TCG_M8_mxmemdp_wei_c(SHORTCODE) \
    gen_helper_hmx_matmul_fxp(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_WEI(HMX_WEI_C, HMX_MOD_DP)))
#define fGEN_TCG_M8_mxmemdp_wei_sc(SHORTCODE) \
    gen_helper_hmx_matmul_fxp(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_WEI(HMX_WEI_SC, HMX_MOD_DP)))
#define fGEN_TCG_M8_mxmemdp_wei_b1(SHORTCODE) \
    gen_helper_hmx_matmul_fxp(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_WEI(HMX_WEI_B1, HMX_MOD_DP)))
#define fGEN_TCG_M8_mxmemdp_wei_sb1(SHORTCODE) \
    gen_helper_hmx_matmul_fxp(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_WEI(HMX_WEI_SB1, HMX_MOD_DP)))
#define fGEN_TCG_M8_mxmemdp_wei_n_2x(SHORTCODE) \
    gen_helper_hmx_matmul_fxp(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_WEI(HMX_WEI_N_2X, HMX_MOD_DP)))
#define fGEN_TCG_M8_mxmema_wei_b(SHORTCODE) \
    gen_helper_hmx_matmul_fxp(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_WEI(HMX_WEI_B, HMX_MOD_ABOVE)))
#define fGEN_TCG_M8_mxmema_wei_sm(SHORTCODE) \
    gen_helper_hmx_matmul_fxp(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_WEI(HMX_WEI_SM, HMX_MOD_ABOVE)))
#define fGEN_TCG_M8_mxmema_wei_n(SHORTCODE) \
    gen_helper_hmx_matmul_fxp(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_WEI(HMX_WEI_N, HMX_MOD_ABOVE)))
#define fGEN_TCG_M8_mxmema_wei_c(SHORTCODE) \
    gen_helper_hmx_matmul_fxp(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_WEI(HMX_WEI_C, HMX_MOD_ABOVE)))
#define fGEN_TCG_M8_mxmema_wei_sc(SHORTCODE) \
    gen_helper_hmx_matmul_fxp(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_WEI(HMX_WEI_SC, HMX_MOD_ABOVE)))
#define fGEN_TCG_M8_mxmema_wei_b1(SHORTCODE) \
    gen_helper_hmx_matmul_fxp(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_WEI(HMX_WEI_B1, HMX_MOD_ABOVE)))
#define fGEN_TCG_M8_mxmema_wei_sb1(SHORTCODE) \
    gen_helper_hmx_matmul_fxp(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_WEI(HMX_WEI_SB1, HMX_MOD_ABOVE)))
#define fGEN_TCG_M8_mxmema_wei_n_2x(SHORTCODE) \
    gen_helper_hmx_matmul_fxp(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_WEI(HMX_WEI_N_2X, HMX_MOD_ABOVE)))
#define fGEN_TCG_M8_mxmemdi_wei_b(SHORTCODE) \
    gen_helper_hmx_matmul_fxp(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_WEI(HMX_WEI_B, HMX_MOD_DI)))
#define fGEN_TCG_M8_mxmemdi_wei_sm(SHORTCODE) \
    gen_helper_hmx_matmul_fxp(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_WEI(HMX_WEI_SM, HMX_MOD_DI)))
#define fGEN_TCG_M8_mxmemdi_wei_n(SHORTCODE) \
    gen_helper_hmx_matmul_fxp(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_WEI(HMX_WEI_N, HMX_MOD_DI)))
#define fGEN_TCG_M8_mxmemdi_wei_c(SHORTCODE) \
    gen_helper_hmx_matmul_fxp(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_WEI(HMX_WEI_C, HMX_MOD_DI)))
#define fGEN_TCG_M8_mxmemdi_wei_sc(SHORTCODE) \
    gen_helper_hmx_matmul_fxp(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_WEI(HMX_WEI_SC, HMX_MOD_DI)))
#define fGEN_TCG_M8_mxmemdi_wei_b1(SHORTCODE) \
    gen_helper_hmx_matmul_fxp(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_WEI(HMX_WEI_B1, HMX_MOD_DI)))
#define fGEN_TCG_M8_mxmemdi_wei_sb1(SHORTCODE) \
    gen_helper_hmx_matmul_fxp(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_WEI(HMX_WEI_SB1, HMX_MOD_DI)))
#define fGEN_TCG_M8_mxmemdi_wei_n_2x(SHORTCODE) \
    gen_helper_hmx_matmul_fxp(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_WEI(HMX_WEI_N_2X, HMX_MOD_DI)))

/*
 * M8_mxcvt{l,r}[_dm][_sat]_ub[_r] - legacy convert-and-store,
 * all UB format/direction/saturation/retain combinations.
 * HELPER(hmx_cvt_transfer)'s UB path already handles all of
 * them via its fmt/dir/relu/retain parameters.
 */
#define fGEN_TCG_M8_mxcvtl_sat_ub(SHORTCODE) \
    gen_helper_hmx_cvt_transfer(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_CVT(HMX_CVT_LEFT, HMX_CVT_FMT_UB_SM, \
                                      0, 0)))
#define fGEN_TCG_M8_mxcvtl_sat_ub_r(SHORTCODE) \
    gen_helper_hmx_cvt_transfer(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_CVT(HMX_CVT_LEFT, HMX_CVT_FMT_UB_SM, \
                                      0, 1)))
#define fGEN_TCG_M8_mxcvtl_ub(SHORTCODE) \
    gen_helper_hmx_cvt_transfer(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_CVT(HMX_CVT_LEFT, HMX_CVT_FMT_UB_SM, \
                                      1, 0)))
#define fGEN_TCG_M8_mxcvtl_ub_r(SHORTCODE) \
    gen_helper_hmx_cvt_transfer(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_CVT(HMX_CVT_LEFT, HMX_CVT_FMT_UB_SM, \
                                      1, 1)))
#define fGEN_TCG_M8_mxcvtl_dm_sat_ub(SHORTCODE) \
    gen_helper_hmx_cvt_transfer(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_CVT(HMX_CVT_LEFT, HMX_CVT_FMT_UB_DM, \
                                      0, 0)))
#define fGEN_TCG_M8_mxcvtl_dm_sat_ub_r(SHORTCODE) \
    gen_helper_hmx_cvt_transfer(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_CVT(HMX_CVT_LEFT, HMX_CVT_FMT_UB_DM, \
                                      0, 1)))
#define fGEN_TCG_M8_mxcvtl_dm_ub(SHORTCODE) \
    gen_helper_hmx_cvt_transfer(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_CVT(HMX_CVT_LEFT, HMX_CVT_FMT_UB_DM, \
                                      1, 0)))
#define fGEN_TCG_M8_mxcvtl_dm_ub_r(SHORTCODE) \
    gen_helper_hmx_cvt_transfer(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_CVT(HMX_CVT_LEFT, HMX_CVT_FMT_UB_DM, \
                                      1, 1)))
#define fGEN_TCG_M8_mxcvtr_sat_ub_r(SHORTCODE) \
    gen_helper_hmx_cvt_transfer(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_CVT(HMX_CVT_RIGHT, HMX_CVT_FMT_UB_SM, \
                                      0, 1)))
#define fGEN_TCG_M8_mxcvtr_ub(SHORTCODE) \
    gen_helper_hmx_cvt_transfer(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_CVT(HMX_CVT_RIGHT, HMX_CVT_FMT_UB_SM, \
                                      1, 0)))
#define fGEN_TCG_M8_mxcvtr_ub_r(SHORTCODE) \
    gen_helper_hmx_cvt_transfer(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_CVT(HMX_CVT_RIGHT, HMX_CVT_FMT_UB_SM, \
                                      1, 1)))
#define fGEN_TCG_M8_mxcvtr_dm_sat_ub(SHORTCODE) \
    gen_helper_hmx_cvt_transfer(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_CVT(HMX_CVT_RIGHT, HMX_CVT_FMT_UB_DM, \
                                      0, 0)))
#define fGEN_TCG_M8_mxcvtr_dm_sat_ub_r(SHORTCODE) \
    gen_helper_hmx_cvt_transfer(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_CVT(HMX_CVT_RIGHT, HMX_CVT_FMT_UB_DM, \
                                      0, 1)))
#define fGEN_TCG_M8_mxcvtr_dm_ub(SHORTCODE) \
    gen_helper_hmx_cvt_transfer(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_CVT(HMX_CVT_RIGHT, HMX_CVT_FMT_UB_DM, \
                                      1, 0)))
#define fGEN_TCG_M8_mxcvtr_dm_ub_r(SHORTCODE) \
    gen_helper_hmx_cvt_transfer(tcg_env, RsV, RtV, \
        tcg_constant_i32(HMX_PACK_CVT(HMX_CVT_RIGHT, HMX_CVT_FMT_UB_DM, \
                                      1, 1)))

#endif /* HEXAGON_GEN_TCG_HMX_H */
