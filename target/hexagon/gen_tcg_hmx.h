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

#endif /* HEXAGON_GEN_TCG_HMX_H */
