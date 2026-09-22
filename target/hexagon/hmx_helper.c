/*
 * Hexagon HMX (Matrix eXtensions) Helper Functions
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * TCG helper implementations for HMX coprocessor instructions.
 */

#include "qemu/osdep.h"
#include "cpu.h"
#include "exec/helper-proto.h"
#include "accel/tcg/cpu-ldst.h"
#include "hmx_state.h"

static inline const HmxConfig *hmx_cfg_from_env(CPUHexagonState *env)
{
    return &env_archcpu(env)->hmx_cfg;
}

/*
 * M8_mxclracc - clear both FXP accumulator sets (primary and secondary).
 */
void HELPER(hmx_clracc)(CPUHexagonState *env)
{
    HmxState *hmx = env->hmx_state;

    memset(&hmx->acc[0].fxp_primary, 0, sizeof(HmxAccFxp));
    memset(&hmx->acc[0].fxp_secondary, 0, sizeof(HmxAccFxp));
    memset(&hmx->acc[1].fxp_primary, 0, sizeof(HmxAccFxp));
    memset(&hmx->acc[1].fxp_secondary, 0, sizeof(HmxAccFxp));
}

/*
 * M8_mxclracc_hf - clear both FP accumulator sets.
 *
 * hmx_fp_uses_xfp is always false for now (see hmx_config.c), so this
 * only needs the plain-double zero; the XFP true-zero path lands with
 * the rest of the XFP accumulator support.
 */
void HELPER(hmx_clracc_hf)(CPUHexagonState *env)
{
    HmxState *hmx = env->hmx_state;

    g_assert(!hmx_cfg_from_env(env)->hmx_fp_uses_xfp);
    memset(&hmx->acc[0].fp_primary, 0, sizeof(HmxAccFp));
    memset(&hmx->acc[0].fp_secondary, 0, sizeof(HmxAccFp));
    memset(&hmx->acc[1].fp_primary, 0, sizeof(HmxAccFp));
    memset(&hmx->acc[1].fp_secondary, 0, sizeof(HmxAccFp));
}

/*
 * M8_mxaccshl - shift both FXP accumulator sets' primary and secondary
 * accumulators left by 16 bits. Removed in v79 (see tag_rev_info.c.inc).
 */
void HELPER(hmx_accshl)(CPUHexagonState *env)
{
    HmxState *hmx = env->hmx_state;

    for (int set = 0; set < HMX_NUM_ACC_SETS; set++) {
        HmxAccFxp *acc = &hmx->acc[set].fxp_primary;
        for (int s = 0; s < HMX_SPATIAL_DIM_FXP; s++) {
            for (int o = 0; o < HMX_OUTPUT_CHANNELS; o++) {
                acc->data[s][o] = (int32_t)((uint32_t)acc->data[s][o] << 16);
            }
        }
        acc = &hmx->acc[set].fxp_secondary;
        for (int s = 0; s < HMX_SPATIAL_DIM_FXP; s++) {
            for (int o = 0; o < HMX_OUTPUT_CHANNELS; o++) {
                acc->data[s][o] = (int32_t)((uint32_t)acc->data[s][o] << 16);
            }
        }
    }
}

/*
 * M8_mxmem_bias / M8_mxmem2_bias - load HMX bias registers from memory.
 *
 * Bias set is (rs & 0x3); pre-v75 CPUs only have one bias group, so the
 * set is forced to 0.
 *
 * mxmem (is_mxmem2 == false) transfers only the low 32 bits of each of
 * mx_cols channels, packed contiguously.  mxmem2 additionally transfers
 * the high 32 bits, at a memory offset of HMX_BIAS_HIGH_WORD_OFFSET
 * bytes past the low half -- the HVX vector-length stride between the
 * two halves in the real bias-vector layout, not a function of mx_cols.
 */
void HELPER(hmx_bias_load)(CPUHexagonState *env, uint32_t rs,
                           uint32_t is_mxmem2)
{
    HmxState *hmx = env->hmx_state;
    const HmxConfig *hmx_cfg = hmx_cfg_from_env(env);
    uintptr_t ra = GETPC();
    uint32_t entry_bytes = is_mxmem2 ? HMX_BIAS_ENTRY_BYTES
                                     : HMX_OUTPUT_WORD_BYTES;
    uint32_t bias_bytes = hmx_cfg->mx_cols * entry_bytes;
    uint32_t align_mask = ~(bias_bytes - 1u);
    uint32_t addr = rs & align_mask;
    uint32_t set = rs & 0x3;

    if (env_archcpu(env)->cfg.hex_def->hex_version < HEX_VER_V75) {
        set = 0;
    }

    if (is_mxmem2) {
        for (uint32_t i = 0; i < hmx_cfg->mx_cols; i++) {
            uint32_t lo = cpu_ldl_le_data_ra(
                env, addr + i * HMX_OUTPUT_WORD_BYTES, ra);
            uint32_t hi = cpu_ldl_le_data_ra(
                env, addr + HMX_BIAS_HIGH_WORD_OFFSET
                     + i * HMX_OUTPUT_WORD_BYTES, ra);
            hmx->bias_raw[set][i] = ((uint64_t)hi << 32) | lo;
        }
    } else {
        for (uint32_t i = 0; i < hmx_cfg->mx_cols; i++) {
            uint32_t lo = cpu_ldl_le_data_ra(
                env, addr + i * HMX_OUTPUT_WORD_BYTES, ra);
            hmx->bias_raw[set][i] = lo;
        }
    }
}

/*
 * M8_mxmem_st_bias / M8_mxmem2_st_bias - store HMX bias registers to
 * memory. See HELPER(hmx_bias_load) for the addressing/layout rationale.
 */
void HELPER(hmx_bias_store)(CPUHexagonState *env, uint32_t rs,
                            uint32_t is_mxmem2)
{
    HmxState *hmx = env->hmx_state;
    const HmxConfig *hmx_cfg = hmx_cfg_from_env(env);
    uintptr_t ra = GETPC();
    uint32_t entry_bytes = is_mxmem2 ? HMX_BIAS_ENTRY_BYTES
                                     : HMX_OUTPUT_WORD_BYTES;
    uint32_t bias_bytes = hmx_cfg->mx_cols * entry_bytes;
    uint32_t align_mask = ~(bias_bytes - 1u);
    uint32_t addr = rs & align_mask;
    uint32_t set = rs & 0x3;

    if (env_archcpu(env)->cfg.hex_def->hex_version < HEX_VER_V75) {
        set = 0;
    }

    if (is_mxmem2) {
        for (uint32_t i = 0; i < hmx_cfg->mx_cols; i++) {
            uint64_t raw = hmx->bias_raw[set][i];
            cpu_stl_le_data_ra(env, addr + i * HMX_OUTPUT_WORD_BYTES,
                               (uint32_t)raw, ra);
            cpu_stl_le_data_ra(env, addr + HMX_BIAS_HIGH_WORD_OFFSET
                                    + i * HMX_OUTPUT_WORD_BYTES,
                               (uint32_t)(raw >> 32), ra);
        }
    } else {
        for (uint32_t i = 0; i < hmx_cfg->mx_cols; i++) {
            uint64_t raw = hmx->bias_raw[set][i];
            cpu_stl_le_data_ra(env, addr + i * HMX_OUTPUT_WORD_BYTES,
                               (uint32_t)raw, ra);
        }
    }
}
