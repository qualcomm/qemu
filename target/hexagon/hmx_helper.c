/*
 * Hexagon HMX (Matrix eXtensions) Helper Functions
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * TCG helper implementations for HMX coprocessor instructions.
 */

#include "qemu/osdep.h"
#include "qemu/host-utils.h"
#include "cpu.h"
#include "exec/helper-proto.h"
#include "accel/tcg/cpu-ldst.h"
#include "hmx_state.h"

#ifndef CONFIG_INT128
#error "HMX FXP convert requires 128-bit integer support (CONFIG_INT128)"
#endif

static inline const HmxConfig *hmx_cfg_from_env(CPUHexagonState *env)
{
    return &env_archcpu(env)->hmx_cfg;
}

static inline HexagonVersion hmx_cpu_version(CPUHexagonState *env)
{
    return env_archcpu(env)->cfg.hex_def->hex_version;
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

    if (hmx_cpu_version(env) < HEX_VER_V75) {
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

    if (hmx_cpu_version(env) < HEX_VER_V75) {
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

/*
 * Compute multi-tap convolution parameters from activation Rs/Rt.
 *
 * Called from HELPER(hmx_act_load) to pre-compute tile masks, filter
 * positions, channel ranges, and tap counts that the subsequent weight
 * multiply instruction (HELPER(hmx_matmul_fxp)) reads back out of
 * HmxState.
 */
static void hmx_compute_act_params(const HmxConfig *hmx_cfg,
                                    HmxState *hmx,
                                    uint32_t rs, uint32_t rt,
                                    int act_fmt, int act_mod, int act_type)
{
    int format_offset = (act_fmt == HMX_ACT_FMT_SM) ? 2 : 0;
    uint32_t ch_mask = (hmx_cfg->mx_cols - 1) << format_offset;
    int is_flt = (act_type == HMX_ACT_HF) || (act_type == HMX_ACT_F8);

    hmx->format_offset = format_offset;
    hmx->dY = rt & ~2047;  /* offset to second activation crouton */

    /*
     * Channel range and group convolution detection.
     *
     * When ch_start > ch_stop (raw), the hardware enters group
     * convolution mode.  The leading-ones count of
     * ((ch_stop-ch_start)^ch_start^ch_stop) determines the group size
     * (32 >> cl1).  The channel indices are then masked to within the
     * group, and the matmul iterates over all groups.
     */
    int raw_start = (rs & ch_mask) >> format_offset;
    int raw_stop = (rt & ch_mask) >> format_offset;

    hmx->group_conv = (raw_start > raw_stop) &&
                      (act_mod != HMX_ACT_NOBLK);

    if (hmx->group_conv) {
        /* Count leading ones on an 8-bit value to determine group size. */
        uint8_t cl1_arg = (uint8_t)(((raw_stop - raw_start) ^
                                      raw_start ^ raw_stop) << 2);
        int cl1 = 0;
        while (cl1_arg & 0x80) {
            cl1++;
            cl1_arg <<= 1;
        }

        hmx->group_size = 32 >> cl1;
        hmx->group_count = 32 / hmx->group_size;

        int grp_mask = 0x1F >> cl1;
        int group_ch_start, group_ch_stop;

        if (hmx->group_count == 16) {
            group_ch_start = (raw_start & ~1) & grp_mask;
            group_ch_stop = (raw_stop | 1) & grp_mask;
        } else if (hmx->group_count <= 8) {
            group_ch_start = (raw_start & ~3) & grp_mask;
            group_ch_stop = (raw_stop | 3) & grp_mask;
        } else {
            group_ch_start = raw_start & grp_mask;
            group_ch_stop = raw_stop & grp_mask;
        }

        hmx->ch_start = group_ch_start;
        hmx->ch_stop = group_ch_stop + 1;  /* exclusive */
    } else {
        hmx->group_size = 32;
        hmx->group_count = 1;
        hmx->ch_start = raw_start & ~3;
        /* exclusive, rounded up to group of 4 */
        hmx->ch_stop = (raw_stop | 3) + 1;
    }

    /* Tile masks from Rt spatial bits */
    hmx->tile_x_mask = hmx_get_spatial_mask(~rt, format_offset);
    hmx->tile_y_mask = hmx_get_spatial_mask(rt, format_offset);

    /*
     * FP mode: clear LSB of tile masks so spatial iteration steps by 2.
     * Each FP16 value occupies 2 crouton bytes (adjacent spatial
     * positions), so the spatial loop must skip odd positions.
     */
    if (is_flt) {
        hmx->tile_x_mask &= ~1;
        hmx->tile_y_mask &= ~1;
    }

    /* Tile increments (lowest set bit of each mask) */
    hmx->tile_x_inc = hmx_get_masked_inc(hmx->tile_x_mask);
    hmx->tile_y_inc = hmx_get_masked_inc(hmx->tile_y_mask);

    /* Filter positions from Rs */
    hmx->fx = rs & hmx->tile_x_mask;
    hmx->fy = rs & hmx->tile_y_mask;

    /* Y-dimension tap parameters (determined by activation block type) */
    hmx->y_start = 0;
    hmx->y_stop = 0;
    hmx->y_dilate = 0;
    hmx->blocks = 1;

    switch (act_mod) {
    case HMX_ACT_BLK:   /* BLOCK: normal multi-tap */
        hmx->y_stop = hmx->fy;
        hmx->y_dilate = 0;
        break;
    case HMX_ACT_NOBLK:  /* DEEP: multi-block mode */
    {
        uint32_t dY = rt & ~2047;
        hmx->blocks = (dY >> 11) + 1;
        hmx->y_stop = 0;
        break;
    }
    case HMX_ACT_BLK_U:  /* ABOVE: taps from fy to end */
        hmx->y_start = hmx->fy;
        hmx->y_stop = hmx->tile_y_mask;
        break;
    case HMX_ACT_BLK_S:  /* SINGLE: one y tap at fy */
        hmx->y_start = hmx->fy;
        hmx->y_stop = hmx->fy;
        break;
    case HMX_ACT_BLK_D:  /* DILATE: with dilation */
        hmx->y_stop = hmx->fy;
        hmx->y_dilate = 1;
        break;
    }

    /*
     * Guard: if ch_start >= ch_stop after processing, no channels to
     * process.  blocks=0.
     */
    if (hmx->ch_start >= hmx->ch_stop && hmx->blocks == 1) {
        hmx->blocks = 0;
    }
}

/*
 * M8_mxmem*_act_* - load an activation crouton (or two, for multi-tap Y
 * convolution) into HmxState.act_buffer and latch the parameters the
 * subsequent weight-multiply instruction needs.
 *
 * F8 activations are expanded to FP16 in place after loading: a raw bit
 * shift with no exponent rebias (HMX F8 is designed so this is exact),
 * so the matmul's FP path can always read act_buffer as uint16_t[]
 * regardless of whether the source was HF or F8.
 */
void HELPER(hmx_act_load)(CPUHexagonState *env, uint32_t rs, uint32_t rt,
                          uint32_t params)
{
    HmxState *hmx = env->hmx_state;
    int act_type = HMX_UNPACK_ACT_TYPE(params);
    int act_fmt = HMX_UNPACK_ACT_FMT(params);
    uintptr_t ra = GETPC();

    /* 2KB-aligned base address */
    uint32_t base_addr = rs & 0xFFFFF800;

    /* Latch parameters for weight multiply */
    hmx->act_rs = rs;
    hmx->act_rt = rt;
    hmx->act_format = act_fmt;
    hmx->act_type = act_type;

    /* 256 MAC cycles for FP, 512 for FXP. */
    {
        int is_flt_act = (act_type == HMX_ACT_HF) || (act_type == HMX_ACT_F8);
        hmx->mac_cycle_limit = is_flt_act ? 256 : 512;
    }

    /*
     * FP8 odd-byte select: Rs[0] picks the even (0) or odd (1) byte of
     * each 16-bit activation word for F8 expansion.
     */
    hmx->is_f8_odd = (act_type == HMX_ACT_F8) ? (rs & 1) : 0;

    /* Compute multi-tap convolution parameters */
    int act_mod = HMX_UNPACK_ACT_MOD(params);
    hmx_compute_act_params(hmx_cfg_from_env(env), hmx, rs, rt, act_fmt,
                           act_mod, act_type);

    /*
     * Load second activation crouton for multi-tap Y convolution: when
     * (blocks == 1) and the y-tap range isn't degenerate.
     */
    int need_second = (hmx->blocks == 1) &&
        ((hmx->y_stop != hmx->y_start) || (hmx->y_start != 0));

    switch (act_type) {
    case HMX_ACT_UB:
    case HMX_ACT_HF:
        /* Load 2KB crouton using 32-bit word loads. */
        for (int i = 0; i < HMX_ACT_CROUTON_SIZE; i += 4) {
            uint32_t w = cpu_ldl_le_data_ra(env, base_addr + i, ra);
            stl_le_p(&hmx->act_buffer[i], w);
        }
        if (need_second) {
            uint32_t addr2 = base_addr + hmx->dY;
            for (int i = 0; i < HMX_ACT_CROUTON_SIZE; i += 4) {
                uint32_t w = cpu_ldl_le_data_ra(env, addr2 + i, ra);
                stl_le_p(&hmx->act_buffer[HMX_ACT_CROUTON_SIZE + i], w);
            }
        }
        break;
    case HMX_ACT_F8:
        for (int i = 0; i < HMX_ACT_CROUTON_SIZE; i += 4) {
            uint32_t w = cpu_ldl_le_data_ra(env, base_addr + i, ra);
            stl_le_p(&hmx->act_buffer[i], w);
        }
        /* Expand F8-to-FP16 in-place at even spatial positions */
        for (int s = HMX_SPATIAL_DIM_FP - 1; s >= 0; s--) {
            int crouton_s = s * 2;
            for (int c = HMX_INPUT_CHANNELS - 1; c >= 0; c--) {
                int off = hmx_act_offset_sm(crouton_s, c);
                uint16_t raw16 = hmx->act_buffer[off] |
                                 (hmx->act_buffer[off + 1] << 8);
                uint8_t f8 = (raw16 >> (hmx->is_f8_odd * 8)) & 0xff;
                uint16_t f16;
                if (f8 == 0x80) {
                    f16 = 0xFE00;
                } else {
                    f16 = ((f8 & 0x80) << 8) | ((f8 & 0x7F) << 7);
                }
                stw_le_p(&hmx->act_buffer[off], f16);
            }
        }
        if (need_second) {
            uint32_t addr2 = base_addr + hmx->dY;
            for (int i = 0; i < HMX_ACT_CROUTON_SIZE; i += 4) {
                uint32_t w = cpu_ldl_le_data_ra(env, addr2 + i, ra);
                stl_le_p(&hmx->act_buffer[HMX_ACT_CROUTON_SIZE + i], w);
            }
            for (int s = HMX_SPATIAL_DIM_FP - 1; s >= 0; s--) {
                int crouton_s = s * 2;
                for (int c = HMX_INPUT_CHANNELS - 1; c >= 0; c--) {
                    int off = hmx_act_offset_sm(crouton_s, c);
                    int buf_off = HMX_ACT_CROUTON_SIZE + off;
                    uint16_t raw16 = hmx->act_buffer[buf_off] |
                                     (hmx->act_buffer[buf_off + 1] << 8);
                    uint8_t f8 = (raw16 >> (hmx->is_f8_odd * 8)) & 0xff;
                    uint16_t f16;
                    if (f8 == 0x80) {
                        f16 = 0xFE00;
                    } else {
                        f16 = ((f8 & 0x80) << 8) | ((f8 & 0x7F) << 7);
                    }
                    stw_le_p(&hmx->act_buffer[buf_off], f16);
                }
            }
        }
        break;
    }
}

/*
 * Weight memory layout in VTCM (per 128B vector):
 *
 * Each 128B vector contains 32 output channels x 4 bytes.  Word at
 * offset (och * 4) contains weights for output channel 'och'.  Within
 * each 4-byte word, bytes map to input channels:
 *   byte[0] = input_ch_in_group 0
 *   byte[1] = input_ch_in_group 1
 *   byte[2] = input_ch_in_group 2
 *   byte[3] = input_ch_in_group 3
 *
 * For byte weights: 4 input channels per vector.
 * For nibble weights: 8 input channels per vector (lo/hi nibbles).
 * For crumb weights: 16 input channels per vector (4 crumbs/byte).
 * For bit weights: 32 input channels per vector (8 bits/byte).
 */

/*
 * Bulk-load one weight vector as little-endian int32 words.
 *
 * Zero-fill storage past mx_cols so fixed-size downstream loops consume
 * zero contributions for inactive output channels.
 */
static inline void hmx_preload_weight_vec(const HmxConfig *hmx_cfg,
                                           CPUHexagonState *env,
                                           uint32_t wei_base,
                                           int vec_idx,
                                           uint32_t *wei_words,
                                           uintptr_t ra)
{
    uint32_t base =
        wei_base + vec_idx * (hmx_cfg->mx_cols * HMX_OUTPUT_WORD_BYTES);
    for (int i = 0; i < hmx_cfg->mx_cols; i++) {
        wei_words[i] = cpu_ldl_le_data_ra(env, base + i * HMX_OUTPUT_WORD_BYTES,
                                          ra);
    }
    for (int i = hmx_cfg->mx_cols; i < HMX_OUTPUT_CHANNELS; i++) {
        wei_words[i] = 0;
    }
}

/* Byte: 4 stream indices per vector */
static inline void hmx_extract_weights_byte(
    const uint32_t *wei_words, int sub_idx, int8_t *weights)
{
    int shift = sub_idx * 8;
    for (int o = 0; o < HMX_OUTPUT_CHANNELS; o++) {
        weights[o] = (int8_t)((wei_words[o] >> shift) & 0xFF);
    }
}

/*
 * Sign-magnitude: same packing as byte, different interpretation.
 * Positive (bit7=0): result = magnitude.  Negative (bit7=1):
 * result = 0x7F ^ in = ~magnitude = -(magnitude+1), mapping
 * [0x80..0xFF] to [-1..-128] with no negative zero.
 */
static inline void hmx_extract_weights_sm(
    const uint32_t *wei_words, int sub_idx, int8_t *weights)
{
    int shift = sub_idx * 8;
    for (int o = 0; o < HMX_OUTPUT_CHANNELS; o++) {
        int8_t v = (int8_t)((wei_words[o] >> shift) & 0xFF);
        weights[o] = (((v >> 7) & 0x7F) ^ v);
    }
}

/*
 * Nibble: 8 stream indices per vector.  Extract 4-bit value and sign
 * extend: values 8-15 become -8..-1.
 */
static inline void hmx_extract_weights_nibble(
    const uint32_t *wei_words, int sub_idx, int8_t *weights)
{
    int byte_in_word = sub_idx % 4;
    int nibble_sel = sub_idx / 4;
    int shift = byte_in_word * 8 + nibble_sel * 4;
    for (int o = 0; o < HMX_OUTPUT_CHANNELS; o++) {
        int32_t raw = (wei_words[o] >> shift) & 0xF;
        weights[o] = (int8_t)((int32_t)(raw << 28) >> 28);
    }
}

/*
 * Crumb: 16 stream indices per vector.  Extract 2-bit value and sign
 * extend: {0,1,2,3} -> {0,1,-2,-1}.
 */
static inline void hmx_extract_weights_crumb(
    const uint32_t *wei_words, int sub_idx, int8_t *weights)
{
    int byte_in_word = sub_idx % 4;
    int crumb_sel = sub_idx / 4;
    int shift = byte_in_word * 8 + crumb_sel * 2;
    for (int o = 0; o < HMX_OUTPUT_CHANNELS; o++) {
        int32_t raw = (wei_words[o] >> shift) & 0x3;
        weights[o] = (int8_t)((int32_t)(raw << 30) >> 30);
    }
}

/* Signed crumb: lookup table {0,1,2,3} -> {2,1,-2,-1} */
static inline void hmx_extract_weights_signed_crumb(
    const uint32_t *wei_words, int sub_idx, int8_t *weights)
{
    static const int8_t sc_map[4] = { 2, 1, -2, -1 };
    int byte_in_word = sub_idx % 4;
    int crumb_sel = sub_idx / 4;
    int shift = byte_in_word * 8 + crumb_sel * 2;
    for (int o = 0; o < HMX_OUTPUT_CHANNELS; o++) {
        weights[o] = sc_map[(wei_words[o] >> shift) & 0x3];
    }
}

/* Bit (unsigned 1-bit): 32 stream indices per vector */
static inline void hmx_extract_weights_bit(
    const uint32_t *wei_words, int sub_idx, int8_t *weights)
{
    int byte_in_word = sub_idx % 4;
    int bit_sel = sub_idx / 4;
    int shift = byte_in_word * 8 + bit_sel;
    for (int o = 0; o < HMX_OUTPUT_CHANNELS; o++) {
        weights[o] = (wei_words[o] >> shift) & 0x1;
    }
}

/* Signed bit: {0,1} -> {+1,-1} */
static inline void hmx_extract_weights_signed_bit(
    const uint32_t *wei_words, int sub_idx, int8_t *weights)
{
    int byte_in_word = sub_idx % 4;
    int bit_sel = sub_idx / 4;
    int shift = byte_in_word * 8 + bit_sel;
    for (int o = 0; o < HMX_OUTPUT_CHANNELS; o++) {
        weights[o] = ((wei_words[o] >> shift) & 0x1) ? -1 : 1;
    }
}

/*
 * Byte weights are always signed int8_t at every revision: the
 * reference has no v71+ unsigned override for HMX_WEI_B (an earlier
 * QEMU attempt to add one produced all-0xff output for filters with
 * bytes > 0x7f, per the reference's own history).
 */
static inline void hmx_mac32(int32_t *acc_row, const int8_t *weights,
                              int32_t act_x_negate)
{
    for (int o = 0; o < HMX_OUTPUT_CHANNELS; o++) {
        acc_row[o] += act_x_negate * (int32_t)weights[o];
    }
}

/* Range-limited MAC for group convolution. */
static inline void hmx_mac_group(int32_t *acc_row, const int8_t *weights,
                                  int32_t act_x_negate,
                                  int grp_start, int grp_end)
{
    for (int o = grp_start; o < grp_end; o++) {
        acc_row[o] += act_x_negate * (int32_t)weights[o];
    }
}

/* Weight extraction function pointer type */
typedef void (*hmx_extract_fn)(const uint32_t *, int, int8_t *);

/* Stream indices packed per 128-byte weight vector, indexed by wei_type */
static const int hmx_channels_per_vec[8] = {
    4, 4, 8, 16, 16, 32, 32, 8
};

/* Extraction function dispatch table, indexed by wei_type */
static const hmx_extract_fn hmx_extract_table[8] = {
    hmx_extract_weights_byte,          /* HMX_WEI_B    */
    hmx_extract_weights_sm,            /* HMX_WEI_SM   */
    hmx_extract_weights_nibble,        /* HMX_WEI_N    */
    hmx_extract_weights_crumb,         /* HMX_WEI_C    */
    hmx_extract_weights_signed_crumb,  /* HMX_WEI_SC   */
    hmx_extract_weights_bit,           /* HMX_WEI_B1   */
    hmx_extract_weights_signed_bit,    /* HMX_WEI_SB1  */
    hmx_extract_weights_nibble,        /* HMX_WEI_N_2X */
};

/*
 * Reload activation crouton for a given block index.  Only used when
 * blocks > 1 (deep activation mode with more than one 2KB crouton).
 */
static void hmx_reload_act_crouton(CPUHexagonState *env, HmxState *hmx,
                                   uint32_t base_addr, int crouton_idx,
                                   int act_type, uintptr_t ra)
{
    uint32_t addr = base_addr + crouton_idx * HMX_ACT_CROUTON_SIZE;

    for (int i = 0; i < HMX_ACT_CROUTON_SIZE; i += 4) {
        uint32_t w = cpu_ldl_le_data_ra(env, addr + i, ra);
        stl_le_p(&hmx->act_buffer[i], w);
    }

    if (act_type == HMX_ACT_F8) {
        for (int s = HMX_SPATIAL_DIM_FP - 1; s >= 0; s--) {
            int crouton_s = s * 2;
            for (int c = HMX_INPUT_CHANNELS - 1; c >= 0; c--) {
                int off = hmx_act_offset_sm(crouton_s, c);
                uint16_t raw16 = hmx->act_buffer[off] |
                                 (hmx->act_buffer[off + 1] << 8);
                uint8_t f8 = (raw16 >> (hmx->is_f8_odd * 8)) & 0xff;
                uint16_t f16;

                if (f8 == 0x80) {
                    f16 = 0xFE00;
                } else {
                    f16 = ((f8 & 0x80) << 8) | ((f8 & 0x7F) << 7);
                }
                stw_le_p(&hmx->act_buffer[off], f16);
            }
        }
    }
}

/*
 * Compute per-crouton channel range for deep activation mode.
 *
 *   - First crouton (idx=0): [ch_start, 32)
 *   - Middle croutons:       [0, 32)
 *   - Last crouton (idx=blocks-1): [0, ch_stop)
 */
static void hmx_crouton_ch_range(int crouton_idx, int num_croutons,
                                 int orig_ch_start, int orig_ch_stop,
                                 int *out_start, int *out_stop)
{
    if (num_croutons <= 1) {
        *out_start = orig_ch_start;
        *out_stop = orig_ch_stop;
        return;
    }
    if (crouton_idx == 0) {
        *out_start = orig_ch_start;
        *out_stop = 32;
    } else if (crouton_idx == num_croutons - 1) {
        *out_start = 0;
        *out_stop = orig_ch_stop;
    } else {
        *out_start = 0;
        *out_stop = 32;
    }
}

static void hmx_fxp_spatial_mac(
    HmxState *hmx, const int8_t *weights,
    int y_count, int x_count,
    const int *intra_y_array, const int *intra_x_array,
    int y_tap, int x_tap,
    int32_t tile_x_mask, int32_t tile_y_mask,
    int ch_addr, int drop, int deep,
    int current_acc, int format_mask, int negate,
    int grp_count, int out_start, int out_end)
{
    for (int iy = 0; iy < y_count; iy++) {
        int intra_y = intra_y_array[iy];
        int32_t y_ovf = 0;
        int32_t act_y = hmx_inc_with_spatial_mask_ovf(
            y_tap, intra_y, tile_y_mask, &y_ovf);
        act_y = (act_y & 0x7FFFFFFF) + y_ovf * 0x800;

        for (int ix = 0; ix < x_count; ix++) {
            int intra_x = intra_x_array[ix];
            int32_t x_ovf = 0;
            int32_t output_idx = hmx_inc_with_spatial_mask_ovf(
                x_tap, intra_x, tile_x_mask, &x_ovf);
            int acc_sel = x_ovf
                ? (current_acc ^ 1) & 1
                : current_acc & 1;

            output_idx |= intra_y;

            int spatial = ((output_idx >> 5) & ~format_mask)
                        | (output_idx & format_mask);

            if (x_ovf && (drop || deep)) {
                continue;
            }

            if (spatial < 0 || spatial >= HMX_SPATIAL_DIM_FXP) {
                continue;
            }

            int32_t act_idx = (act_y + intra_x + ch_addr) & 0xFFF;
            uint8_t act_val = hmx->act_buffer[act_idx];

            HmxAccFxp *acc = &hmx->acc[acc_sel].fxp_primary;
            int32_t act_x_neg = (int32_t)act_val * negate;

            if (grp_count > 1) {
                hmx_mac_group(acc->data[spatial], weights, act_x_neg,
                              out_start, out_end);
            } else {
                hmx_mac32(acc->data[spatial], weights, act_x_neg);
            }
        }
    }
}

/*
 * A weight vector at wei_base + vec_idx * (mx_cols * 4) is valid only
 * while that byte offset stays within the Rt range and the MAC cycle
 * budget.  This baseline has no VTCM device to bound-check against yet
 * (see hw/hexagon/hexagon.h's cfgtable.vtcm_size_kb for where that
 * would come from), so unlike the reference this always treats VTCM as
 * unbounded; a future commit can wire that in if/when it matters.  The
 * N_2X type packs two output channels per word, so it needs one less
 * weight_count.
 */
static int hmx_fxp_max_valid_vec(CPUHexagonState *env, uint32_t rs,
                                 uint32_t rt, int wei_type)
{
    const HmxConfig *hmx_cfg = hmx_cfg_from_env(env);
    const uint32_t mx_cols = hmx_cfg->mx_cols;
    const uint32_t mx_rows = hmx_cfg->mx_rows;
    const uint32_t last_wgt_mask = mx_cols * HMX_OUTPUT_WORD_BYTES - 1u;

    int cpv = hmx_channels_per_vec[wei_type];
    int output_ch_scale = (wei_type == HMX_WEI_N_2X) ? 2 : 1;
    int weight_count = ctz32((uint32_t)cpv) - 2; /* log2(cpv/4) */
    if (output_ch_scale == 2) {
        weight_count -= 1;
    }
    if (weight_count < 0) {
        weight_count = 0;
    }

    /* Weight decompression buffer limit, as a byte offset. */
    uint64_t limit = (uint64_t)mx_rows * mx_cols;
    limit = limit * mx_cols;
    limit = limit * 9 / 8;
    int wgtc_mode = (rs >> 4) & 1;
    if (wgtc_mode == 0) {
        limit >>= weight_count;
    }

    /* Rt range, rounded up to the end of the vector it lands in. */
    uint64_t max_wgt_off = (uint64_t)rt | last_wgt_mask;

    if (max_wgt_off >= limit) {
        max_wgt_off = limit - 1;
    }

    /* 512 FXP MAC cycles, not applied for compressed weights. */
    uint32_t mac_cycle_limit = 512;
    uint32_t fxp_max_wgt = HMX_FXP_WEIGHTS_PER_WORD * mx_cols;
    uint64_t mac_off =
        ((uint64_t)mac_cycle_limit * fxp_max_wgt) >> weight_count;
    if (mac_off >= 1) {
        mac_off -= 1;
    }
    if (wgtc_mode != 1 && mac_off < max_wgt_off) {
        max_wgt_off = mac_off;
    }

    return (int)(max_wgt_off / (mx_cols * HMX_OUTPUT_WORD_BYTES));
}

/*
 * M8_mxmem_wei_* - load one weight stream and run the FXP matrix
 * multiply against the activation crouton latched by the preceding
 * HELPER(hmx_act_load) call.
 *
 * Loop order: crouton -> y_tap -> deep_blk -> x_tap -> channel ->
 * (preload+extract) -> spatial_y -> spatial_x -> MAC32.  Weights are
 * preloaded once per 128B vector and extracted once per stream index;
 * the same extracted weights are then reused across all spatial
 * positions.
 */
void HELPER(hmx_matmul_fxp)(CPUHexagonState *env, uint32_t rs, uint32_t rt,
                            uint32_t params)
{
    HmxState *hmx = env->hmx_state;
    int wei_type = HMX_UNPACK_WEI_TYPE(params);
    int wei_mod = HMX_UNPACK_MOD(params);
    int current_acc = hmx->current_acc_set;
    uintptr_t ra = GETPC();
    const HmxConfig *hmx_cfg = hmx_cfg_from_env(env);

    /* Align to one weight vector. */
    uint32_t wei_base = rs & ~(hmx_cfg->mx_cols * HMX_OUTPUT_WORD_BYTES - 1);
    int max_valid_vec = hmx_fxp_max_valid_vec(env, rs, rt, wei_type);
    /* Rs[5] (weight negate) only applies to the FP matmul, not FXP. */
    int negate = 1;

    /* X-dimension tap parameters (determined by the weight modifier). */
    uint32_t x_start = 0;
    uint32_t x_stop = 0;
    int x_dilate = 0;
    int deep = 0;
    int drop = 0;

    switch (wei_mod) {
    case HMX_MOD_NORMAL:
        x_stop = hmx->fx;
        break;
    case HMX_MOD_SINGLE:
        x_start = hmx->fx;
        x_stop = hmx->fx;
        break;
    case HMX_MOD_DR:
        x_stop = hmx->fx;
        drop = 1;
        break;
    case HMX_MOD_DP:
        deep = 1;
        x_stop = 0;
        break;
    case HMX_MOD_ABOVE:
        x_start = hmx->fx;
        x_stop = hmx->tile_x_mask;
        break;
    case HMX_MOD_DI:
        x_stop = hmx->fx;
        x_dilate = 1;
        break;
    }

    int format_offset = hmx->format_offset;
    int32_t tile_x_mask = hmx->tile_x_mask;
    int32_t tile_y_mask = hmx->tile_y_mask;
    int32_t tile_x_mask_msb = tile_x_mask | (1 << 31);
    int32_t tile_y_mask_msb = tile_y_mask | (1 << 31);
    int32_t tile_x_inc = hmx->tile_x_inc;
    int32_t tile_y_inc = hmx->tile_y_inc;
    int format_mask = (1 << format_offset) - 1;

    int x_tap_array[HMX_MAX_TAP_ARRAY];
    int y_tap_array[HMX_MAX_TAP_ARRAY];
    int intra_x_array[HMX_MAX_TAP_ARRAY];
    int intra_y_array[HMX_MAX_TAP_ARRAY];

    int x_tap_count = hmx_compute_indices(
        x_start, x_stop, tile_x_inc, tile_x_mask_msb,
        x_dilate, x_tap_array, HMX_MAX_TAP_ARRAY);
    int y_tap_count = hmx_compute_indices(
        hmx->y_start, hmx->y_stop, tile_y_inc, tile_y_mask_msb,
        hmx->y_dilate, y_tap_array, HMX_MAX_TAP_ARRAY);
    int x_count = hmx_compute_indices(
        0, 0x7FFFFFFF, tile_x_inc, tile_x_mask_msb,
        0, intra_x_array, HMX_MAX_TAP_ARRAY);
    int y_count = hmx_compute_indices(
        0, 0x7FFFFFFF, tile_y_inc, tile_y_mask_msb,
        0, intra_y_array, HMX_MAX_TAP_ARRAY);

    int ch_start = hmx->ch_start;
    int ch_stop = hmx->ch_stop;

    /*
     * Sub-byte weight types iterate over all 32 input channels
     * regardless of the activation instruction's channel range.
     */
    int cpv = hmx_channels_per_vec[wei_type];
    if (cpv > 4) {
        ch_start = 0;
        ch_stop = 32;
    }

    int wgt_stream_idx = 0;
    hmx_extract_fn extract = hmx_extract_table[wei_type];
    uint32_t wei_words[HMX_OUTPUT_CHANNELS];
    int prev_vec_idx = -1;

    /*
     * Deep mode processes twice the weight kernels: the first set of
     * 32 channels accumulates to primary, the second to secondary.
     */
    int num_deep_blk = deep ? 2 : 1;

    int num_croutons = hmx->blocks;
    uint32_t act_base = hmx->act_rs & 0xFFFFF800;

    for (int crouton_idx = 0; crouton_idx < num_croutons; crouton_idx++) {
        if (num_croutons > 1) {
            hmx_reload_act_crouton(env, hmx, act_base, crouton_idx,
                                   hmx->act_type, ra);
        }

        int crouton_ch_start, crouton_ch_stop;
        hmx_crouton_ch_range(crouton_idx, num_croutons, ch_start, ch_stop,
                             &crouton_ch_start, &crouton_ch_stop);

        for (int ytd = 0; ytd < y_tap_count; ytd++) {
            int y_tap = y_tap_array[ytd];

            for (int deep_blk = 0; deep_blk < num_deep_blk; deep_blk++) {
                for (int xtd = 0; xtd < x_tap_count; xtd++) {
                    int x_tap = x_tap_array[xtd];

                    for (int ch = crouton_ch_start; ch < crouton_ch_stop;
                         ch++) {
                        /*
                         * One MAC cycle per cpv channels, i.e. per
                         * weight vector consumed.  An exhausted budget
                         * drops the rest of the multiply.
                         */
                        if (((ch - crouton_ch_start) % cpv) == 0) {
                            if (hmx->mac_cycle_limit <= 0) {
                                hmx->mac_cycle_limit = 0;
                                goto mac_budget_exhausted;
                            }
                            hmx->mac_cycle_limit--;
                        }

                        int saved_wgt = wgt_stream_idx;
                        int grp_count = hmx->group_count;
                        int grp_size = hmx->group_size;

                        for (int grp = 0; grp < grp_count; grp++) {
                            wgt_stream_idx = saved_wgt;

                            int ch_in = ch + grp * grp_size;
                            int ch_addr = ch_in << format_offset;
                            int out_start = grp * grp_size;
                            int out_end = out_start + grp_size;

                            int vec_idx = wgt_stream_idx / cpv;
                            if (vec_idx > max_valid_vec) {
                                wgt_stream_idx++;
                                continue;
                            }

                            if (vec_idx != prev_vec_idx) {
                                hmx_preload_weight_vec(
                                    hmx_cfg, env, wei_base, vec_idx,
                                    wei_words, ra);
                                prev_vec_idx = vec_idx;
                            }

                            int8_t weights[HMX_OUTPUT_CHANNELS];
                            int sub_idx = wgt_stream_idx % cpv;
                            extract(wei_words, sub_idx, weights);

                            hmx_fxp_spatial_mac(
                                hmx, weights, y_count, x_count,
                                intra_y_array, intra_x_array,
                                y_tap, x_tap, tile_x_mask, tile_y_mask,
                                ch_addr, drop, deep, current_acc,
                                format_mask, negate, grp_count,
                                out_start, out_end);

                            wgt_stream_idx++;
                        }
                    }
                }
                if (deep) {
                    current_acc = (current_acc ^ 1) & 1;
                }
            }
        }
    }
mac_budget_exhausted:
    return;
}

/*
 * FXP accumulator convert math (hmx_acc_shift/rectify/scale/bias,
 * hmx_sat_to_max, hmx_u8_cvt): bit-exact rounding/rectify/saturation
 * pipeline reverse-engineered from real hardware, not derivable from
 * first principles -- ported as literally as possible rather than
 * simplified, since there is no independent way to re-derive it.
 */
static inline int64_t hmx_acc_shift(int64_t acc_biased, int32_t exp,
                                    int32_t sat, int32_t frac_bits,
                                    int32_t int_bits)
{
    int32_t shift_acc = 32 - frac_bits;
    int64_t acc_shifted = acc_biased << exp;
    int64_t mask = ((int64_t)((1ULL << (frac_bits + int_bits)) - 1))
                   << shift_acc;
    if (sat) {
        mask |= ((int64_t)((1ULL << 32) - 1)) << 32;
    }
    acc_shifted &= mask;
    return acc_shifted;
}

static inline int64_t hmx_acc_rectify(int64_t acc_shifted, int16_t zeroing,
                                      int16_t legacy, int64_t acc_biased,
                                      uint16_t element_size,
                                      int16_t disable_jam)
{
    int64_t acc_rectified = 0;
    int64_t summarize_output = 0;
    int64_t sign_bit = 0;
    uint16_t frac_bits = element_size * 12;
    int64_t summarization_bits = acc_shifted & 0xFFFFFFFE00000000LL;

    if (acc_biased < 0) {
        sign_bit = 0x400000000LL;
    }
    if (sign_bit) {
        if (summarization_bits == (int64_t)0xFFFFFFFE00000000LL) {
            summarize_output = 0x200000000LL;
        }
    } else {
        if (summarization_bits) {
            summarize_output = 0x200000000LL;
        }
    }

    int64_t maskbits = ((1LL << (frac_bits + 1)) - 1) << (32 - frac_bits);
    acc_shifted &= maskbits;
    acc_shifted |= summarize_output;
    acc_shifted |= sign_bit;

    if (!legacy && !disable_jam && acc_shifted) {
        acc_shifted |= 1LL << (31 - frac_bits);
    }

    if (((zeroing >= 4) || ((zeroing == 3) && sign_bit))
        && !((zeroing == 7) && sign_bit)) {
        acc_shifted &= (0x000000FFFFF00000LL | maskbits);
    }

    acc_shifted = (acc_shifted << 29) >> 29 >> (31 - frac_bits);

    switch (zeroing) {
    case 1:
        acc_rectified = (acc_shifted > 0) ? 0 : acc_shifted;
        break;
    case 2:
        acc_rectified = (acc_shifted < 0) ? 0 : acc_shifted;
        break;
    case 3:
        if (acc_shifted != 0) {
            acc_rectified = (acc_shifted >= 0) ? acc_shifted
                                               : -acc_shifted - 1;
        }
        break;
    case 4:
        if (acc_biased != 0) {
            acc_rectified = -acc_shifted - 1;
        }
        break;
    case 5:
        acc_rectified = (acc_biased >= 0) ? 0 : -acc_shifted - 1;
        break;
    case 6:
        acc_rectified = (acc_biased <= 0) ? 0 : -acc_shifted - 1;
        break;
    case 7:
        if (acc_biased != 0) {
            acc_rectified = (acc_shifted >= 0) ? -acc_shifted - 1
                                               : acc_shifted;
        }
        break;
    default:
        acc_rectified = acc_shifted;
        break;
    }

    acc_rectified = acc_rectified << (31 - frac_bits);
    return acc_rectified;
}

static inline __int128_t hmx_acc_scale(int64_t acc_rectified,
                                       int64_t scale_cvt)
{
    return (__int128_t)scale_cvt * acc_rectified;
}

static inline int64_t hmx_acc_bias(__int128_t acc_scaled,
                                   int32_t element_size,
                                   uint32_t rnd_bit, int32_t frac_bits)
{
    int64_t ulp_bit = 64 - 8 * element_size - 3 - 1;
    /* Zero-extend the ULP (original code cleared .hi after sign-extend) */
    __int128_t ulp = (__int128_t)(uint64_t)((int64_t)rnd_bit << ulp_bit);
    acc_scaled = acc_scaled + acc_scaled;
    __int128_t acc_rnd = acc_scaled + ulp;
    ulp_bit += 3;
    int convert_width = 12;
    return (int64_t)(acc_rnd >>
        (ulp_bit + 1 - ((convert_width - 8) * element_size)));
}

static inline uint32_t hmx_sat_to_max(int64_t in, int32_t element_size,
                                      int32_t sat)
{
    int convert_width = 12;
    int64_t max_element = (1LL << (element_size * convert_width)) - 1;
    uint32_t out;
    if (sat) {
        if (in < 0) {
            out = 0;
        } else if (in > max_element) {
            out = (uint32_t)max_element;
        } else {
            out = (uint32_t)(in & max_element);
        }
    } else {
        out = (uint32_t)(in & max_element);
    }
    return out;
}

/*
 * hmx_u8_cvt: byte convert.
 *
 *   acc      - 32-bit accumulator value (sign-extended to 64-bit)
 *   bias32   - input bias (signed 32-bit, from bias register [63:32])
 *   exp      - exponent/shift amount (5-bit, [14:10])
 *   zeroing  - shape function selector (3-bit)
 *   sig      - scale value (unsigned 12-bit)
 *   out_bias - output bias (unsigned 12-bit)
 *   sat      - saturation enable (1=clamp to [0,max], 0=mask)
 *   legacy   - 1 for the legacy combined convert+store instructions
 */
static uint32_t hmx_u8_cvt(int64_t acc, int32_t bias32, int16_t exp,
                           int16_t zeroing, int16_t sig, uint16_t out_bias,
                           int32_t sat, int16_t legacy)
{
    const int32_t element_size = 1;
    const int32_t frac_bits = 12;
    int64_t acc_biased = acc + (int64_t)bias32;

    int32_t int_bits = 32;
    int64_t acc_shifted = hmx_acc_shift(acc_biased, exp, sat,
                                        frac_bits, int_bits);

    int64_t acc_rectified = hmx_acc_rectify(acc_shifted, zeroing, legacy,
                                            acc_biased, element_size, 0);

    int64_t scale_cvt = ((int64_t)sig) << 20;
    __int128_t acc_scaled = hmx_acc_scale(acc_rectified, scale_cvt);
    int64_t acc_final = hmx_acc_bias(acc_scaled, element_size,
                                     out_bias, frac_bits);

    return hmx_sat_to_max(acc_final, element_size, sat);
}

/*
 * hmx_acc_rnd: rounding-only variant of hmx_acc_bias(), used where the
 * convert has no output-bias field to add (e.g. UH 2x1's feedback-off
 * path, which uses rnd_bit instead).
 */
static inline int64_t hmx_acc_rnd(__int128_t acc_scaled,
                                  int32_t element_size,
                                  int32_t rnd_bit, int32_t frac_bits)
{
    int convert_width = 12;
    int64_t ulp_bit = 64 - 8 * element_size - 1;
    __int128_t ulp = (__int128_t)((int64_t)rnd_bit << ulp_bit);

    (void)frac_bits;
    acc_scaled = acc_scaled + acc_scaled;
    __int128_t acc_rnd = acc_scaled + ulp;
    return (int64_t)(acc_rnd >>
        ((ulp_bit + 1) - ((convert_width - 8) * element_size)));
}

/*
 * Split a 20-bit UH result into lo/hi 16-bit CVT buffer entries.
 * lo = result[11:0] (12 bits); hi = result[19:8] aligned to bits
 * [11:4] (12 bits, lower 4 zeroed).
 */
static inline uint16_t hmx_cvt_out_lo(uint32_t result)
{
    return (uint16_t)(result & 0xFFF);
}

static inline uint16_t hmx_cvt_out_hi(uint32_t result)
{
    return (uint16_t)((result >> 8) & 0xFF0);
}

/*
 * hmx_u16_cvt: unsigned halfword 16x8 (2x1) convert. Combines two
 * adjacent spatial accumulators into a wider-precision value. Uses
 * rounding (not output bias) per the reference.
 */
static uint32_t hmx_u16_cvt(int64_t acc_hl, int64_t acc_ll,
                            int32_t bias32, int16_t exp,
                            int16_t zeroing, uint32_t sig,
                            uint16_t rnd_bit, int32_t sat,
                            int16_t legacy, int16_t has_feedback,
                            int16_t has_extra_acc_bits)
{
    const int32_t element_size = 2;
    const int32_t frac_bits = 24;

    int64_t acc_combined = acc_hl + (acc_ll >> 8);
    int64_t acc_biased = acc_combined + (int64_t)bias32;

    /*
     * extra_8bit_acc (Rs[5]): the 8 LSBs dropped by (acc_ll >> 8) are
     * reintroduced as extra precision below the binary point; the
     * conversion only takes effect for exp > 8, otherwise the extra
     * bits are truncated by the shift, so acc_biased_for_shift stays
     * at the un-extended value. acc_biased (extended) still feeds
     * rectify's sign/zero detection.
     */
    int64_t acc_biased_for_shift = acc_biased;
    if (has_extra_acc_bits) {
        uint32_t extra_8bits = (uint32_t)(acc_ll & 0xFF);
        acc_biased = (acc_biased << 8) | extra_8bits;
        if (exp > 8) {
            acc_biased_for_shift = acc_biased;
            exp -= 8;
        }
    }

    int64_t acc_shifted = hmx_acc_shift(acc_biased_for_shift, exp, sat,
                                        frac_bits, 32);
    int64_t acc_rectified = hmx_acc_rectify(acc_shifted, zeroing, legacy,
                                            acc_biased, element_size, 0);

    int64_t scale_cvt = ((int64_t)sig) << 12;
    __int128_t acc_scaled = hmx_acc_scale(acc_rectified, scale_cvt);
    int64_t acc_final;
    if (has_feedback) {
        acc_final = hmx_acc_bias(acc_scaled, element_size,
                                 rnd_bit, frac_bits);
    } else {
        acc_final = hmx_acc_rnd(acc_scaled, element_size,
                                rnd_bit, frac_bits);
    }

    return hmx_sat_to_max(acc_final, element_size, sat);
}

/*
 * hmx_u16x16_cvt: unsigned halfword 16x16 (2x2) convert. Combines four
 * accumulator positions (2 spatial x 2 output channels) into a single
 * wider-precision value. Uses output bias (unlike 2x1's rounding-only
 * path), concatenated from two adjacent bias registers.
 */
static uint32_t hmx_u16x16_cvt(int64_t acc_hh, int64_t acc_hl,
                               int64_t acc_lh, int64_t acc_ll,
                               int64_t bias48, int16_t exp,
                               int16_t zeroing, int32_t sig,
                               uint32_t out_bias, int32_t sat,
                               int16_t legacy)
{
    const int32_t element_size = 2;
    const int32_t frac_bits = 24;

    int64_t acc_combined = acc_ll +
        ((acc_lh + acc_hl + (acc_hh << 8)) << 8);
    /* Sign-extend bias48 from 48 bits */
    bias48 = (bias48 << 16) >> 16;
    int64_t acc_biased = (acc_combined + bias48) >> 16;

    int64_t acc_shifted = hmx_acc_shift(acc_biased, exp, sat,
                                        frac_bits, 32);
    int64_t acc_rectified = hmx_acc_rectify(acc_shifted, zeroing, legacy,
                                            acc_biased, element_size, 0);

    /* 2x2 uses scale shifted by 10 (not 20 like 8x8 and 2x1) */
    int64_t scale_cvt = (int64_t)sig << 10;
    __int128_t acc_scaled = hmx_acc_scale(acc_rectified, scale_cvt);
    int64_t acc_final = hmx_acc_bias(acc_scaled, element_size,
                                     out_bias, frac_bits);

    return hmx_sat_to_max(acc_final, element_size, sat);
}

/*
 * Apply any deferred accumulator clear+flip from a previous packet.
 * Deferred pipeline aging (cvt_fxp_pending*): both the legacy
 * HELPER(hmx_cvt_transfer) convert+store path and the non-legacy
 * cvt_rs + mxmem(...)=cvt pair flush through here; see
 * HELPER(hmx_commit_packet) for the packet-boundary flush.
 */
static void hmx_flush_cvt_fxp(HmxState *hmx)
{
    if (!hmx->cvt_fxp_pending) {
        return;
    }
    if (hmx->cvt_fxp_pending_age) {
        hmx->cvt_fxp[2] = hmx->cvt_fxp[1];
        hmx->cvt_fxp[1] = hmx->cvt_fxp[0];
    }
    hmx->cvt_fxp[0] = hmx->cvt_future_fxp;
    hmx->cvt_fxp_pending = 0;
    hmx->cvt_fxp_pending_age = 0;
}

static void hmx_flush_acc_clear(CPUHexagonState *env, HmxState *hmx)
{
    HmxAccFxp *acc;

    if (!hmx->cvt_acc_clear_pending) {
        return;
    }
    acc = &hmx->acc[hmx->cvt_acc_clear_set].fxp_primary;
    memset(acc, 0, sizeof(HmxAccFxp));
    g_assert(!hmx_cfg_from_env(env)->hmx_fp_uses_xfp);
    memset(&hmx->acc[hmx->cvt_acc_clear_set].fp_primary, 0,
           sizeof(HmxAccFp));
    hmx->current_acc_set = hmx->cvt_acc_clear_set ^ 1;
    hmx->cvt_acc_clear_pending = 0;
}

/*
 * M8_mxcvt{l,r}[_dm]_[sat_]ub[_r] / M8_mxcvt{b,a}[_sat]_uh[_r] - legacy
 * convert-and-store: convert the current FXP accumulator to bytes (UB)
 * or halfwords (UH, combining adjacent spatial pairs) and store them
 * to VTCM in one instruction (as opposed to the newer cvt_rs +
 * mxmem(...)=cvt pair, which defers the store -- see
 * hmx_flush_cvt_fxp()). UH2X2 combines 2x2
 * spatial+channel blocks via hmx_u16x16_cvt(), with scale/output-bias
 * concatenated from two adjacent bias registers rather than UH's
 * single one.
 *
 * HF lands with its own override later.
 */
void HELPER(hmx_cvt_transfer)(CPUHexagonState *env, uint32_t rs, uint32_t rt,
                              uint32_t params)
{
    HmxState *hmx = env->hmx_state;
    const HmxConfig *hmx_cfg = hmx_cfg_from_env(env);
    int dir = HMX_UNPACK_CVT_DIR(params);
    int fmt = HMX_UNPACK_CVT_FMT(params);
    int relu = HMX_UNPACK_CVT_RELU(params);
    int retain = HMX_UNPACK_CVT_RETAIN(params);
    uintptr_t ra = GETPC();

    g_assert(fmt == HMX_CVT_FMT_UB_SM || fmt == HMX_CVT_FMT_UB_DM ||
             fmt == HMX_CVT_FMT_UH || fmt == HMX_CVT_FMT_UH2X2);

    hmx_flush_cvt_fxp(hmx);
    hmx_flush_acc_clear(env, hmx);

    /* Legacy: always use bias set 0, no control bits from Rs. */
    uint32_t bias_set = 0;
    uint32_t store_base = rs & 0xFFFFF800;

    HmxAccFxp *acc = &hmx->acc[hmx->current_acc_set].fxp_primary;
    HmxCvtStateFxp *cvt = &hmx->cvt_fxp[0];

    int s_start = 0, s_end = HMX_SPATIAL_DIM_FXP;

    int is_sm_fmt = (fmt != HMX_CVT_FMT_UB_DM);
    uint32_t spatial_mask = is_sm_fmt ? 0x783 : 0x7E0;
    uint32_t tile_x_mask = (~rt) & spatial_mask;
    uint32_t tile_y_mask = rt & spatial_mask;
    uint32_t x_offset = rs & tile_x_mask;
    uint32_t tile_x_inc = tile_x_mask & (~tile_x_mask + 1);
    uint32_t x_count = tile_x_inc ? (tile_x_mask / tile_x_inc + 1) : 1;
    uint32_t x_off_pos = tile_x_inc ? (x_offset / tile_x_inc) : 0;
    uint32_t y_offset = rs & tile_y_mask;
    uint32_t tile_y_inc = tile_y_mask & (~tile_y_mask + 1);
    uint32_t y_count = tile_y_inc ? (tile_y_mask / tile_y_inc + 1) : 1;
    uint32_t y_off_pos = tile_y_inc ? (y_offset / tile_y_inc) : 0;

    /* Age pipeline: shift ages 2 <- 1, 1 <- 0 */
    hmx->cvt_fxp[2] = hmx->cvt_fxp[1];
    hmx->cvt_fxp[1] = hmx->cvt_fxp[0];

    int32_t  bias_input[HMX_OUTPUT_CHANNELS];
    int16_t  bias_exp[HMX_OUTPUT_CHANNELS];
    int16_t  bias_shape[HMX_OUTPUT_CHANNELS];
    int16_t  bias_scale[HMX_OUTPUT_CHANNELS];
    uint16_t bias_out[HMX_OUTPUT_CHANNELS];

    for (int o = 0; o < hmx_cfg->mx_cols; o++) {
        uint64_t raw = hmx->bias_raw[bias_set][o];
        bias_input[o] = hmx_bias_input_bias(raw);
        bias_exp[o]   = (int16_t)hmx_bias_exponent(raw);
        bias_shape[o] = (int16_t)hmx_bias_shape(raw);
        bias_scale[o] = (int16_t)hmx_bias_scale(raw);
        bias_out[o]   = hmx_bias_output_bias_unsigned(raw);
    }

    if (fmt == HMX_CVT_FMT_UH) {
        /*
         * UH 2x1: combine adjacent spatial pairs. Uses rnd_bit
         * (BIAS[22]) instead of the full output bias.
         */
        for (int s = s_start; s < s_end; s += 2) {
            for (int o = 0; o < hmx_cfg->mx_cols; o++) {
                int64_t acc_ll = (int64_t)acc->data[s][o];
                int64_t acc_hl = (int64_t)acc->data[s + 1][o];
                uint64_t raw = hmx->bias_raw[bias_set][o];
                uint16_t rnd = (raw >> 22) & 1;
                uint32_t bias1 = (raw >> 23) & 0xFF;
                uint32_t poly_scale = ((uint32_t)bias_scale[o] << 8) | bias1;

                uint32_t result = hmx_u16_cvt(
                    acc_hl, acc_ll, bias_input[o], bias_exp[o],
                    bias_shape[o], poly_scale, rnd,
                    /* sat */ !relu, /* legacy */ 1,
                    /* has_feedback */ 0, /* has_extra_acc_bits */ 0);

                result >>= 4;
                cvt->data[s][o] = hmx_cvt_out_lo(result);
                cvt->data[s + 1][o] = hmx_cvt_out_hi(result);
            }
        }
    } else if (fmt == HMX_CVT_FMT_UH2X2) {
        /*
         * UH 2x2: combine 2x2 spatial+channel blocks. Scale and
         * output bias are concatenated from adjacent bias registers.
         */
        for (int s = s_start; s < s_end; s += 2) {
            for (int o = 0; o < hmx_cfg->mx_cols; o += 2) {
                uint64_t raw_lo = hmx->bias_raw[bias_set][o];
                uint64_t raw_hi = hmx->bias_raw[bias_set][o + 1];

                int32_t ibias = hmx_bias_input_bias(raw_lo);
                int16_t exp2 = (int16_t)hmx_bias_exponent(raw_lo);
                int16_t shp = (int16_t)hmx_bias_shape(raw_lo);

                uint32_t sig_lo = hmx_bias_scale(raw_lo);
                uint32_t sigmsb_hi = !((raw_hi >> 16) & 1);
                uint32_t sig_hi = raw_hi & 0x3FF;
                uint32_t scale2 =
                    (((sigmsb_hi << 10) | sig_hi) << 11) |
                    (sig_lo & 0x7FF);

                uint32_t bhi = (raw_hi >> 23) & 0xFF;
                int32_t blo = hmx_bias_output_bias(raw_lo);
                uint32_t obias2 = (uint32_t)(((bhi << 12) +
                    (blo & 0xFFF)) << 2);

                int64_t bias48 = (int64_t)ibias << 16;

                int64_t acc_ll = (int64_t)acc->data[s][o];
                int64_t acc_hl = (int64_t)acc->data[s + 1][o];
                int64_t acc_lh = (int64_t)acc->data[s][o + 1];
                int64_t acc_hh = (int64_t)acc->data[s + 1][o + 1];

                uint32_t result = hmx_u16x16_cvt(
                    acc_hh, acc_hl, acc_lh, acc_ll,
                    bias48, exp2, shp, scale2, obias2,
                    /* sat */ !relu, /* legacy */ 1);

                result >>= 4;
                cvt->data[s][o] = hmx_cvt_out_lo(result);
                cvt->data[s + 1][o] = hmx_cvt_out_hi(result);
                cvt->data[s][o + 1] = 0;
                cvt->data[s + 1][o + 1] = 0;
            }
        }
    } else {
        /* UB convert (8x8 byte) */
        for (int s = s_start; s < s_end; s++) {
            for (int o = 0; o < hmx_cfg->mx_cols; o++) {
                int64_t acc_combined = (int64_t)acc->data[s][o];

                /*
                 * Legacy mode: sat = !relu (relu=0 -> saturate,
                 * relu=1 -> mask/wrap), legacy=1 (disables jamming).
                 */
                uint32_t result = hmx_u8_cvt(
                    acc_combined, bias_input[o], bias_exp[o],
                    bias_shape[o], bias_scale[o], bias_out[o],
                    /* sat */ !relu, /* legacy */ 1);

                cvt->data[s][o] = (uint16_t)result;
            }
        }
    }

    /*
     * Store convert state to VTCM (respecting direction/split).  See
     * the reference's HELPER(hmx_cvt_transfer) for the before/after
     * split-point rationale; unchanged here.
     */
    int is_cm = (fmt == HMX_CVT_FMT_UB_DM);
    int is_y_dir = (dir >= HMX_CVT_BOTTOM);
    uint32_t split_mask = is_y_dir ? tile_y_mask : tile_x_mask;
    uint32_t split_offset = is_y_dir ? y_offset : x_offset;
    uint32_t split_inc = is_y_dir ? tile_y_inc : tile_x_inc;
    uint32_t split_count = is_y_dir ? y_count : x_count;
    uint32_t split_off_pos = is_y_dir ? y_off_pos : x_off_pos;
    int split_before = (dir == HMX_CVT_LEFT || dir == HMX_CVT_BOTTOM);

    for (int s = s_start; s < s_end; s++) {
        uint32_t sp_addr = is_cm ? hmx_act_offset_cm(s, 0)
                                 : hmx_act_offset_sm(s, 0);
        uint32_t split_bits = sp_addr & split_mask;
        if (split_before && split_bits >= split_offset) {
            continue;
        }
        if (!split_before && split_bits < split_offset) {
            continue;
        }

        int acc_s = s;
        if (split_offset != 0 && split_inc != 0) {
            uint32_t sa = is_cm ? (uint32_t)(s << 5) :
                          (uint32_t)(((s >> 2) << 7) | (s & 3));
            uint32_t sa_split = sa & split_mask;
            uint32_t o_addr = sa & ~split_mask & spatial_mask;
            uint32_t s_pos = sa_split / split_inc;
            uint32_t acc_s_pos = (s_pos + split_count -
                                  split_off_pos) % split_count;
            uint32_t acc_addr = (acc_s_pos * split_inc) | o_addr;
            if (is_cm) {
                acc_s = acc_addr >> 5;
            } else {
                acc_s = ((acc_addr >> 7) << 2) | (acc_addr & 3);
            }
        }

        for (int o = 0; o < hmx_cfg->mx_cols; o++) {
            int offset = is_cm ? hmx_act_offset_cm(s, o)
                               : hmx_act_offset_sm(s, o);
            uint8_t val = (cvt->data[acc_s][o] >> 4) & 0xFF;
            uint32_t addr = store_base + offset;
            cpu_stb_data_ra(env, addr, val, ra);
        }
    }

    /* Clear accumulator and flip acc set, unless retaining. */
    if (!retain) {
        for (int s = s_start; s < s_end; s++) {
            memset(&acc->data[s], 0, sizeof(acc->data[s]));
        }
        hmx->current_acc_set ^= 1;
    }
}

/*
 * Non-legacy FXP convert: acc -> cvt_future_fxp (deferred; not stored to
 * memory here -- see HELPER(hmx_cvt_rs) and HELPER(hmx_cvt_store)).
 *
 * Rs bitfield (convert control, read by HELPER(hmx_cvt_rs)):
 *   [0]   acc_clear: 0=clear acc, 1=retain
 *   [1]   relu: 0=apply ReLU (clip negatives), 1=no ReLU (active-low)
 *   [3:2] fb_dst: feedback destination (0=none, 1=out_bias, 2=scale)
 *   [4]   fb_limit: feedback limit mode
 *   [13:12] bias_sel
 */
static void hmx_fxp_convert(const HmxConfig *hmx_cfg, HmxState *hmx,
                            int acc_set, int relu,
                            int bias_set, int fb_dst, int fb_limit,
                            uint32_t cur_pc)
{
    HmxAccFxp *acc = &hmx->acc[acc_set & 1].fxp_primary;

    /*
     * Deferred pipeline aging (matching the reference's commit_regs):
     *
     * fb=0 sets cvt_advance=0 (age), fb!=0 sets cvt_advance=1 (don't
     * age). When fb=0 and fb!=0 both occur for the same PC (the same
     * packet), the fb!=0 pass suppresses the aging fb=0 requested.
     *
     * The age+copy itself is deferred until the next consumer (store
     * or the next fb=0 convert) needs the committed data -- see
     * hmx_flush_cvt_fxp().
     */
    if (fb_dst == 0) {
        hmx_flush_cvt_fxp(hmx);
        hmx->cvt_fxp_pending = 1;
        hmx->cvt_fxp_pending_age = 1;
        hmx->cvt_fxp_pending_pc = cur_pc;
    } else {
        if (hmx->cvt_fxp_pending && cur_pc == hmx->cvt_fxp_pending_pc) {
            hmx->cvt_fxp_pending_age = 0;
        } else {
            hmx_flush_cvt_fxp(hmx);
            hmx->cvt_fxp_pending = 1;
            hmx->cvt_fxp_pending_age = 0;
            hmx->cvt_fxp_pending_pc = cur_pc;
        }
    }

    /* Feedback reads from the working buffer (cvt_future_fxp). */
    HmxCvtStateFxp *feedback_buf = &hmx->cvt_future_fxp;

    int32_t  bias_input[HMX_OUTPUT_CHANNELS];
    int16_t  bias_exp[HMX_OUTPUT_CHANNELS];
    int16_t  bias_shape[HMX_OUTPUT_CHANNELS];
    int16_t  bias_scale[HMX_OUTPUT_CHANNELS];
    uint16_t bias_out[HMX_OUTPUT_CHANNELS];

    for (int o = 0; o < hmx_cfg->mx_cols; o++) {
        uint64_t raw = hmx->bias_raw[bias_set][o];
        bias_input[o] = hmx_bias_input_bias(raw);
        bias_exp[o]   = (int16_t)hmx_bias_exponent(raw);
        bias_shape[o] = (int16_t)hmx_bias_shape(raw);
        bias_scale[o] = (int16_t)hmx_bias_scale(raw);
        bias_out[o]   = hmx_bias_output_bias_unsigned(raw);
    }

    HmxCvtStateFxp *cvt_out = &hmx->cvt_future_fxp;

    for (int s = 0; s < (int)hmx_cfg->mx_rows; s++) {
        for (int o = 0; o < hmx_cfg->mx_cols; o++) {
            int64_t acc_combined = (int64_t)acc->data[s][o];
            int16_t scale = bias_scale[o];
            uint16_t out_bias = bias_out[o];

            if (fb_dst != 0) {
                uint16_t fb = feedback_buf->data[s][o];
                if (fb_dst == 1) {
                    out_bias = fb_limit ? (out_bias > fb ? out_bias : fb)
                                        : (out_bias > fb ? fb : out_bias);
                } else if (fb_dst == 2) {
                    int16_t fb16 = (int16_t)fb;
                    scale = fb_limit ? (scale > fb16 ? scale : fb16)
                                     : (scale > fb16 ? fb16 : scale);
                }
            }

            uint32_t result = hmx_u8_cvt(
                acc_combined, bias_input[o], bias_exp[o],
                bias_shape[o], scale, out_bias,
                /* sat */ relu, /* legacy */ 0);

            cvt_out->data[s][o] = (uint16_t)result;
        }
    }
}

/*
 * Non-legacy UH (2x1) convert: acc -> cvt_future_fxp, combining
 * adjacent spatial pairs via hmx_u16_cvt(). Same deferred-pipeline
 * aging as hmx_fxp_convert(); see that function's comment.
 */
static void hmx_fxp_convert_2x1(const HmxConfig *hmx_cfg, HmxState *hmx,
                                int acc_set, int relu,
                                int bias_set, int fb_dst, int extra_8bit,
                                uint32_t cur_pc)
{
    HmxAccFxp *acc = &hmx->acc[acc_set & 1].fxp_primary;

    if (fb_dst == 0) {
        hmx_flush_cvt_fxp(hmx);
        hmx->cvt_fxp_pending = 1;
        hmx->cvt_fxp_pending_age = 1;
        hmx->cvt_fxp_pending_pc = cur_pc;
    } else {
        if (hmx->cvt_fxp_pending && cur_pc == hmx->cvt_fxp_pending_pc) {
            hmx->cvt_fxp_pending_age = 0;
        } else {
            hmx_flush_cvt_fxp(hmx);
            hmx->cvt_fxp_pending = 1;
            hmx->cvt_fxp_pending_age = 0;
            hmx->cvt_fxp_pending_pc = cur_pc;
        }
    }

    int32_t  bias_input[HMX_OUTPUT_CHANNELS];
    int16_t  bias_exp[HMX_OUTPUT_CHANNELS];
    int16_t  bias_shape[HMX_OUTPUT_CHANNELS];
    int16_t  bias_scale[HMX_OUTPUT_CHANNELS];
    uint32_t bias1[HMX_OUTPUT_CHANNELS];
    uint16_t bias_rnd[HMX_OUTPUT_CHANNELS];

    for (int o = 0; o < hmx_cfg->mx_cols; o++) {
        uint64_t raw = hmx->bias_raw[bias_set][o];
        bias_input[o] = hmx_bias_input_bias(raw);
        bias_exp[o]   = (int16_t)hmx_bias_exponent(raw);
        bias_shape[o] = (int16_t)hmx_bias_shape(raw);
        bias_scale[o] = (int16_t)hmx_bias_scale(raw);
        bias1[o]      = (raw >> 23) & 0xFF;
        /* UH 2x1 uses rnd_bit (BIAS[22]) instead of the full out_bias. */
        bias_rnd[o] = (raw >> 22) & 1;
    }

    HmxCvtStateFxp *cvt_out = &hmx->cvt_future_fxp;

    for (int s = 0; s < (int)hmx_cfg->mx_rows; s += 2) {
        for (int o = 0; o < hmx_cfg->mx_cols; o++) {
            int64_t acc_ll = (int64_t)acc->data[s][o];
            int64_t acc_hl = (int64_t)acc->data[s + 1][o];
            uint32_t poly_scale = ((uint32_t)bias_scale[o] << 8) | bias1[o];

            uint32_t result = hmx_u16_cvt(
                acc_hl, acc_ll, bias_input[o], bias_exp[o],
                bias_shape[o], poly_scale, bias_rnd[o],
                /* sat */ relu, /* legacy */ 0,
                /* has_feedback */ 0, /* has_extra_acc_bits */ extra_8bit);

            result >>= 4;
            cvt_out->data[s][o] = hmx_cvt_out_lo(result);
            cvt_out->data[s + 1][o] = hmx_cvt_out_hi(result);
        }
    }
}

/*
 * Non-legacy UH2X2 convert: acc -> cvt_future_fxp, combining 2x2
 * spatial+channel blocks via hmx_u16x16_cvt(). Same deferred-pipeline
 * aging as hmx_fxp_convert().
 *
 * ch_sel (cvt Rs[10:9]) selects which output channel of each 2x2
 * block the 16-bit result lands in, and which accumulators feed the
 * convert: ch_sel==2 shifts the "low" pair's accumulators into the
 * "high" (odd-channel) slot instead of using the real odd-channel
 * accumulators; any ch_sel != 3 zeroes the low-channel accumulators
 * before combining. See the reference for the hardware rationale;
 * ported as specified rather than only handling the one case an
 * earlier QEMU attempt hardcoded (which produced all-zero odd output
 * channels for ch_sel 0/1).
 */
static void hmx_fxp_convert_2x2(const HmxConfig *hmx_cfg, HmxState *hmx,
                                int acc_set, int relu,
                                int bias_set, int fb_dst, int ch_sel,
                                uint32_t cur_pc)
{
    HmxAccFxp *acc = &hmx->acc[acc_set & 1].fxp_primary;
    int output_adjust = (ch_sel == 2) ? 0 : 1;

    if (fb_dst == 0) {
        hmx_flush_cvt_fxp(hmx);
        hmx->cvt_fxp_pending = 1;
        hmx->cvt_fxp_pending_age = 1;
        hmx->cvt_fxp_pending_pc = cur_pc;
    } else {
        if (hmx->cvt_fxp_pending && cur_pc == hmx->cvt_fxp_pending_pc) {
            hmx->cvt_fxp_pending_age = 0;
        } else {
            hmx_flush_cvt_fxp(hmx);
            hmx->cvt_fxp_pending = 1;
            hmx->cvt_fxp_pending_age = 0;
            hmx->cvt_fxp_pending_pc = cur_pc;
        }
    }

    HmxCvtStateFxp *cvt_out = &hmx->cvt_future_fxp;

    for (int s = 0; s < (int)hmx_cfg->mx_rows; s += 2) {
        for (int o = 0; o < hmx_cfg->mx_cols; o += 2) {
            uint64_t raw_lo = hmx->bias_raw[bias_set][o];
            uint64_t raw_hi = hmx->bias_raw[bias_set][o + 1];

            int32_t input_bias = hmx_bias_input_bias(raw_lo);
            int16_t exp = (int16_t)hmx_bias_exponent(raw_lo);
            int16_t shape = (int16_t)hmx_bias_shape(raw_lo);

            uint32_t sig_lo = hmx_bias_scale(raw_lo);
            uint32_t sigmsb_hi = !((raw_hi >> 16) & 1);
            uint32_t sig_hi = raw_hi & 0x3FF;
            uint32_t scale = (((sigmsb_hi << 10) | sig_hi) << 11) |
                             (sig_lo & 0x7FF);

            uint32_t bias1_hi = (raw_hi >> 23) & 0xFF;
            int32_t bias_lo_val = hmx_bias_output_bias(raw_lo);
            uint32_t out_bias = (uint32_t)((bias1_hi << 12) +
                                (bias_lo_val & 0xFFF));

            int64_t bias48 = (int64_t)input_bias << 16;

            int64_t acc_ll = (int64_t)acc->data[s][o];
            int64_t acc_hl = (int64_t)acc->data[s + 1][o];
            int64_t acc_lh = (int64_t)acc->data[s][o + 1];
            int64_t acc_hh = (int64_t)acc->data[s + 1][o + 1];

            if (ch_sel != 3) {
                if (ch_sel == 2) {
                    acc_lh = acc_ll;
                    acc_hh = acc_hl;
                }
                acc_ll = 0;
                acc_hl = 0;
            }

            uint32_t result = hmx_u16x16_cvt(
                acc_hh, acc_hl, acc_lh, acc_ll,
                bias48, exp, shape, scale, out_bias,
                /* sat */ relu, /* legacy */ 0);

            result >>= 4;
            cvt_out->data[s][o + output_adjust] = hmx_cvt_out_lo(result);
            cvt_out->data[s + 1][o + output_adjust] = hmx_cvt_out_hi(result);
            if (!fb_dst) {
                cvt_out->data[s][o + !output_adjust] = 0;
                cvt_out->data[s + 1][o + !output_adjust] = 0;
            }
        }
    }
}

/*
 * M8_cvt_rs_{ub,ub_sc0,ub_sc1,uh_2x1,uh_2x2} - trigger the deferred FXP
 * convert (acc -> cvt_future_fxp, see hmx_fxp_convert()/
 * hmx_fxp_convert_2x1()/hmx_fxp_convert_2x2()). Always returns 0 (the
 * destination register, per the reference); the converted data isn't
 * read back through a GPR, it's read back from memory by a later
 * HELPER(hmx_cvt_store) (M8_mxmem/M8_mxmem_deep/M8_mxmem_2x2).
 *
 * HF/F8 fall through to the default (still returns 0, matching the
 * reference's own default case) since no tag is overridden to reach
 * them yet.
 */
uint32_t HELPER(hmx_cvt_rs)(CPUHexagonState *env, uint32_t rs, uint32_t type)
{
    HmxState *hmx = env->hmx_state;
    int relu = !((rs >> 1) & 1);
    int bias_sel = (rs >> 12) & 0x3;
    int fb_dst = (rs >> 2) & 0x3;
    int fb_limit = (rs >> 4) & 0x1;
    int acc_clear = !(rs & 1);
    uint32_t cur_pc = env->gpr[HEX_REG_PC];
    const HmxConfig *hmx_cfg = hmx_cfg_from_env(env);

    if (hmx_cpu_version(env) < HEX_VER_V75) {
        bias_sel = 0;
    }

    switch (type) {
    case HMX_CVT_RS_UB:
    case HMX_CVT_RS_UB_SC0:
    case HMX_CVT_RS_UB_SC1:
        hmx_fxp_convert(hmx_cfg, hmx, hmx->current_acc_set, relu, bias_sel,
                        fb_dst, fb_limit, cur_pc);
        if (acc_clear) {
            hmx->cvt_acc_clear_pending = 1;
            hmx->cvt_acc_clear_set = hmx->current_acc_set;
            hmx->cvt_acc_clear_pc = cur_pc;
        }
        return 0;
    case HMX_CVT_RS_UH_2X1:
        /* Rs[5]: extra_8bit_acc, see hmx_u16_cvt(). */
        hmx_fxp_convert_2x1(hmx_cfg, hmx, hmx->current_acc_set, relu,
                            bias_sel, fb_dst, (rs >> 5) & 1, cur_pc);
        if (acc_clear) {
            hmx->cvt_acc_clear_pending = 1;
            hmx->cvt_acc_clear_set = hmx->current_acc_set;
            hmx->cvt_acc_clear_pc = cur_pc;
        }
        return 0;
    case HMX_CVT_RS_UH_2X2:
    {
        /* Rs[10:9]: ch_sel, see hmx_fxp_convert_2x2(). */
        int ch_sel = (rs >> 9) & 0x3;
        hmx_fxp_convert_2x2(hmx_cfg, hmx, hmx->current_acc_set, relu,
                            bias_sel, fb_dst, ch_sel, cur_pc);
        if (acc_clear) {
            hmx->cvt_acc_clear_pending = 1;
            hmx->cvt_acc_clear_set = hmx->current_acc_set;
            hmx->cvt_acc_clear_pc = cur_pc;
        }
        return 0;
    }
    default:
        return 0;
    }
}

/*
 * M8_commit_packet's HMX half: flush the deferred convert pipeline and
 * any deferred accumulator clear at the end of every packet that has
 * an HMX instruction (see translate.c's gen_commit_packet()). Mirrors
 * the reference's commit_regs + commit_mem.
 */
void HELPER(hmx_commit_packet)(CPUHexagonState *env)
{
    HmxState *hmx = env->hmx_state;

    hmx_flush_cvt_fxp(hmx);
    hmx_flush_acc_clear(env, hmx);
}

/*
 * Convert raw spatial address to linear CVT buffer index (0-63).
 * CM: 6 spatial bits at [10:5]; SM: 6 spatial bits at [10:7] and [1:0].
 */
static inline int hmx_raw_to_linear(int32_t raw, int is_cm)
{
    if (is_cm) {
        return (raw >> 5) & 0x3F;
    }
    return ((((uint32_t)raw >> 7) << 2) | (raw & 3)) & 0x3F;
}

/*
 * Write one FXP spatial "peg" (mx_cols output channels) to memory.
 * CM: channels at byte stride (mx_cols bytes per peg).
 * SM/2x2: channels at 4-byte stride (interleaved with spatial).
 */
static void hmx_store_fxp_peg(CPUHexagonState *env,
                              const HmxConfig *hmx_cfg,
                              HmxCvtStateFxp *cvt,
                              int linear_s, uint32_t base_addr,
                              int32_t mem_spatial, int fmt, uintptr_t ra)
{
    uint32_t pa = base_addr + (mem_spatial & 0x7FF);

    if (fmt == HMX_CVTST_CM) {
        g_assert((hmx_cfg->mx_cols & 3) == 0);
        for (int o = 0; o < (int)hmx_cfg->mx_cols; o += 4) {
            uint32_t w =
                (((cvt->data[linear_s][o + 0] >> 4) & 0xFF)) |
                (((cvt->data[linear_s][o + 1] >> 4) & 0xFF) << 8) |
                (((cvt->data[linear_s][o + 2] >> 4) & 0xFF) << 16) |
                (((cvt->data[linear_s][o + 3] >> 4) & 0xFF) << 24);
            cpu_stl_le_data_ra(env, pa + o, w, ra);
        }
    } else {
        for (int o = 0; o < (int)hmx_cfg->mx_cols; o++) {
            uint8_t val = (cvt->data[linear_s][o] >> 4) & 0xFF;
            cpu_stb_data_ra(env, pa + (o << 2), val, ra);
        }
    }
}

/*
 * Write one x-row of the store.
 *
 * The CVT buffer index uses a rotated accumulator index (x_acc_idx,
 * y_acc_idx) rather than the memory position (x_idx, y_idx); this
 * rotation accounts for the spatial split at x_offset.
 *
 * Age selection is based on X position relative to x_offset:
 *   x < x_offset  -> cvt_ages[before_state] (previous convert)
 *   x >= x_offset -> cvt_ages[0] (current convert)
 */
static void hmx_store_x_row(CPUHexagonState *env,
                            const HmxConfig *hmx_cfg,
                            HmxCvtStateFxp *cvt_ages,
                            uint32_t base_addr, int32_t y_idx,
                            int32_t y_acc_idx, uint32_t x_offset,
                            uint32_t tile_x_inc, int32_t xm,
                            int is_cm, int fmt, int before_state,
                            int32_t x_acc_offset, uintptr_t ra)
{
    int32_t x_idx, x_acc_idx;
    int s;

    /* BEFORE: x < x_offset, reads from the previous CVT age. */
    x_idx = 0;
    x_acc_idx = x_acc_offset;
    for (; x_idx < (int32_t)x_offset; ) {
        s = hmx_raw_to_linear(x_acc_idx | y_acc_idx, is_cm);
        hmx_store_fxp_peg(env, hmx_cfg, &cvt_ages[before_state],
                          s, base_addr, x_idx | y_idx, fmt, ra);
        x_idx = hmx_inc_with_spatial_mask(x_idx, tile_x_inc, xm);
        x_acc_idx = hmx_inc_with_spatial_mask(x_acc_idx, tile_x_inc, xm);
        if (!tile_x_inc) {
            break;
        }
    }

    /* AFTER: x >= x_offset, reads from the current CVT age. */
    x_acc_idx = 0;
    while (x_idx >= 0) {
        s = hmx_raw_to_linear(x_acc_idx | y_acc_idx, is_cm);
        hmx_store_fxp_peg(env, hmx_cfg, &cvt_ages[0],
                          s, base_addr, x_idx | y_idx, fmt, ra);
        x_idx = hmx_inc_with_spatial_mask(x_idx, tile_x_inc, xm);
        x_acc_idx = hmx_inc_with_spatial_mask(x_acc_idx, tile_x_inc, xm);
        if (!tile_x_inc) {
            break;
        }
    }
}

/*
 * M8_mxmem / M8_mxmem_deep / M8_mxmem_cm / M8_mxmem_cm_deep / M8_mxmem_2x2
 * - store the deferred convert pipeline's output to memory, with a
 * before/after spatial split at (y_offset, x_offset) derived from Rs/Rt
 * (see the reference's HELPER(hmx_cvt_store) for the full row-rotation
 * rationale, ported unchanged here). Only the FXP (CM/SM/2x2) formats
 * are implemented; F8 needs the FP convert path, not written yet.
 */
void HELPER(hmx_cvt_store)(CPUHexagonState *env, uint32_t rs, uint32_t rt,
                           uint32_t params)
{
    HmxState *hmx = env->hmx_state;
    const HmxConfig *hmx_cfg = hmx_cfg_from_env(env);
    int fmt = HMX_UNPACK_CVTST_FMT(params);
    int age = HMX_UNPACK_CVTST_AGE(params);
    uintptr_t ra = GETPC();

    g_assert(fmt != HMX_CVTST_F8);
    g_assert(age < HMX_NUM_CVT_AGES);

    uint32_t base_addr = rs & 0xFFFFF800;

    /* Flush the pending conversion before reading the aged pipeline. */
    hmx_flush_cvt_fxp(hmx);
    hmx_flush_acc_clear(env, hmx);

    int is_cm = (fmt == HMX_CVTST_CM);
    uint32_t sp_mask = is_cm ? HMX_SPATIAL_MASK_BITS_CM
                             : HMX_SPATIAL_MASK_BITS_SM;
    uint32_t tile_y_mask = rt & sp_mask;
    uint32_t tile_x_mask = (~rt) & sp_mask;
    uint32_t tile_x_inc =
        tile_x_mask ? (tile_x_mask & (-(int32_t)tile_x_mask)) : 0;
    uint32_t tile_y_inc =
        tile_y_mask ? (tile_y_mask & (-(int32_t)tile_y_mask)) : 0;
    uint32_t x_offset = rs & tile_x_mask;
    uint32_t y_offset = rs & tile_y_mask;
    int32_t xm = (int32_t)(tile_x_mask | (1u << 31));
    int32_t ym = (int32_t)(tile_y_mask | (1u << 31));

    /* BEFORE reads from previous CVT state: age=0->[1], age=1->[2]. */
    int before_state = 1 + age;

    int32_t x_acc_offset = 0;
    if (x_offset && tile_x_inc) {
        int32_t x_count = (int32_t)x_offset;
        while (x_count >= 0) {
            x_acc_offset =
                hmx_inc_with_spatial_mask(x_acc_offset, tile_x_inc, xm);
            x_count = hmx_inc_with_spatial_mask(x_count, tile_x_inc, xm);
        }
    }

    /* First y pass: y from y_offset onward. */
    int32_t y_idx = (int32_t)y_offset;
    int32_t y_acc_idx = 0;

    while (y_idx >= 0) {
        hmx_store_x_row(env, hmx_cfg, hmx->cvt_fxp, base_addr, y_idx,
                        y_acc_idx, x_offset, tile_x_inc, xm,
                        is_cm, fmt, before_state, x_acc_offset, ra);
        y_idx = hmx_inc_with_spatial_mask(y_idx, tile_y_inc, ym);
        y_acc_idx = hmx_inc_with_spatial_mask(y_acc_idx, tile_y_inc, ym);
        if (!tile_y_inc) {
            break;
        }
    }

    /* Second y pass: y from 0 to y_offset, address adjusted by dY. */
    if ((int32_t)y_offset > 0) {
        uint32_t dY = rt & 0xFFFFF800;
        if (dY) {
            base_addr += dY;
        }
        for (y_idx = 0; y_idx < (int32_t)y_offset; ) {
            hmx_store_x_row(env, hmx_cfg, hmx->cvt_fxp, base_addr,
                            y_idx, y_acc_idx, x_offset, tile_x_inc, xm,
                            is_cm, fmt, before_state, x_acc_offset, ra);
            y_idx = hmx_inc_with_spatial_mask(y_idx, tile_y_inc, ym);
            y_acc_idx = hmx_inc_with_spatial_mask(y_acc_idx, tile_y_inc, ym);
            if (!tile_y_inc) {
                break;
            }
        }
    }
}
