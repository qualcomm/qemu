/*
 * Hexagon HMX (Matrix eXtensions) Helper Functions
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Native TCG helper implementations for HMX coprocessor instructions.
 * These replace the external RPC-based coproc implementation.
 */

#include "qemu/osdep.h"
#include "cpu.h"
#include "exec/helper-proto.h"
#include "accel/tcg/cpu-ldst.h"
#include "fpu/softfloat.h"
#include "hmx_state.h"
#include <math.h>

#ifndef CONFIG_INT128
#error "HMX helpers require 128-bit integer support (CONFIG_INT128)"
#endif

/* Forward declaration (hmx_fp_convert called before its definition) */
static void hmx_fp_convert(CPUHexagonState *env, HmxState *hmx, int acc_set,
                            int is_f8, int relu, int bias_sel, int maxnorm,
                            int fp8_odd_sel);

/*
 * Flush pending FXP CVT pipeline commit.
 *
 * Defers pipeline aging to commit_regs time.
 * When fb=0 and fb=2 are in the same packet, fb=2 sets cvt_advance=1
 * which suppresses the aging that fb=0 requested.  We emulate this by
 * writing convert results to cvt_future_fxp and deferring the
 * age + copy-to-pipeline until the next consumer needs committed data.
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

/*
 * Apply any deferred accumulator clear+flip from a previous packet.
 * Called at the start of operations that begin a new packet context:
 * CVT store and legacy CVT transfer.
 */
static void hmx_flush_acc_clear(HmxState *hmx)
{
    HmxAccFxp *acc;

    if (!hmx->cvt_acc_clear_pending) {
        return;
    }
    acc = &hmx->acc[hmx->cvt_acc_clear_set].fxp_primary;
    memset(acc, 0, sizeof(HmxAccFxp));
    hmx->current_acc_set = hmx->cvt_acc_clear_set ^ 1;
    hmx->cvt_acc_clear_pending = 0;
}

/*
 * Commit HMX state at packet boundary.
 *
 * Called from gen_commit_packet() via TCG at the end of every coproc
 * packet.  This replaces the broken PC-based deferral that failed in
 * hardware loops where the same PC appears across loop iterations.
 *
 * Mirrors commit_regs + commit_mem:
 *   - Flush pending CVT pipeline (aging + copy to cvt_fxp[])
 *   - Apply deferred accumulator clear + set flip
 */
void HELPER(hmx_commit_packet)(CPUHexagonState *env)
{
    HmxState *hmx = env->hmx_state;
    hmx_flush_cvt_fxp(hmx);
    hmx_flush_acc_clear(hmx);
}

/*
 * Compute multi-tap convolution parameters from activation Rs/Rt.
 *
 * This implements the activation-parameter logic
 * implementation. Called from hmx_act_load to pre-compute
 * tile masks, filter positions, channel ranges, and tap counts.
 */
static void hmx_compute_act_params(HmxState *hmx, uint32_t rs, uint32_t rt,
                                    int act_fmt, int act_mod, int act_type)
{
    int format_offset = (act_fmt == HMX_ACT_FMT_SM) ? 2 : 0;
    uint32_t ch_mask = 0x1F << format_offset;
    int is_flt = (act_type == HMX_ACT_HF) || (act_type == HMX_ACT_F8);

    hmx->format_offset = format_offset;
    hmx->dY = rt & ~2047;  /* offset to second activation crouton */

    /*
     * Channel range.
     * Activation parameter handling.
     */
    /*
     * Channel range extraction and group convolution detection.
     *
     * Activation parameter handling.
     * When ch_start > ch_stop (raw), the hardware enters group convolution
     * mode.  The leading-ones count of ((ch_stop-ch_start)^ch_start^ch_stop)
     * determines the group size (32 >> cl1).  The channel indices are then
     * masked to within the group, and the matmul iterates over all groups.
     */
    int raw_start = (rs & ch_mask) >> format_offset;
    int raw_stop = (rt & ch_mask) >> format_offset;

    hmx->group_conv = (raw_start > raw_stop) &&
                      (act_mod != HMX_ACT_NOBLK);

    if (hmx->group_conv) {
        /*
         * Count leading ones on 8-bit value to determine group size.
         * Count-leading-ones helper takes uint8_t.
         */
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
     * Each FP16 value occupies 2 crouton bytes (adjacent spatial positions),
     * so the spatial loop must skip odd positions.
     * Activation parameter handling.
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
 * Weight memory layout in VTCM (per 128B vector):
 *
 * Each 128B vector contains 32 output channels x 4 bytes.
 * Word at offset (och * 4) contains weights for output channel 'och'.
 * Within each 4-byte word, bytes map to input channels:
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
 * Clear / shift accumulator helpers
 */

void HELPER(hmx_clracc)(CPUHexagonState *env)
{
    HmxState *hmx = env->hmx_state;

    memset(&hmx->acc[0].fxp_primary, 0, sizeof(HmxAccFxp));
    memset(&hmx->acc[0].fxp_secondary, 0, sizeof(HmxAccFxp));
    memset(&hmx->acc[1].fxp_primary, 0, sizeof(HmxAccFxp));
    memset(&hmx->acc[1].fxp_secondary, 0, sizeof(HmxAccFxp));
}

void HELPER(hmx_clracc_hf)(CPUHexagonState *env)
{
    HmxState *hmx = env->hmx_state;
    memset(&hmx->acc[0].fp_primary, 0, sizeof(HmxAccFp));
    memset(&hmx->acc[0].fp_secondary, 0, sizeof(HmxAccFp));
    memset(&hmx->acc[1].fp_primary, 0, sizeof(HmxAccFp));
    memset(&hmx->acc[1].fp_secondary, 0, sizeof(HmxAccFp));
}

void HELPER(hmx_accshl)(CPUHexagonState *env)
{
    HmxState *hmx = env->hmx_state;
    int set, s, o;
    for (set = 0; set < HMX_NUM_ACC_SETS; set++) {
        HmxAccFxp *acc = &hmx->acc[set].fxp_primary;
        for (s = 0; s < HMX_SPATIAL_DIM_FXP; s++) {
            for (o = 0; o < HMX_OUTPUT_CHANNELS; o++) {
                acc->data[s][o] = (int32_t)((uint32_t)acc->data[s][o]
                                             << 16);
            }
        }
        acc = &hmx->acc[set].fxp_secondary;
        for (s = 0; s < HMX_SPATIAL_DIM_FXP; s++) {
            for (o = 0; o < HMX_OUTPUT_CHANNELS; o++) {
                acc->data[s][o] = (int32_t)((uint32_t)acc->data[s][o]
                                             << 16);
            }
        }
    }
}

/*
 * Fixed-point matrix multiply
 *
 * Weight data layout in VTCM (per 128B vector):
 *   Each 128B vector has 4 input channels x 32 output channels.
 *   Byte at offset b within vector:
 *     input_channel_offset = b / 32   (0-3)
 *     output_channel = b % 32         (0-31)
 *
 * Overall weight_byte[ich * 32 + och] for byte weights.
 *
 * Activations are UNSIGNED bytes. Weights are signed.
 * MAC: acc[s][o] += sum_i(uint8_t(act[s][i]) * int8_t(weight[i][o]))
 */

/*
 * ================================================================
 * Optimized weight preload + vectorized MAC for FXP matmul
 *
 * Instead of calling cpu_ldub_data_ra() per output channel per spatial
 * point, we bulk-load one 128-byte weight vector as 32 int32 words,
 * then extract weights on the host side using shifts.
 *
 * Weight vectors are 128 bytes = 32 output channels x 4 bytes each.
 * Each 4-byte word packs multiple stream indices depending on type.
 * ================================================================
 */

/* Bulk-load one 128-byte weight vector as 32 little-endian int32 words */
static inline void hmx_preload_weight_vec(CPUHexagonState *env,
                                           uint32_t wei_base,
                                           int vec_idx,
                                           uint32_t *wei_words,
                                           uintptr_t ra)
{
    uint32_t base = wei_base + vec_idx * 128;
    for (int i = 0; i < HMX_OUTPUT_CHANNELS; i++) {
        wei_words[i] = cpu_ldl_le_data_ra(env, base + i * 4, ra);
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
 * Small-format unpack computes (((in >> 7)) & 0x7F) ^ in.
 * Positive (bit7=0): result = magnitude.
 * Negative (bit7=1): result = 0x7F ^ in = ~magnitude = -(magnitude+1).
 * This maps [0x80..0xFF] to [-1..-128] with no negative zero.
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
 * Nibble: 8 stream indices per vector.
 * Extract 4-bit value and sign-extend: values 8-15 become -8..-1.
 * Nibble unpack uses sign extension to byte.
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
 * Crumb: 16 stream indices per vector.
 * Extract 2-bit value and sign-extend: {0,1,2,3} -> {0,1,-2,-1}.
 * Crumb unpack uses sign extension to byte.
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
 * V75 8x4 redundant accumulator MAC.
 *
 * Hardware decomposes each byte weight into lo/hi nibbles and
 * accumulates them into SEPARATE 32-bit accumulators:
 *   wgt_lo = (int8_t)byte & 0xF        (0..15)
 *   wgt_hi = (uint8_t)byte >> 4         (0..15)
 *   acc_lo += act * wgt_lo
 *   acc_hi += act * wgt_hi
 *
 * During convert, the combine produces:
 *   combined = (int64_t)(acc_hi << 4) + (int64_t)acc_lo
 *
 * The combined value per MAC equals act * (uint8_t)byte (unsigned).
 * For negative weights, this differs from act * (int8_t)byte by
 * act * 256.  NN bias registers are calibrated for this behavior.
 */
/*
 * Direct signed MAC for sub-byte weight types (nibble, crumb, bit).
 * These don't use the 8x4 decomposition even on V75.
 */
static inline void hmx_mac32(int32_t *acc_row, const int8_t *weights,
                              int32_t act_x_negate)
{
    for (int o = 0; o < HMX_OUTPUT_CHANNELS; o++) {
        acc_row[o] += act_x_negate * (int32_t)weights[o];
    }
}


/*
 * Range-limited MAC for group convolution.
 * Only accumulates into output channels [grp_start, grp_end).
 */
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
 * Reload activation crouton for a given block index.
 *
 * When blocks > 1 (deep activation mode with >2 croutons), the multiply
 * function must reload activation data for each crouton block.  Each 2KB
 * crouton is loaded from base_addr + crouton_idx * 2048.  Only 2KB is
 * loaded per crouton (no secondary block).
 *
 * For F8 activations, the loaded data is expanded to FP16 in-place.
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

    /*
     * F8 activations: expand F8-to-FP16 in-place at even spatial
     * positions.  Must iterate backwards since each 1-byte F8 value
     * expands to a 2-byte FP16 at the same offset.
     */
    if (act_type == HMX_ACT_F8) {
        int s, c;

        for (s = HMX_SPATIAL_DIM_FP - 1; s >= 0; s--) {
            int crouton_s = s * 2;
            for (c = HMX_INPUT_CHANNELS - 1; c >= 0; c--) {
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
 * When blocks > 1, each crouton covers a portion of the total input
 * channel range:
 *   - First crouton (idx=0): [ch_start, 32)
 *   - Middle croutons:       [0, 32)
 *   - Last crouton (idx=blocks-1): [0, ch_stop)
 *
 * Crouton-range calculation.
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

            if (spatial < 0 ||
                spatial >= HMX_SPATIAL_DIM_FXP) {
                continue;
            }

            int32_t act_idx =
                (act_y + intra_x + ch_addr) & 0xFFF;
            uint8_t act_val = hmx->act_buffer[act_idx];

            HmxAccFxp *acc =
                &hmx->acc[acc_sel].fxp_primary;
            int32_t act_x_neg = (int32_t)act_val * negate;

            if (grp_count > 1) {
                hmx_mac_group(acc->data[spatial],
                              weights, act_x_neg,
                              out_start, out_end);
            } else {
                hmx_mac32(acc->data[spatial],
                          weights, act_x_neg);
            }
        }
    }
}

void HELPER(hmx_matmul_fxp)(CPUHexagonState *env, uint32_t rs, uint32_t rt,
                             uint32_t params)
{
    HmxState *hmx = env->hmx_state;

    /* Apply deferred acc clear from a previous CVT packet */
    hmx_flush_acc_clear(hmx);

    int wei_type = HMX_UNPACK_WEI_TYPE(params);
    int wei_mod = HMX_UNPACK_MOD(params);
    int current_acc = hmx->current_acc_set;
    uintptr_t ra = GETPC();

    /* Weight address: Rs[31:7] is 128B-aligned */
    uint32_t wei_base = rs & 0xFFFFFF80;
    /*
     * Weight address range from Rt.
     * computes max_weight_pa = (base + (Rt & vtcm_mask)) | 0x7F and
     * only loads weight vectors within that range.  Vectors beyond the
     * range get valid=0 in the cache and are skipped during multiply.
     *
     * Simplified: max_valid_vec_idx = Rt >> 7 (number of 128B vectors
     * beyond the first).  With Rt=0, only one vector at wei_base is
     * valid.
     */
    int max_valid_vec = rt >> 7;
    /*
     * Weight negate: Rs[5] only applies to FP matmul, not FXP.
     * wgt_negate = 0 when !is_flt.
     */
    int negate = 1;

    /*
     * X-dimension tap parameters (determined by weight modifier).
     * Weight parameter handling.
     */
    uint32_t x_start = 0;
    uint32_t x_stop = 0;
    int x_dilate = 0;
    int deep = 0;
    int drop = 0;

    switch (wei_mod) {
    case HMX_MOD_NORMAL:  /* x_stop=fx, x_start=0 */
        x_stop = hmx->fx;
        break;
    case HMX_MOD_SINGLE:  /* single x tap at fx */
        x_start = hmx->fx;
        x_stop = hmx->fx;
        break;
    case HMX_MOD_DR:      /* drop: like normal but overflow dropped */
        x_stop = hmx->fx;
        drop = 1;
        break;
    case HMX_MOD_DP:      /* deep: single tap, fx=0 */
        deep = 1;
        x_stop = 0;
        break;
    case HMX_MOD_ABOVE:   /* after: taps from fx to end of tile */
        x_start = hmx->fx;
        x_stop = hmx->tile_x_mask;
        break;
    case HMX_MOD_DI:      /* dilate: normal with x dilation */
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

    /* Compute tap and intra-tile position arrays */
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
     * Sub-byte weight types iterate over all 32 input channels regardless
     * of the activation instruction's channel range (reference:
     * weight parameter handling).
     */
    int cpv = hmx_channels_per_vec[wei_type];
    if (cpv > 4) {
        ch_start = 0;
        ch_stop = 32;
    }

    /*
     * Main multiply loop with weight preload optimization.
     *
     * Legacy multiply path.
     * Weight vectors are consumed sequentially (wgt_stream_idx).
     * Each stream index = one raw input channel.
     *
     * Optimization: weights are preloaded once per weight vector (32 int32
     * words) and extracted once per stream index.  The same extracted
     * weights are then reused across all spatial positions, eliminating
     * redundant guest memory loads.
     *
     * Loop order: crouton -> y_tap -> deep_blk -> x_tap -> channel ->
     *             (preload+extract) -> spatial_y -> spatial_x -> MAC32
     *
     * When blocks > 1 (deep activation mode), the outermost loop iterates
     * over activation croutons, reloading 2KB of activation data per
     * crouton and adjusting the channel range.  Reference:
     * Full multiply math.
     */
    int wgt_stream_idx = 0;
    hmx_extract_fn extract = hmx_extract_table[wei_type];
    uint32_t wei_words[HMX_OUTPUT_CHANNELS];
    int prev_vec_idx = -1;

    /*
     * Deep mode processes twice the weight kernels: first set of 32
     * channels accumulates to primary, second set to secondary.
     * Loops over wgt_deep_idx
     * and flips primary_acc after each block.
     */
    int num_deep_blk = deep ? 2 : 1;

    /*
     * Deep activation: multiple crouton blocks.
     * Activation base address from the latched act_load instruction.
     */
    int num_croutons = hmx->blocks;
    uint32_t act_base = hmx->act_rs & 0xFFFFF800;

    for (int crouton_idx = 0; crouton_idx < num_croutons; crouton_idx++) {
        if (num_croutons > 1) {
            hmx_reload_act_crouton(env, hmx, act_base,
                                   crouton_idx,
                                   hmx->act_type, ra);
        }

        int crouton_ch_start, crouton_ch_stop;
        hmx_crouton_ch_range(crouton_idx, num_croutons,
                             ch_start, ch_stop,
                             &crouton_ch_start,
                             &crouton_ch_stop);

        for (int ytd = 0; ytd < y_tap_count; ytd++) {
            int y_tap = y_tap_array[ytd];

            for (int deep_blk = 0; deep_blk < num_deep_blk;
                 deep_blk++) {
                for (int xtd = 0; xtd < x_tap_count; xtd++) {
                    int x_tap = x_tap_array[xtd];

                    for (int ch = crouton_ch_start;
                         ch < crouton_ch_stop; ch++) {
                        /*
                         * Group convolution: each group resets the
                         * weight stream index and accumulates into
                         * its own output channels.  For non-group
                         * (count=1, size=32), single-pass.
                         */
                        int saved_wgt = wgt_stream_idx;
                        int grp_count = hmx->group_count;
                        int grp_size = hmx->group_size;

                        for (int grp = 0; grp < grp_count;
                             grp++) {
                            wgt_stream_idx = saved_wgt;

                            int ch_in = ch + grp * grp_size;
                            int ch_addr =
                                ch_in << format_offset;
                            int out_start = grp * grp_size;
                            int out_end =
                                out_start + grp_size;

                            int vec_idx =
                                wgt_stream_idx / cpv;
                            if (vec_idx > max_valid_vec) {
                                wgt_stream_idx++;
                                continue;
                            }

                            if (vec_idx != prev_vec_idx) {
                                hmx_preload_weight_vec(
                                    env, wei_base,
                                    vec_idx, wei_words,
                                    ra);
                                prev_vec_idx = vec_idx;
                            }

                            int8_t weights[
                                HMX_OUTPUT_CHANNELS];
                            int sub_idx =
                                wgt_stream_idx % cpv;
                            extract(wei_words, sub_idx,
                                    weights);

                            hmx_fxp_spatial_mac(
                                hmx, weights,
                                y_count, x_count,
                                intra_y_array,
                                intra_x_array,
                                y_tap, x_tap,
                                tile_x_mask, tile_y_mask,
                                ch_addr, drop, deep,
                                current_acc,
                                format_mask, negate,
                                grp_count,
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
}

static inline double hmx_fp16_to_double(uint16_t h)
{
    uint64_t sign = (uint64_t)(h >> 15) << 63;
    int exp = (h >> 10) & 0x1F;
    uint64_t mant = h & 0x3FF;

    if (likely(exp != 0 && exp != 0x1F)) {
        /* Normal: re-bias exponent and widen mantissa */
        uint64_t d_bits = sign
                        | ((uint64_t)(exp - 15 + 1023) << 52)
                        | (mant << 42);
        union { uint64_t u; double d; } u = { .u = d_bits };
        return u.d;
    }

    if (exp == 0) {
        if (mant == 0) {
            /* +/- zero */
            union { uint64_t u; double d; } u = { .u = sign };
            return u.d;
        }
        /* Denormal: normalize by shifting mantissa up */
        while (!(mant & 0x400)) {
            mant <<= 1;
            exp--;
        }
        exp++;
        mant &= 0x3FF;
        uint64_t d_bits = sign
                        | ((uint64_t)(exp - 15 + 1023) << 52)
                        | (mant << 42);
        union { uint64_t u; double d; } u = { .u = d_bits };
        return u.d;
    }

    /* exp == 0x1F: infinity or NaN */
    uint64_t d_bits = sign | UINT64_C(0x7FF0000000000000) | (mant << 42);
    union { uint64_t u; double d; } u = { .u = d_bits };
    return u.d;
}

static void hmx_fp_extract_weights(
    const uint32_t *wei_words, int sub_idx, int wei_type,
    int wei_negate, double *wei_dbl)
{
    for (int o = 0; o < HMX_OUTPUT_CHANNELS; o++) {
        uint16_t f16;
        if (wei_type == HMX_WEI_HF) {
            f16 = (wei_words[o] >> (sub_idx * 16)) & 0xFFFF;
        } else {
            static const int f8_byte_order[4] = {
                0, 2, 1, 3
            };
            int byte_pos = f8_byte_order[sub_idx];
            uint8_t f8 = (wei_words[o] >> (byte_pos * 8))
                         & 0xFF;
            if (f8 == 0x80) {
                f16 = 0xFE00;
            } else {
                f16 = ((f8 & 0x80) << 8) |
                      ((f8 & 0x7F) << 7);
            }
        }
        if (wei_negate) {
            f16 ^= 0x8000;
        }
        wei_dbl[o] = hmx_fp16_to_double(f16);
    }
}

static void hmx_fp_spatial_mac(
    HmxState *hmx, const double *wei_dbl,
    const uint16_t *act_fp,
    int y_count, int x_count,
    const int *intra_y_array, const int *intra_x_array,
    int y_tap, int x_tap,
    int32_t tile_x_mask, int32_t tile_y_mask,
    int ch_addr, int drop, int deep,
    int current_acc, int format_mask)
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

            if (spatial & 1) {
                continue;
            }
            int fp_spatial = spatial >> 1;
            if (fp_spatial < 0 ||
                fp_spatial >= HMX_SPATIAL_DIM_FP) {
                continue;
            }

            int32_t act_idx =
                (act_y + intra_x + ch_addr) & 0xFFF;
            double d_act =
                hmx_fp16_to_double(act_fp[act_idx >> 1]);

            HmxAccFp *acc =
                &hmx->acc[acc_sel].fp_primary;

            for (int o = 0; o < HMX_OUTPUT_CHANNELS; o++) {
                double d_prod = d_act * wei_dbl[o];

                union { uint64_t u; double d; } prev_u;
                prev_u.u = acc->data[fp_spatial][o];
                prev_u.d += d_prod;
                acc->data[fp_spatial][o] = prev_u.u;
            }
        }
    }
}

void HELPER(hmx_matmul_fp)(CPUHexagonState *env, uint32_t rs, uint32_t rt,
                            uint32_t params)
{
    HmxState *hmx = env->hmx_state;

    /* Apply deferred acc clear from a previous CVT packet */
    hmx_flush_acc_clear(hmx);

    int wei_type = HMX_UNPACK_WEI_TYPE(params);
    int wei_mod = HMX_UNPACK_MOD(params);
    int current_acc = hmx->current_acc_set;
    uintptr_t ra = GETPC();

    /*
     * Weight negate: Rs[5] flips sign of all weights for FP matmul.
     * wgt_negate = (Rs[5]) << 15
     * which is XORed with each FP16 weight's sign bit.
     */
    int wei_negate = (rs >> 5) & 1;
    uint32_t wei_base = rs & 0xFFFFFF80;
    int max_valid_vec = rt >> 7;

    /*
     * X-dimension tap parameters (same as FXP matmul).
     */
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
     * FP activations are loaded into act_buffer in raw crouton layout.
     * Access as uint16_t via byte_offset >> 1, matching the reference's
     * act_cache_uh[cache_idx >> 1] pattern.
     */
    uint16_t *act_fp = (uint16_t *)hmx->act_buffer;

    /*
     * Weight stream: FP16 = 2 weights per 32-bit word (64 bytes per vec),
     * F8 = 4 weights per word (32 bytes per vec).
     * Weights are consumed sequentially across taps and channels.
     */
    int wgt_stream_idx = 0;
    /*
     * Weight vector layout: 128B vectors with 32 output channels x 4 bytes.
     * FP16 (HF): 2 input channels per vector (2 FP16 per 4-byte word)
     * F8: 4 input channels per vector (4 bytes per word)
     */
    int cpv = (wei_type == HMX_WEI_HF) ? 2 : 4;
    uint32_t wei_words[HMX_OUTPUT_CHANNELS];
    int prev_vec_idx = -1;

    /*
     * Deep mode processes twice the weight kernels: first set of 32
     * channels accumulates to primary, second set to secondary.
     * Loops over wgt_deep_idx
     * and flips primary_acc after each block.
     */
    int num_deep_blk = deep ? 2 : 1;

    /*
     * Deep activation: multiple crouton blocks.
     * Activation base address from the latched act_load instruction.
     */
    int num_croutons = hmx->blocks;
    uint32_t act_base = hmx->act_rs & 0xFFFFF800;

    for (int crouton_idx = 0; crouton_idx < num_croutons; crouton_idx++) {
        if (num_croutons > 1) {
            hmx_reload_act_crouton(env, hmx, act_base,
                                   crouton_idx,
                                   hmx->act_type, ra);
        }

        int crouton_ch_start, crouton_ch_stop;
        hmx_crouton_ch_range(crouton_idx, num_croutons,
                             ch_start, ch_stop,
                             &crouton_ch_start,
                             &crouton_ch_stop);

        for (int ytd = 0; ytd < y_tap_count; ytd++) {
            int y_tap = y_tap_array[ytd];

            for (int deep_blk = 0; deep_blk < num_deep_blk;
                 deep_blk++) {
                for (int xtd = 0; xtd < x_tap_count; xtd++) {
                    int x_tap = x_tap_array[xtd];

                    for (int ch = crouton_ch_start;
                         ch < crouton_ch_stop; ch++) {
                        int vec_idx = wgt_stream_idx / cpv;
                        if (vec_idx > max_valid_vec) {
                            wgt_stream_idx++;
                            continue;
                        }

                        int ch_addr = ch << format_offset;

                        if (vec_idx != prev_vec_idx) {
                            hmx_preload_weight_vec(
                                env, wei_base, vec_idx,
                                wei_words, ra);
                            prev_vec_idx = vec_idx;
                        }

                        double wei_dbl[HMX_OUTPUT_CHANNELS];
                        int sub_idx = wgt_stream_idx % cpv;
                        hmx_fp_extract_weights(
                            wei_words, sub_idx, wei_type,
                            wei_negate, wei_dbl);

                        hmx_fp_spatial_mac(
                            hmx, wei_dbl, act_fp,
                            y_count, x_count,
                            intra_y_array, intra_x_array,
                            y_tap, x_tap,
                            tile_x_mask, tile_y_mask,
                            ch_addr, drop, deep,
                            current_acc, format_mask);

                        wgt_stream_idx++;
                    }
                }
                if (deep) {
                    current_acc = (current_acc ^ 1) & 1;
                }
            }
        }
    }
}

/*
 * Activation load
 *
 * Copy 2KB activation crouton from VTCM to internal buffer.
 * Latch Rs/Rt for use by subsequent weight multiply.
 *
 * Address: Rs[31:11] = 2KB-aligned base address
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

    /*
     * FP8 odd-byte select: Rs[0] picks the even (0) or odd (1) byte of each
     * 16-bit activation word for F8 expansion.
     * is_f8_odd = (start & 0b1), consumed by
     * hmx_act_f8_expand() as raw16 >> (is_f8_odd * 8).
     */
    hmx->is_f8_odd = (act_type == HMX_ACT_F8) ? (rs & 1) : 0;

    /* Compute multi-tap convolution parameters */
    int act_mod = HMX_UNPACK_ACT_MOD(params);
    hmx_compute_act_params(hmx, rs, rt, act_fmt, act_mod, act_type);

    /*
     * Load second activation crouton for multi-tap Y convolution.
     * Loads 2nd block from base + dY when
     * (blocks == 1) && (y_tap_count > 1 || y_start != 0).
     */
    int need_second = (hmx->blocks == 1) &&
        ((hmx->y_stop != hmx->y_start) || (hmx->y_start != 0));

    switch (act_type) {
    case HMX_ACT_UB:
        /* Load 2KB crouton using 32-bit word loads (4x fewer TLB lookups) */
        for (int i = 0; i < HMX_ACT_CROUTON_SIZE; i += 4) {
            uint32_t w = cpu_ldl_le_data_ra(env, base_addr + i, ra);
            stl_le_p(&hmx->act_buffer[i], w);
        }
        if (need_second) {
            uint32_t addr2 = base_addr + hmx->dY;
            for (int i = 0; i < HMX_ACT_CROUTON_SIZE; i += 4) {
                uint32_t w = cpu_ldl_le_data_ra(env, addr2 + i, ra);
                stl_le_p(&hmx->act_buffer[HMX_ACT_CROUTON_SIZE + i],
                          w);
            }
        }
        break;
    case HMX_ACT_HF:
    {
        /*
         * FP16: load 2KB crouton into act_buffer (raw crouton layout).
         * FP matmul accesses as uint16_t via byte_offset >> 1, matching
         * the reference's act_cache_uh[cache_idx >> 1] pattern.
         */
        for (int i = 0; i < HMX_ACT_CROUTON_SIZE; i += 4) {
            uint32_t w = cpu_ldl_le_data_ra(env, base_addr + i, ra);
            stl_le_p(&hmx->act_buffer[i], w);
        }
        if (need_second) {
            uint32_t addr2 = base_addr + hmx->dY;
            for (int i = 0; i < HMX_ACT_CROUTON_SIZE; i += 4) {
                uint32_t w = cpu_ldl_le_data_ra(env, addr2 + i, ra);
                stl_le_p(&hmx->act_buffer[HMX_ACT_CROUTON_SIZE + i],
                          w);
            }
        }
        break;
    }
    case HMX_ACT_F8:
    {
        /*
         * F8: Load full 2KB crouton into act_buffer, then expand
         * F8-to-FP16 in-place at even spatial positions.
         *
         * FP matmul reads act_buffer as uint16_t[], so the expanded
         * FP16 values must land at the same byte offsets as HF data.
         * F8 uses even spatial indices; each 1-byte F8 expands to a
         * 2-byte FP16 at the same offset.  The adjacent odd-spatial
         * byte is unused and safely overwritten.
         *
         * F8-to-FP16 expansion: simple bit shift (no exponent rebias).
         * HMX F8 is designed so raw bit shift produces valid FP16.
         * Special: 0x80 (negative zero) maps to 0xFE00.
         */
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
                stl_le_p(&hmx->act_buffer[HMX_ACT_CROUTON_SIZE + i],
                          w);
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
                        f16 = ((f8 & 0x80) << 8) |
                              ((f8 & 0x7F) << 7);
                    }
                    stw_le_p(&hmx->act_buffer[buf_off], f16);
                }
            }
        }
        break;
    }
    }

}

/*
 * Bias load/store
 *
 * Load/store 256 bytes (32 entries x 8 bytes) of raw bias data.
 * Address: Rs[31:7] = 128B-aligned address
 * Bias set: Rs[1:0] = set index (0-3)
 */

void HELPER(hmx_bias_load)(CPUHexagonState *env, uint32_t rs,
                           uint32_t is_mxmem2)
{
    HmxState *hmx = env->hmx_state;
    uintptr_t ra = GETPC();
    uint32_t addr = rs & 0xFFFFFF80;
    uint32_t set = rs & 0x3;

    if (is_mxmem2) {
        /*
         * mxmem2: load 256 bytes as two 128B HVX vectors
         * Vec0 (addr+0..127):   lower 32 bits of each channel (control fields)
         * Vec1 (addr+128..255): upper 32 bits of each channel (input_bias)
         */
        for (int i = 0; i < HMX_OUTPUT_CHANNELS; i++) {
            uint32_t lo = cpu_ldl_le_data_ra(env, addr + i * 4, ra);
            uint32_t hi = cpu_ldl_le_data_ra(env, addr + 128 + i * 4, ra);
            hmx->bias_raw[set][i] = ((uint64_t)hi << 32) | lo;
        }
    } else {
        /* mxmem: load 32-bit entries (128 bytes) into lower 32 bits */
        for (int i = 0; i < HMX_OUTPUT_CHANNELS; i++) {
            uint32_t lo = cpu_ldl_le_data_ra(env, addr + i * 4, ra);
            /* Preserve upper 32 bits (input_bias) */
            uint64_t old = hmx->bias_raw[set][i];
            hmx->bias_raw[set][i] = (old & 0xFFFFFFFF00000000ULL) | lo;
        }
    }
}

void HELPER(hmx_bias_store)(CPUHexagonState *env, uint32_t rs,
                            uint32_t is_mxmem2)
{
    HmxState *hmx = env->hmx_state;
    uintptr_t ra = GETPC();
    uint32_t addr = rs & 0xFFFFFF80;
    uint32_t set = rs & 0x3;

    if (is_mxmem2) {
        /*
         * mxmem2: store 256 bytes as two 128B HVX vectors
         * Vec0 (addr+0..127):   lower 32 bits of each channel
         * Vec1 (addr+128..255): upper 32 bits of each channel
         */
        for (int i = 0; i < HMX_OUTPUT_CHANNELS; i++) {
            uint64_t raw = hmx->bias_raw[set][i];
            cpu_stl_le_data_ra(env, addr + i * 4, (uint32_t)raw, ra);
            cpu_stl_le_data_ra(env, addr + 128 + i * 4,
                               (uint32_t)(raw >> 32), ra);
        }
    } else {
        /* mxmem: store lower 32 bits only (128 bytes) */
        for (int i = 0; i < HMX_OUTPUT_CHANNELS; i++) {
            uint64_t raw = hmx->bias_raw[set][i];
            cpu_stl_le_data_ra(env, addr + i * 4, (uint32_t)raw, ra);
        }
    }
}

/*
 * 128-bit arithmetic: use compiler native __int128 (available on x86-64
 * and aarch64 hosts) for the scale multiply in the convert pipeline.
 */

/*
 * Convert equation ported from the reference model
 * U8 convert helpers
 *
 * Fixed-point Q-format pipeline:
 *   1. ACC + input_bias
 *   2. Left-shift by exponent
 *   3. Clip to Q4.12 with summarization
 *   4. Shape function (rectify)
 *   5. Scale multiply (128-bit)
 *   6. Add output bias as rounding term
 *   7. Normalize and saturate
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

    /* Jamming / von Neumann rounding: only for non-legacy */
    if (!legacy && !disable_jam && acc_shifted) {
        acc_shifted |= 1LL << (31 - frac_bits);
    }

    /* Negate shapes need wider mask for intermediate */
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
 * Parameters:
 *   acc        - 32-bit accumulator value (sign-extended to 64-bit)
 *   bias32     - input bias (signed 32-bit, from bias register [63:32])
 *   exp        - exponent/shift amount (5-bit, [14:10])
 *   zeroing    - shape function selector (3-bit)
 *   sig        - scale value (unsigned 12-bit)
 *   out_bias   - output bias (unsigned 12-bit)
 *   sat        - saturation enable (1=clamp to [0,max], 0=mask)
 *   legacy     - 1 for legacy combined instructions
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
 * hmx_u16_cvt: unsigned halfword 16x8 (2x1) convert.
 *
 * Combines two adjacent spatial accumulators into a wider-precision
 * value.  Uses rounding (not output bias) per the reference.
 *
 * U16 convert handling
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
     * reintroduced as extra precision below the binary point, matching
     * the conversion only takes
     * effect for exp > 8; otherwise the extra bits are truncated by the
     * shift, so acc_biased_for_shift stays at the un-extended value.
     * acc_biased (extended) still feeds rectify's sign/zero detection.
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
 * hmx_u16x16_cvt: unsigned halfword 16x16 (2x2) convert.
 *
 * Combines four accumulator positions (2 spatial x 2 output channels)
 * into a single wider-precision value.  Uses output bias (unlike 2x1).
 *
 * U16x16 convert handling
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
 * Split a 20-bit UH result into lo/hi 16-bit CVT buffer entries.
 * Convert output low/high handling
 *
 * lo = result[11:0] (12 bits)
 * hi = result[19:8] aligned to bits [11:4] (12 bits, lower 4 zeroed)
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
 * Non-legacy FXP convert pipeline (8x8 byte)
 *
 * New-style convert instructions: cvt.ub = acc(Rs)
 * These use the deferred CVT pipeline with polynomial feedback support.
 *
 * Rs bitfield (convert control):
 *   [0]   acc_clear: 0=clear acc, 1=retain
 *   [1]   relu: 0=apply ReLU (clip negatives), 1=no ReLU. Active-low, matching Rs[0].
 *   [3:2] fb_dst: feedback destination (0=none, 1=out_bias, 2=scale)
 *   [4]   fb_limit: feedback limit mode
 *   [13:12] bias_sel
 *
 * Accumulator convert path
 * implementation notes
 */
static void hmx_fxp_convert(HmxState *hmx, int acc_set, int relu,
                             int bias_set, int fb_dst, int fb_limit,
                             uint32_t cur_pc)
{
    HmxAccFxp *acc = &hmx->acc[acc_set & 1].fxp_primary;

    /*
     * Deferred pipeline aging (matching reference commit_regs):
     *
     * Sets cvt_advance based on the LAST cvt_rs
     * in a packet: fb=0 sets cvt_advance=0 (age), fb!=0 sets
     * cvt_advance=1 (don't age).  When fb=0 and fb=2 are in the
     * same packet, fb=2 overwrites cvt_advance to 1, suppressing
     * the aging fb=0 requested.
     *
     * We defer the age+copy until the next consumer (store or next
     * fb=0) needs committed data.  On fb=0, we flush any previous
     * pending commit and mark the new convert as pending-with-age.
     * On fb!=0 in the same packet (same PC), we suppress the age.
     */
    if (fb_dst == 0) {
        /* New convert cycle: flush any pending from previous cycle */
        hmx_flush_cvt_fxp(hmx);
        hmx->cvt_fxp_pending = 1;
        hmx->cvt_fxp_pending_age = 1;
        hmx->cvt_fxp_pending_pc = cur_pc;
    } else {
        /* Feedback pass */
        if (hmx->cvt_fxp_pending &&
            cur_pc == hmx->cvt_fxp_pending_pc) {
            /* Same packet as fb=0: suppress aging */
            hmx->cvt_fxp_pending_age = 0;
        } else {
            /* Different packet or no prior pending: start new */
            hmx_flush_cvt_fxp(hmx);
            hmx->cvt_fxp_pending = 1;
            hmx->cvt_fxp_pending_age = 0;
            hmx->cvt_fxp_pending_pc = cur_pc;
        }
    }

    /* Feedback reads from the working buffer (cvt_future_fxp) */
    HmxCvtStateFxp *feedback_buf = &hmx->cvt_future_fxp;

    /* Hoist bias field extraction out of the spatial loop */
    int32_t  bias_input[HMX_OUTPUT_CHANNELS];
    int16_t  bias_exp[HMX_OUTPUT_CHANNELS];
    int16_t  bias_shape[HMX_OUTPUT_CHANNELS];
    int16_t  bias_scale[HMX_OUTPUT_CHANNELS];
    uint16_t bias_out[HMX_OUTPUT_CHANNELS];
    int o;

    for (o = 0; o < HMX_OUTPUT_CHANNELS; o++) {
        uint64_t raw = hmx->bias_raw[bias_set][o];
        bias_input[o] = hmx_bias_input_bias(raw);
        bias_exp[o]   = (int16_t)hmx_bias_exponent(raw);
        bias_shape[o] = (int16_t)hmx_bias_shape(raw);
        bias_scale[o] = (int16_t)hmx_bias_scale(raw);
        bias_out[o]   = hmx_bias_output_bias_unsigned(raw);
    }

    /* Convert all spatial x channel positions */
    HmxCvtStateFxp *cvt_out = &hmx->cvt_future_fxp;

    for (int s = 0; s < HMX_SPATIAL_DIM_FXP; s++) {
        for (o = 0; o < HMX_OUTPUT_CHANNELS; o++) {
            int64_t acc_combined = (int64_t)acc->data[s][o];

            int16_t scale = bias_scale[o];
            uint16_t out_bias = bias_out[o];

            /* Apply polynomial feedback if enabled */
            if (fb_dst != 0) {
                uint16_t fb = feedback_buf->data[s][o];
                if (fb_dst == 1) {
                    /* Feedback replaces output bias */
                    if (fb_limit) {
                        out_bias = (out_bias > fb)
                                 ? out_bias : fb;
                    } else {
                        out_bias = (out_bias > fb)
                                 ? fb : out_bias;
                    }
                } else if (fb_dst == 2) {
                    /* Feedback replaces scale */
                    if (fb_limit) {
                        scale = (scale > (int16_t)fb)
                              ? scale : (int16_t)fb;
                    } else {
                        scale = (scale > (int16_t)fb)
                              ? (int16_t)fb : scale;
                    }
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
 * UH 2x1 (16x8) FXP convert: combines adjacent spatial pairs.
 *
 * Iterates spatial positions in steps of 2.  For each pair (s, s+1),
 * reads both accumulators and passes to hmx_u16_cvt.  The 20-bit
 * result is split into lo/hi halves stored at the two spatial entries.
 *
 * Accumulator convert path
 * implementation notes
 */
static void hmx_fxp_convert_2x1(HmxState *hmx, int acc_set, int relu,
                                  int bias_set, int fb_dst, int extra_8bit,
                                  uint32_t cur_pc)
{
    HmxAccFxp *acc = &hmx->acc[acc_set & 1].fxp_primary;

    /* Same deferred aging as UB convert */
    if (fb_dst == 0) {
        hmx_flush_cvt_fxp(hmx);
        hmx->cvt_fxp_pending = 1;
        hmx->cvt_fxp_pending_age = 1;
        hmx->cvt_fxp_pending_pc = cur_pc;
    } else {
        if (hmx->cvt_fxp_pending &&
            cur_pc == hmx->cvt_fxp_pending_pc) {
            hmx->cvt_fxp_pending_age = 0;
        } else {
            hmx_flush_cvt_fxp(hmx);
            hmx->cvt_fxp_pending = 1;
            hmx->cvt_fxp_pending_age = 0;
            hmx->cvt_fxp_pending_pc = cur_pc;
        }
    }

    /* Hoist bias field extraction */
    int32_t  bias_input[HMX_OUTPUT_CHANNELS];
    int16_t  bias_exp[HMX_OUTPUT_CHANNELS];
    int16_t  bias_shape[HMX_OUTPUT_CHANNELS];
    int16_t  bias_scale[HMX_OUTPUT_CHANNELS];
    uint32_t bias1[HMX_OUTPUT_CHANNELS];
    uint16_t bias_rnd[HMX_OUTPUT_CHANNELS];
    int o;

    for (o = 0; o < HMX_OUTPUT_CHANNELS; o++) {
        uint64_t raw = hmx->bias_raw[bias_set][o];
        bias_input[o] = hmx_bias_input_bias(raw);
        bias_exp[o]   = (int16_t)hmx_bias_exponent(raw);
        bias_shape[o] = (int16_t)hmx_bias_shape(raw);
        bias_scale[o] = (int16_t)hmx_bias_scale(raw);
        bias1[o]      = (raw >> 23) & 0xFF;
        /*
         * UH 2x1 uses rnd_bit (BIAS[22] = output_bias bit 3)
         * instead of the full 12-bit output bias.
         */
        bias_rnd[o] = (raw >> 22) & 1;
    }

    HmxCvtStateFxp *cvt_out = &hmx->cvt_future_fxp;

    /* Spatial stride = 2: process adjacent pairs */
    for (int s = 0; s < HMX_SPATIAL_DIM_FXP; s += 2) {
        for (o = 0; o < HMX_OUTPUT_CHANNELS; o++) {
            int64_t acc_ll = (int64_t)acc->data[s][o];
            int64_t acc_hl = (int64_t)acc->data[s + 1][o];
            uint32_t poly_scale = ((uint32_t)bias_scale[o] << 8) | bias1[o];

            uint32_t result = hmx_u16_cvt(
                acc_hl, acc_ll, bias_input[o], bias_exp[o],
                bias_shape[o], poly_scale, bias_rnd[o],
                /* sat */ relu, /* legacy */ 0,
                /* has_feedback */ 0,
                /* has_extra_acc_bits */ extra_8bit);

            /* 24-bit result >> 4 = 20-bit, split into lo/hi */
            result >>= 4;
            cvt_out->data[s][o] = hmx_cvt_out_lo(result);
            cvt_out->data[s + 1][o] = hmx_cvt_out_hi(result);
        }
    }
}

/*
 * UH 2x2 (16x16) FXP convert: combines 2x2 spatial+channel blocks.
 *
 * Iterates spatial in steps of 2 and output channels in steps of 2.
 * For each 2x2 block, reads four accumulators and passes to
 * hmx_u16x16_cvt.  Scale and output bias are concatenated from two
 * adjacent bias registers.
 *
 * Accumulator convert path
 * implementation notes
 */
static void hmx_fxp_convert_2x2(HmxState *hmx, int acc_set, int relu,
                                  int bias_set, int fb_dst, int ch_sel,
                                  uint32_t cur_pc)
{
    HmxAccFxp *acc = &hmx->acc[acc_set & 1].fxp_primary;

    /*
     * fxp16_ch_sel (cvt Rs[10:9]) selects which output channel of each
     * 2x2 block the 16-bit result lands in, and which accumulators feed
     * the convert:
     *
     *   output_adjust = (ch_sel == 2) ? 0 : 1;
     *   if (ch_sel != 3) {
     *       if (ch_sel == 2) { acc_lh = acc_ll; acc_hh = acc_hl; }
     *       acc_ll = 0; acc_hl = 0;
     *   }
     *   ...
     *   cvt[..][o + output_adjust]  = result_{lo,hi};
     *   if (!fb_dst) cvt[..][o + !output_adjust] = 0;
     *
     * QEMU previously hardcoded the ch_sel==2 behavior (write o, zero o+1),
     * which produced all-zero odd output channels for ch_sel 0/1.
     */
    int output_adjust = (ch_sel == 2) ? 0 : 1;

    /* Same deferred aging as UB convert */
    if (fb_dst == 0) {
        hmx_flush_cvt_fxp(hmx);
        hmx->cvt_fxp_pending = 1;
        hmx->cvt_fxp_pending_age = 1;
        hmx->cvt_fxp_pending_pc = cur_pc;
    } else {
        if (hmx->cvt_fxp_pending &&
            cur_pc == hmx->cvt_fxp_pending_pc) {
            hmx->cvt_fxp_pending_age = 0;
        } else {
            hmx_flush_cvt_fxp(hmx);
            hmx->cvt_fxp_pending = 1;
            hmx->cvt_fxp_pending_age = 0;
            hmx->cvt_fxp_pending_pc = cur_pc;
        }
    }

    HmxCvtStateFxp *cvt_out = &hmx->cvt_future_fxp;

    /* Spatial stride = 2, output stride = 2 */
    for (int s = 0; s < HMX_SPATIAL_DIM_FXP; s += 2) {
        for (int o = 0; o < HMX_OUTPUT_CHANNELS; o += 2) {
            uint64_t raw_lo = hmx->bias_raw[bias_set][o];
            uint64_t raw_hi = hmx->bias_raw[bias_set][o + 1];

            int32_t input_bias = hmx_bias_input_bias(raw_lo);
            int16_t exp = (int16_t)hmx_bias_exponent(raw_lo);
            int16_t shape = (int16_t)hmx_bias_shape(raw_lo);

            /*
             * 2x2 concatenates scale from both bias registers:
             *   scale = ((~sigmsb_hi << 10) | sig_hi) << 11) |
             *           (sig_lo << 1) | siglsb_lo
             * Convert-body handling
             */
            uint32_t sig_lo = hmx_bias_scale(raw_lo);
            uint32_t sigmsb_hi = !((raw_hi >> 16) & 1);
            uint32_t sig_hi = raw_hi & 0x3FF;
            uint32_t scale = (((sigmsb_hi << 10) | sig_hi) << 11) |
                             (sig_lo & 0x7FF);

            /*
             * 2x2 concatenates output bias from both registers:
             *   output_bias = ((bias1_hi << 12) | bias_lo)
             * Forms output_bias << 2
             * but then passes poly_bias = output_bias >> 2 to
             * the net rnd_bit value is the
             * un-shifted concatenation.  (The earlier QEMU code applied the
             * << 2 without the matching >> 2, making out_bias 4x too large
             * and biasing every converted value high.)
             */
            uint32_t bias1_hi = (raw_hi >> 23) & 0xFF;
            int32_t bias_lo_val = hmx_bias_output_bias(raw_lo);
            uint32_t out_bias = (uint32_t)((bias1_hi << 12) +
                                 (bias_lo_val & 0xFFF));

            /* Input bias extended to 48 bits */
            int64_t bias48 = (int64_t)input_bias << 16;

            /* Read four accumulator positions */
            int64_t acc_ll = (int64_t)acc->data[s][o];
            int64_t acc_hl = (int64_t)acc->data[s + 1][o];
            int64_t acc_lh = (int64_t)acc->data[s][o + 1];
            int64_t acc_hh = (int64_t)acc->data[s + 1][o + 1];

            /*
             * Accumulator selection per ch_sel.
             */
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

            /* 24-bit result >> 4 = 20-bit, split into lo/hi */
            result >>= 4;
            cvt_out->data[s][o + output_adjust] = hmx_cvt_out_lo(result);
            cvt_out->data[s + 1][o + output_adjust] = hmx_cvt_out_hi(result);
            /*
             * Clear the alternate output channel entries when not
             * in feedback mode.
             */
            if (!fb_dst) {
                cvt_out->data[s][o + !output_adjust] = 0;
                cvt_out->data[s + 1][o + !output_adjust] = 0;
            }
        }
    }
}

/*
 * Combined convert transfer + store (legacy instruction format)
 *
 * Legacy instructions: mxmem(Rs,Rt):before/after[:retain][:cm]:sat.ub=acc
 * Rs/Rt are STORE ADDRESSES, not convert control registers.
 *
 * The params encode: direction (before=left, after=right),
 * format (SM/DM), relu, retain.
 * Bias set is always 0 for legacy combined instructions.
 *
 * After convert, the result is stored to VTCM at address Rs.
 */

void HELPER(hmx_cvt_transfer)(CPUHexagonState *env, uint32_t rs, uint32_t rt,
                               uint32_t params)
{
    HmxState *hmx = env->hmx_state;
    int dir = HMX_UNPACK_CVT_DIR(params);
    int fmt = HMX_UNPACK_CVT_FMT(params);
    int relu = HMX_UNPACK_CVT_RELU(params);
    int retain = HMX_UNPACK_CVT_RETAIN(params);
    uintptr_t ra = GETPC();

    /*
     * Flush any pending new-style convert before we age.
     * Without this, a prior cvt_rs that set pending=1 would
     * leave stale state that causes double-aging later.
     */
    hmx_flush_cvt_fxp(hmx);
    hmx_flush_acc_clear(hmx);

    /* Legacy: always use bias set 0, no control bits from Rs */
    uint32_t bias_set = 0;

    /* Store base address from Rs (2KB-aligned) */
    uint32_t store_base = rs & 0xFFFFF800;


    if (fmt == HMX_CVT_FMT_HF) {
        /* FP16 path: always age and convert */
        hmx->cvt_fp[2] = hmx->cvt_fp[1];
        hmx->cvt_fp[1] = hmx->cvt_fp[0];
        hmx_fp_convert(env, hmx, hmx->current_acc_set, 0, relu,
                       bias_set, /* maxnorm */ 0, /* fp8_odd_sel */ 0);

        /* Store FP16 values to VTCM in crouton SM layout (2 bytes each) */
        HmxCvtStateFp *cvt_fp = &hmx->cvt_fp[0];
        for (int s = 0; s < HMX_SPATIAL_DIM_FP; s++) {
            for (int o = 0; o < HMX_OUTPUT_CHANNELS; o++) {
                uint16_t fp16 = cvt_fp->data[s][o];
                int crouton_s = s * 2;
                int offset = hmx_act_offset_sm(crouton_s, o);
                cpu_stw_le_data_ra(env, store_base + offset,
                                   fp16, ra);
            }
        }
        /* Clear FP accumulator unless retaining */
        if (!retain) {
            HmxAccFp *acc_fp =
                &hmx->acc[hmx->current_acc_set].fp_primary;
            memset(acc_fp, 0, sizeof(HmxAccFp));
            hmx->current_acc_set ^= 1;
        }
        return;
    }

    /* Fixed-point convert path */
    HmxAccFxp *acc = &hmx->acc[hmx->current_acc_set].fxp_primary;
    HmxCvtStateFxp *cvt = &hmx->cvt_fxp[0];

    /*
     * Direction: The before/after split point is determined by Rs masked
     * with the spatial mask derived from Rt.
     *
     * BEFORE (dir=0): stores to spatial positions where X < x_offset
     * AFTER  (dir=1): stores to spatial positions where X >= x_offset
     *
     * When x_offset=0, AFTER covers all positions.  The convert phase
     * always processes all 64 spatial positions; direction only restricts
     * which positions are STORED to VTCM.
     */
    int s_start = 0, s_end = HMX_SPATIAL_DIM_FXP;

    /* Compute spatial masks and split point for direction handling */
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

    /*
     * Hoist bias field extraction out of the spatial loop.
     * Bias depends only on channel (o), not spatial position (s),
     * so decoding once per channel saves 64x redundant work.
     */
    int32_t  bias_input[HMX_OUTPUT_CHANNELS];
    int16_t  bias_exp[HMX_OUTPUT_CHANNELS];
    int16_t  bias_shape[HMX_OUTPUT_CHANNELS];
    int16_t  bias_scale[HMX_OUTPUT_CHANNELS];
    uint16_t bias_out[HMX_OUTPUT_CHANNELS];

    for (int o = 0; o < HMX_OUTPUT_CHANNELS; o++) {
        uint64_t raw = hmx->bias_raw[bias_set][o];
        bias_input[o] = hmx_bias_input_bias(raw);
        bias_exp[o]   = (int16_t)hmx_bias_exponent(raw);
        bias_shape[o] = (int16_t)hmx_bias_shape(raw);
        bias_scale[o] = (int16_t)hmx_bias_scale(raw);
        bias_out[o]   = hmx_bias_output_bias_unsigned(raw);
    }

    if (fmt == HMX_CVT_FMT_UH) {
        /*
         * UH 2x1: combine adjacent spatial pairs.
         * Uses rnd_bit (BIAS[22]) instead of the full output bias.
         * 16x8 convert body
         */
        for (int s = s_start; s < s_end; s += 2) {
            for (int o = 0; o < HMX_OUTPUT_CHANNELS; o++) {
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
                    /* has_feedback */ 0,
                    /* has_extra_acc_bits */ 0);

                result >>= 4;
                cvt->data[s][o] = hmx_cvt_out_lo(result);
                cvt->data[s + 1][o] = hmx_cvt_out_hi(result);
            }
        }
    } else if (fmt == HMX_CVT_FMT_UH2X2) {
        /*
         * UH 2x2: combine 2x2 spatial+channel blocks.
         * Scale and output bias are concatenated from adjacent
         * bias registers.
         * 16x16 convert body
         */
        for (int s = s_start; s < s_end; s += 2) {
            for (int o = 0; o < HMX_OUTPUT_CHANNELS; o += 2) {
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
            for (int o = 0; o < HMX_OUTPUT_CHANNELS; o++) {
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
     * Store convert state to VTCM (respecting direction/split).
     *
     * Direction can be X-based (LEFT/RIGHT for UB) or Y-based
     * (BOTTOM/ABOVE for UH/UH2X2).  The split axis variables
     * are selected accordingly.
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

        /*
         * Compute the accumulator spatial index (acc_s) that maps
         * to this memory position.  The before/after mechanism
         * rotates the acc-to-memory mapping on the split axis.
         */
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

        for (int o = 0; o < HMX_OUTPUT_CHANNELS; o++) {
            int offset;
            if (is_cm) {
                offset = hmx_act_offset_cm(s, o);
            } else {
                offset = hmx_act_offset_sm(s, o);
            }
            uint8_t val = (cvt->data[acc_s][o] >> 4) & 0xFF;
            uint32_t addr = store_base + offset;
            cpu_stb_data_ra(env, addr, val, ra);
        }
    }

    /* Clear accumulator and flip acc set. */
    if (!retain) {
        for (int s = s_start; s < s_end; s++) {
            memset(&acc->data[s], 0, sizeof(acc->data[s]));
        }
        hmx->current_acc_set ^= 1;
    }
}

/*
 * Convert extended FP16 value (FP16 base + extra precision bits) to double.
 *
 * The extra bits extend the FP16 mantissa downward. They are packed
 * as: combined = (fp16 << extra_width) | extra_bits, interpreted as
 * a floating-point number with (10 + extra_width) mantissa bits and
 * 5 exponent bits (bias=15).
 */
static double hmx_xfp16_to_double(uint16_t fp16, uint32_t extra,
                                   int extra_width)
{
    uint32_t combined = ((uint32_t)fp16 << extra_width) | extra;
    int frac_bits = 10 + extra_width;
    int sign = (combined >> (5 + frac_bits)) & 1;
    int exp = (combined >> frac_bits) & 0x1F;
    uint32_t man = combined & ((1u << frac_bits) - 1);

    double result;
    if (exp == 0) {
        if (man == 0) {
            return 0.0;
        }
        /* Denorm: man * 2^(1 - bias - frac_bits) */
        result = ldexp((double)man, 1 - 15 - frac_bits);
    } else if (exp == 31) {
        /*
         * HMX XFP treats exp=31 man=0 as infinity (tracked via status
         * flags). Using IEEE infinity here lets the arithmetic propagate
         * correctly, and we handle clipping at the final F8/FP16 output.
         */
        if (man == 0) {
            return sign ? -INFINITY : INFINITY;
        }
        /* exp=31 man!=0 is NaN in IEEE; treat as NaN */
        return NAN;
    } else {
        /* Normal: (2^frac_bits + man) * 2^(exp - bias - frac_bits) */
        result = ldexp((double)((1u << frac_bits) + man),
                       exp - 15 - frac_bits);
    }
    return sign ? -result : result;
}

/*
 * Convert a double to FP16 (IEEE half-precision, 1-5-10, bias=15).
 * Uses round-to-nearest-even.
 */
static uint16_t hmx_double_to_fp16(double val)
{
    if (val == 0.0) {
        return signbit(val) ? 0x8000 : 0;
    }
    if (isinf(val)) {
        return val > 0 ? 0x7C00 : 0xFC00;
    }

    uint16_t sign = val < 0 ? 1 : 0;
    val = fabs(val);

    int exp;
    double frac = frexp(val, &exp);
    int biased_exp = exp + 14;

    if (biased_exp >= 31) {
        return (sign << 15) | 0x7C00;
    }
    if (biased_exp <= 0) {
        int shift = 1 - biased_exp;
        if (shift > 10) {
            return 0;
        }
        int man = (int)(ldexp(frac, 11 - shift) + 0.5);
        if (man >= (1 << (11 - shift))) {
            /* Carry into next exponent */
            biased_exp += man >> (11 - shift);
            man = 0;
            if (biased_exp >= 31) {
                return (sign << 15) | 0x7C00;
            }
            if (biased_exp > 0) {
                return (sign << 15) | (biased_exp << 10);
            }
        }
        return (sign << 15) | (man & 0x3FF);
    }

    int man = (int)((frac * 2.0 - 1.0) * 1024.0 + 0.5);
    if (man >= 1024) {
        man = 0;
        biased_exp++;
        if (biased_exp >= 31) {
            return (sign << 15) | 0x7C00;
        }
    }
    return (sign << 15) | (biased_exp << 10) | (man & 0x3FF);
}

/*
 * Convert a double to e4m3 FP8 format (1 sign + 4 exp + 3 mantissa).
 * Bias = 7. Uses round-to-nearest-even.
 *
 * Normal: (1 + man/8) * 2^(exp-7) for exp in 1..14
 * Denorm: (man/8) * 2^(-6) for exp=0
 * exp=15 with man=7 is NaN; exp=15 with man<7 is valid.
 */
static uint8_t hmx_double_to_f8e4m3(double val)
{
    if (val == 0.0) {
        return 0;
    }

    uint8_t sign = val < 0 ? 1 : 0;
    val = fabs(val);

    /*
     * e4m3 max finite: exp=15, man=6 -> (1+6/8)*2^8 = 448.
     * exp=15 man=7 (0x7F) is NaN, so max representable is 0x7E = 448.
     * Values >= 448 + 32 (midpoint to NaN) saturate to max.
     */
    if (val >= 480.0) {
        return (sign << 7) | 0x7E;  /* max finite */
    }

    int exp;
    double frac = frexp(val, &exp);
    /* frac in [0.5, 1.0), val = frac * 2^exp */
    int biased_exp = exp + 6;  /* exp - 1 + 7 */

    if (biased_exp <= 0) {
        /* Denorm: man/8 * 2^(-6), so man = val / 2^(-9) = val * 512 */
        int man = (int)(val * 512.0 + 0.5);
        if (man > 7) {
            /* Overflows to smallest normal */
            return (sign << 7) | (1 << 3);
        }
        if (man <= 0) {
            return 0;
        }
        return (sign << 7) | (man & 0x7);
    }

    if (biased_exp > 15) {
        return (sign << 7) | 0x7E;  /* max finite */
    }

    int man = (int)((frac * 2.0 - 1.0) * 8.0 + 0.5);
    if (man >= 8) {
        man = 0;
        biased_exp++;
    }
    if (biased_exp > 15 || (biased_exp == 15 && man >= 7)) {
        return (sign << 7) | 0x7E;  /* max finite (exp=15 man=7 is NaN) */
    }
    return (sign << 7) | (biased_exp << 3) | (man & 0x7);
}

/*
 * FP bias register layout (from raw 64-bit bias value):
 *   [15:0]   = scale (FP16)
 *   [31:16]  = out_bias (FP16)
 *   [35:32]  = scale_extra (4 bits below FP16 mantissa)
 *   [39:36]  = out_bias_extra (4 bits)
 *   [41:40]  = shape (0=pass, 1=min(0,x), 2=max(0,x))
 *   [42]     = negate
 *   [47:43]  = acc_bias_extra (5 bits)
 *   [63:48]  = acc_bias (FP16)
 *
 * FP convert pipeline:
 *   acc -> +acc_bias -> shape -> *scale -> +out_bias -> F8/FP16
 *
 * The cvt.f8 instruction reads from the FP accumulator (not FXP).
 * FP8 results are packed two-per-slot in cvt_fp: low byte = even spatial,
 * high byte = odd spatial (see HmxCvtStateFp comment in hmx_state.h).
 */
/*
 * FP16 overflow/NaN fixup based on USR[20:21] and Rs[6] modes.
 *
 * Applies to FP16 values with exp=31 (Inf or NaN encoding).
 * The four modes control what the hardware produces for these
 * special cases (see ch10_convert_transfer spec):
 *
 *   Mode 0 (inf_prop=0):           saturate to ±max_finite
 *   Mode 1 (inf_prop=1, maxn=0):   pass through IEEE ±Inf/NaN
 *   Mode 2 (inf_prop=1, maxn=1, nan_prop=0): clamp to ±max(emax-1)
 *   Mode 3 (inf_prop=1, maxn=1, nan_prop=1): clamp Inf to
 *           ±max(emax-1), NaN to -max_finite
 */
static uint16_t hmx_fp16_fixup(uint16_t fp16, int inf_prop,
                                int nan_prop, int maxnorm)
{
    uint16_t sign = fp16 & 0x8000;
    int is_nan = ((fp16 & 0x7C00) == 0x7C00) && (fp16 & 0x03FF);

    if (is_nan) {
        if (!inf_prop) {
            return 0xFBFF;      /* Mode 0: -max_finite */
        }
        if (!maxnorm) {
            return 0xFFFF;      /* Mode 1: canonical NaN */
        }
        if (nan_prop) {
            return 0xFBFF;      /* Mode 3: -max_finite */
        }
        return 0xF7FF;          /* Mode 2: -max(emax-1) */
    }

    /* Inf (or overflow that became Inf) */
    if (!inf_prop) {
        return sign | 0x7BFF;   /* Mode 0: ±max_finite */
    }
    if (!maxnorm) {
        return fp16;            /* Mode 1: ±Inf */
    }
    return sign | 0x77FF;       /* Mode 2/3: ±max(emax-1) */
}

/*
 * F8 (e4m3) overflow/NaN fixup based on USR[20:21] and Rs[6] modes.
 *
 * F8 has no Inf encoding (exp=15 man=7 is NaN, all others are
 * finite), so Inf→max_finite in all modes.  Mode-dependent
 * differences are in the exponent used for saturation and
 * NaN behavior.
 */
static uint8_t hmx_f8_fixup(double val, int inf_prop,
                              int nan_prop, int maxnorm)
{
    uint8_t sign = signbit(val) ? 0x80 : 0;

    if (isnan(val)) {
        if (!inf_prop) {
            return 0xFE;        /* Mode 0: -max_finite */
        }
        if (!maxnorm) {
            return 0xFF;        /* Mode 1: canonical NaN */
        }
        if (nan_prop) {
            return 0xFE;        /* Mode 3: -max_finite */
        }
        return 0xF7;            /* Mode 2: -max(emax-1) */
    }

    /* Inf or overflow */
    if (!inf_prop || !maxnorm) {
        return sign | 0x7E;     /* Mode 0/1: ±max_finite */
    }
    return sign | 0x77;         /* Mode 2/3: ±max(emax-1) */
}

static void hmx_fp_convert(CPUHexagonState *env, HmxState *hmx,
                            int acc_set, int is_f8,
                            int relu, int bias_sel, int maxnorm,
                            int fp8_odd_sel)
{
    uint32_t usr = env->gpr[HEX_REG_USR];
    int inf_prop = (usr >> 20) & 1;
    int nan_prop = (usr >> 21) & 1;

    HmxAccFp *acc_fp = &hmx->acc[acc_set].fp_primary;
    HmxCvtStateFp *cvt = &hmx->cvt_fp[0];

    for (int o = 0; o < HMX_OUTPUT_CHANNELS; o++) {
        uint64_t raw = hmx->bias_raw[bias_sel][o];

        /* Extract FP bias fields */
        uint16_t scale_fp16 = raw & 0xFFFF;
        uint16_t out_bias_fp16 = (raw >> 16) & 0xFFFF;
        uint32_t scale_extra = (raw >> 32) & 0xF;
        uint32_t out_bias_extra = (raw >> 36) & 0xF;
        uint32_t shape = (raw >> 40) & 0x3;
        uint32_t negate = (raw >> 42) & 0x1;
        uint32_t acc_bias_extra = (raw >> 43) & 0x1F;
        uint16_t acc_bias_fp16 = (raw >> 48) & 0xFFFF;

        /* Convert extended FP bias values to double */
        double d_acc_bias = hmx_xfp16_to_double(acc_bias_fp16,
                                                 acc_bias_extra, 5);
        double d_scale = hmx_xfp16_to_double(scale_fp16,
                                              scale_extra, 4);
        double d_out_bias = hmx_xfp16_to_double(out_bias_fp16,
                                                  out_bias_extra, 4);

        for (int s = 0; s < HMX_SPATIAL_DIM_FP; s++) {
            /*
             * Read from FP accumulator.  The matmul stores IEEE double
             * bit patterns in uint64_t slots - reinterpret, not cast.
             */
            union { uint64_t u; double d; } acc_bits;
            acc_bits.u = acc_fp->data[s][o];
            double d_acc = acc_bits.d;

            /* Add input bias */
            double d_biased = d_acc + d_acc_bias;

            /*
             * Compute per-element scale sign.
             * shape=3 means |x|. The abs is achieved by
             * conditionally flipping the scale sign based on the
             * accumulator sign: abs_negate = (shape==3 && biased<0).
             * Final scale_neg = negate ^ abs_negate.
             */
            int abs_negate = (shape == 3 && d_biased < 0) ? 1 : 0;
            int scale_neg = negate ^ abs_negate;

            /* Apply shape (min/max only; shape=3 handled via scale) */
            switch (shape) {
            case 1:
                d_biased = fmin(d_biased, 0.0);
                break;
            case 2:
                d_biased = fmax(d_biased, 0.0);
                break;
            }

            /* Scale (with per-element negate) */
            double d_scaled = d_biased *
                (scale_neg ? -d_scale : d_scale);

            /* Add output bias */
            double d_result = d_scaled + d_out_bias;

            if (is_f8) {
                uint8_t f8;

                if (isnan(d_result) || isinf(d_result) ||
                    fabs(d_result) > 448.0) {
                    f8 = hmx_f8_fixup(d_result, inf_prop,
                                       nan_prop, maxnorm);
                } else {
                    f8 = hmx_double_to_f8e4m3(d_result);
                }
                /*
                 * Pack the FP8 result into the half selected by
                 * fp8_odd_sel (cvt Rs[11]), preserving the other half.
                 * Low byte = even spatial (crouton 2s), high byte = odd
                 * spatial (crouton 2s+1).  See HmxCvtStateFp comment.
                 */
                uint16_t prev = cvt->data[s][o];
                if (fp8_odd_sel) {
                    cvt->data[s][o] = (prev & 0x00FF) | ((uint16_t)f8 << 8);
                } else {
                    cvt->data[s][o] = (prev & 0xFF00) | (uint16_t)f8;
                }
            } else {
                uint16_t fp16 = hmx_double_to_fp16(d_result);

                /* Fix up Inf/NaN per USR overflow mode */
                if ((fp16 & 0x7C00) == 0x7C00) {
                    fp16 = hmx_fp16_fixup(fp16, inf_prop,
                                           nan_prop, maxnorm);
                }
                cvt->data[s][o] = fp16;
            }
        }
    }
}


/*
 * Convert transfer + optional scalar register read
 *
 * This instruction (cvt.ub=acc(Rs), cvt.hf=acc(Rs), etc.) triggers
 * the full convert pipeline that fills cvt_fxp/cvt_fp from the
 * accumulator, then optionally returns a scalar value.
 *
 * Reference: M8_cvt_rs_ub semantics call fMX_CVT_TX_PARAMETERS()
 * then fMX_CVT() runs the convert path.
 *
 * Rs bitfield (convert control):
 *   [0]     acc_clear: 0=clear acc after convert, 1=retain
 *   [1]     relu: 0=apply ReLU (clip negatives), 1=no ReLU. Active-low, matching Rs[0].
 *   [13:12] bias_sel
 */

uint32_t HELPER(hmx_cvt_rs)(CPUHexagonState *env, uint32_t rs, uint32_t type)
{
    HmxState *hmx = env->hmx_state;
    int relu = !((rs >> 1) & 1);
    int bias_sel = (rs >> 12) & 0x3;
    int fb_dst = (rs >> 2) & 0x3;
    int fb_limit = (rs >> 4) & 0x1;
    int maxnorm = (rs >> 6) & 1;
    int fp8_odd_sel = (rs >> 11) & 1;  /* FP8 convert: write odd half */

    /*
     * acc_clear: Rs[0]=0 means clear accumulator after convert.
     * Rs[0]=1 means retain (don't clear).
     *
     * The clear is DEFERRED to the packet boundary commit rather than
     * applied immediately.  This is critical for hardware loops where
     * the same PC appears across iterations -- immediate clear+flip
     * would cause the next iteration's matmul to use the wrong acc set.
     */
    int acc_clear = !(rs & 1);
    uint32_t cur_pc = env->gpr[HEX_REG_PC];

    switch (type) {
    case HMX_CVT_RS_UB:
    case HMX_CVT_RS_UB_SC0:
    case HMX_CVT_RS_UB_SC1:
    {
        /* Trigger full FXP convert: acc to cvt_fxp (8x8 byte) */
        hmx_fxp_convert(hmx, hmx->current_acc_set, relu, bias_sel,
                         fb_dst, fb_limit, cur_pc);
        if (acc_clear) {
            hmx->cvt_acc_clear_pending = 1;
            hmx->cvt_acc_clear_set = hmx->current_acc_set;
            hmx->cvt_acc_clear_pc = cur_pc;
        }
        return 0;
    }
    case HMX_CVT_RS_UH_2X1:
    {
        /* Trigger 16x8 (2x1) FXP convert */
        hmx_fxp_convert_2x1(hmx, hmx->current_acc_set, relu,
                              bias_sel, fb_dst, (rs >> 5) & 1, cur_pc);
        if (acc_clear) {
            hmx->cvt_acc_clear_pending = 1;
            hmx->cvt_acc_clear_set = hmx->current_acc_set;
            hmx->cvt_acc_clear_pc = cur_pc;
        }
        return 0;
    }
    case HMX_CVT_RS_UH_2X2:
    {
        /* Trigger 16x16 (2x2) FXP convert */
        int ch_sel = (rs >> 9) & 0x3;
        hmx_fxp_convert_2x2(hmx, hmx->current_acc_set, relu,
                              bias_sel, fb_dst, ch_sel, cur_pc);
        if (acc_clear) {
            hmx->cvt_acc_clear_pending = 1;
            hmx->cvt_acc_clear_set = hmx->current_acc_set;
            hmx->cvt_acc_clear_pc = cur_pc;
        }
        return 0;
    }
    case HMX_CVT_RS_HF:
        /* Trigger FP convert: acc to cvt_fp (FP16 mode) */
        hmx->cvt_fp[2] = hmx->cvt_fp[1];
        hmx->cvt_fp[1] = hmx->cvt_fp[0];
        hmx_fp_convert(env, hmx, hmx->current_acc_set, 0,
                        relu, bias_sel, maxnorm, /* fp8_odd_sel */ 0);
        if (acc_clear) {
            hmx->cvt_acc_clear_pending = 1;
            hmx->cvt_acc_clear_set = hmx->current_acc_set;
            hmx->cvt_acc_clear_pc = cur_pc;
        }
        return 0;
    case HMX_CVT_RS_F8:
        /* Trigger FP convert: acc to cvt_fp (F8 mode) */
        hmx->cvt_fp[2] = hmx->cvt_fp[1];
        hmx->cvt_fp[1] = hmx->cvt_fp[0];
        hmx_fp_convert(env, hmx, hmx->current_acc_set, 1,
                        relu, bias_sel, maxnorm, fp8_odd_sel);
        if (acc_clear) {
            hmx->cvt_acc_clear_pending = 1;
            hmx->cvt_acc_clear_set = hmx->current_acc_set;
            hmx->cvt_acc_clear_pc = cur_pc;
        }
        return 0;
    default:
        return 0;
    }
}

/*
 * Convert store
 *
 * Store convert state to VTCM memory.
 * Address uses same crouton format as activation (SM or CM).
 *
 * Rs = base address + spatial offset, Rt = spatial mask + dY
 */

/* Use spatial mask constants and increment from hmx_state.h */

/*
 * Convert raw spatial address to linear CVT buffer index (0-63).
 * CM: 6 spatial bits at [10:5]
 * SM: 6 spatial bits at [10:7] and [1:0]
 */
static inline int hmx_raw_to_linear(int32_t raw, int is_cm)
{
    if (is_cm) {
        return (raw >> 5) & 0x3F;
    }
    return ((((uint32_t)raw >> 7) << 2) | (raw & 3)) & 0x3F;
}

/*
 * Write one FXP spatial "peg" (32 output channels) to memory.
 * CM: channels at byte stride (32 bytes per peg)
 * SM: channels at 4-byte stride (interleaved with spatial)
 */
static void hmx_store_fxp_peg(CPUHexagonState *env, HmxCvtStateFxp *cvt,
                               int linear_s, uint32_t base_addr,
                               int32_t mem_spatial, int fmt, uintptr_t ra)
{
    uint32_t pa = base_addr + (mem_spatial & 0x7FF);
    int o;

    if (fmt == HMX_CVTST_CM) {
        /* CM: 32 channels at byte stride, pack 4 per word */
        for (o = 0; o < HMX_OUTPUT_CHANNELS; o += 4) {
            uint32_t w =
                (((cvt->data[linear_s][o + 0] >> 4) & 0xFF)) |
                (((cvt->data[linear_s][o + 1] >> 4) & 0xFF) << 8) |
                (((cvt->data[linear_s][o + 2] >> 4) & 0xFF) << 16) |
                (((cvt->data[linear_s][o + 3] >> 4) & 0xFF) << 24);
            cpu_stl_le_data_ra(env, pa + o, w, ra);
        }
    } else {
        /* SM/2x2: 32 channels at 4-byte stride */
        for (o = 0; o < HMX_OUTPUT_CHANNELS; o++) {
            uint8_t val = (cvt->data[linear_s][o] >> 4) & 0xFF;
            cpu_stb_data_ra(env, pa + (o << 2), val, ra);
        }
    }
}

/*
 * Write one x-row of the store.
 *
 * The CVT buffer index uses a rotated accumulator index (x_acc_idx,
 * y_acc_idx) rather than the memory position (x_idx, y_idx).  This
 * rotation accounts for the spatial split at x_offset.
 *
 * Memory address uses x_idx|y_idx for physical placement.
 * CVT index uses x_acc_idx|y_acc_idx for data lookup.
 *
 * Age selection is based on X position relative to x_offset:
 *   x < x_offset  -> cvt_fxp[before_state] (previous convert)
 *   x >= x_offset -> cvt_fxp[0] (current convert)
 *
 * This X-based age split applies to ALL Y rows, matching the
 * row-write helper.
 */
static void hmx_store_x_row(CPUHexagonState *env,
                              HmxCvtStateFxp *cvt_ages,
                              uint32_t base_addr, int32_t y_idx,
                              int32_t y_acc_idx, uint32_t x_offset,
                              uint32_t tile_x_inc, int32_t xm,
                              int is_cm, int fmt, int before_state,
                              int32_t x_acc_offset, uintptr_t ra)
{
    int32_t x_idx, x_acc_idx;
    int s;

    /* BEFORE: x < x_offset, reads from previous CVT age */
    x_idx = 0;
    x_acc_idx = x_acc_offset;
    for (; x_idx < (int32_t)x_offset; ) {
        s = hmx_raw_to_linear(x_acc_idx | y_acc_idx, is_cm);
        hmx_store_fxp_peg(env, &cvt_ages[before_state],
                          s, base_addr, x_idx | y_idx, fmt, ra);
        x_idx = hmx_inc_with_spatial_mask(x_idx, tile_x_inc, xm);
        x_acc_idx = hmx_inc_with_spatial_mask(x_acc_idx, tile_x_inc, xm);
        if (!tile_x_inc) {
            break;
        }
    }

    /* AFTER: x >= x_offset, reads from current CVT age */
    x_acc_idx = 0;
    while (x_idx >= 0) {
        s = hmx_raw_to_linear(x_acc_idx | y_acc_idx, is_cm);
        hmx_store_fxp_peg(env, &cvt_ages[0],
                          s, base_addr, x_idx | y_idx, fmt, ra);
        x_idx = hmx_inc_with_spatial_mask(x_idx, tile_x_inc, xm);
        x_acc_idx = hmx_inc_with_spatial_mask(x_acc_idx, tile_x_inc, xm);
        if (!tile_x_inc) {
            break;
        }
    }
}

void HELPER(hmx_cvt_store)(CPUHexagonState *env, uint32_t rs, uint32_t rt,
                            uint32_t params)
{
    HmxState *hmx = env->hmx_state;
    int fmt = HMX_UNPACK_CVTST_FMT(params);
    int age = HMX_UNPACK_CVTST_AGE(params);
    uintptr_t ra = GETPC();

    g_assert(age < HMX_NUM_CVT_AGES);

    uint32_t base_addr = rs & 0xFFFFF800;

    /*
     * Flush any pending CVT commit before reading the pipeline.
     * This ensures the store sees the correctly committed CVT state,
     * with proper aging (or lack thereof for same-packet fb=0+fb=2).
     */
    hmx_flush_cvt_fxp(hmx);

    /* Apply deferred acc clear from the convert packet */
    hmx_flush_acc_clear(hmx);

    if (fmt == HMX_CVTST_F8) {
        /*
         * FP8 store: each cvt_fp slot packs two FP8 results (see
         * HmxCvtStateFp comment): low byte = even spatial (crouton 2s),
         * high byte = odd spatial (crouton 2s+1).  The even/odd half
         * selection happens at convert time via fp8_odd_sel (Rs[11]);
         * the store unconditionally emits both halves to the two adjacent
         * crouton positions.  This mirrors the F8 store, which packs
         * the two adjacent spatial slots into one memory halfword
         * and has no store-time spatial select.
         */
        HmxCvtStateFp *cvt = &hmx->cvt_fp[age];
        int s, o;

        for (s = 0; s < HMX_SPATIAL_DIM_FP; s++) {
            for (o = 0; o < HMX_OUTPUT_CHANNELS; o++) {
                uint16_t packed = cvt->data[s][o];
                uint8_t f8_even = packed & 0xFF;
                uint8_t f8_odd = (packed >> 8) & 0xFF;
                int offset_even = hmx_act_offset_sm(s * 2, o);
                int offset_odd = hmx_act_offset_sm(s * 2 + 1, o);
                cpu_stb_data_ra(env, base_addr + offset_even, f8_even, ra);
                cpu_stb_data_ra(env, base_addr + offset_odd, f8_odd, ra);
            }
        }
        return;
    }

    /*
     * FXP store (CM, SM, 2x2) with before/after spatial split.
     *
     * Rt provides the spatial mask that splits the 6 spatial bits into
     * x (column) and y (row) directions:
     *   tile_y_mask = Rt & sp_mask_bits
     *   tile_x_mask = ~Rt & sp_mask_bits
     *
     * Rs provides the x_offset and y_offset within the tile:
     *   x_offset = Rs & tile_x_mask
     *   y_offset = Rs & tile_y_mask
     *
     * The store writes all 64 spatial positions, but splits them into
     * BEFORE (from older CVT state) and AFTER (from current CVT state)
     * relative to (y_offset, x_offset).
     *
     * CVT buffer index uses a rotated accumulator index (y_acc|x_acc)
     * rather than memory position, matching the row-write order.
     */
    int is_cm = (fmt == HMX_CVTST_CM);
    uint32_t sp_mask = is_cm ? HMX_SPATIAL_MASK_BITS_CM
                              : HMX_SPATIAL_MASK_BITS_SM;
    uint32_t tile_y_mask, tile_x_mask;
    uint32_t tile_x_inc, tile_y_inc;
    uint32_t x_offset, y_offset;
    int32_t xm, ym;
    int before_state;
    int32_t y_idx;

    tile_y_mask = rt & sp_mask;
    tile_x_mask = (~rt) & sp_mask;
    tile_x_inc = tile_x_mask
               ? (tile_x_mask & (-(int32_t)tile_x_mask)) : 0;
    tile_y_inc = tile_y_mask
               ? (tile_y_mask & (-(int32_t)tile_y_mask)) : 0;
    x_offset = rs & tile_x_mask;
    y_offset = rs & tile_y_mask;

    xm = (int32_t)(tile_x_mask | (1u << 31));
    ym = (int32_t)(tile_y_mask | (1u << 31));

    /* BEFORE reads from previous CVT state: age=0->[1], age=1->[2] */
    before_state = 1 + age;

    /*
     * Compute accumulator offset for the before/after rotation.
     * x_acc_offset counts how many X increments from x_offset to wrap.
     * Convert-memory parameters
     */
    int32_t x_acc_offset = 0;
    if (x_offset && tile_x_inc) {
        int32_t x_count = (int32_t)x_offset;
        while (x_count >= 0) {
            x_acc_offset = hmx_inc_with_spatial_mask(x_acc_offset,
                                           tile_x_inc, xm);
            x_count = hmx_inc_with_spatial_mask(x_count, tile_x_inc, xm);
        }
    }

    /*
     * First y pass: y from y_offset onward.
     * y_acc_idx starts at 0, providing the rotated CVT index.
     * Age selection is always based on X position vs x_offset.
     */
    y_idx = (int32_t)y_offset;
    int32_t y_acc_idx = 0;

    while (y_idx >= 0) {
        hmx_store_x_row(env, hmx->cvt_fxp, base_addr, y_idx,
                        y_acc_idx, x_offset, tile_x_inc, xm,
                        is_cm, fmt, before_state,
                        x_acc_offset, ra);
        y_idx = hmx_inc_with_spatial_mask(y_idx, tile_y_inc, ym);
        y_acc_idx = hmx_inc_with_spatial_mask(y_acc_idx, tile_y_inc, ym);
        if (!tile_y_inc) {
            break;
        }
    }

    /*
     * Second y pass: y from 0 to y_offset.
     * y_acc_idx continues from the first pass.
     * Address adjusted by dY if present.
     */
    if ((int32_t)y_offset > 0) {
        uint32_t dY = rt & 0xFFFFF800;

        if (dY) {
            base_addr += dY;
        }
        for (y_idx = 0; y_idx < (int32_t)y_offset; ) {
            hmx_store_x_row(env, hmx->cvt_fxp, base_addr,
                            y_idx, y_acc_idx, x_offset,
                            tile_x_inc, xm, is_cm, fmt,
                            before_state, x_acc_offset, ra);
            y_idx = hmx_inc_with_spatial_mask(y_idx, tile_y_inc, ym);
            y_acc_idx = hmx_inc_with_spatial_mask(y_acc_idx, tile_y_inc, ym);
            if (!tile_y_inc) {
                break;
            }
        }
    }
}
