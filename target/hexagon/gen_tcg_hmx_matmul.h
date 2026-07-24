/*
 * Hexagon HMX FXP Matmul - TCG Code Generation
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Translation-time functions that emit TCG IR for the HMX fixed-point
 * matrix multiply instruction.  All loops are emitted as TCG labels
 * and branches; the inner MAC uses GVec for host SIMD.
 */

#ifndef HEXAGON_GEN_TCG_HMX_MATMUL_H
#define HEXAGON_GEN_TCG_HMX_MATMUL_H

#include "tcg/tcg-op.h"
#include "tcg/tcg-op-gvec-common.h"
#include "hmx_state.h"

/*
 * Offset helpers.
 *
 * Scratch buffers live in CPUHexagonState at fixed env offsets (for GVec).
 * Shared HMX state is accessed via an env->hmx_state pointer load.
 */
#define HMX_STATE_PTR_OFS  offsetof(CPUHexagonState, hmx_state)

/* Fixed offsets for GVec scratch buffers (per-CPU, in env) */
#define HMX_WEI_RAW_OFS   offsetof(CPUHexagonState, hmx_wei_raw)
#define HMX_WEI_EXP_OFS   offsetof(CPUHexagonState, hmx_wei_expanded)
#define HMX_MAC_TMP_OFS    offsetof(CPUHexagonState, hmx_mac_tmp)

/* GVec operation size: 32 x int32 = 128 bytes */
#define HMX_GVEC_LEN  (HMX_OUTPUT_CHANNELS * sizeof(int32_t))

/*
 * Emit masked increment: out = masked_inc(in, inc, mask)
 *
 * out = (((in | ~mask) + (inc & mask)) & mask) | (in & ~mask)
 */
static inline void gen_hmx_masked_inc(TCGv out, TCGv in,
                                      TCGv inc, TCGv mask)
{
    TCGv not_mask = tcg_temp_new();
    TCGv tmp1 = tcg_temp_new();
    TCGv tmp2 = tcg_temp_new();

    tcg_gen_not_tl(not_mask, mask);        /* not_mask = ~mask           */
    tcg_gen_or_tl(tmp1, in, not_mask);     /* tmp1 = in | ~mask          */
    tcg_gen_and_tl(tmp2, inc, mask);       /* tmp2 = inc & mask          */
    tcg_gen_add_tl(tmp1, tmp1, tmp2);      /* tmp1 = tmp1 + tmp2         */
    tcg_gen_and_tl(tmp1, tmp1, mask);      /* tmp1 = tmp1 & mask         */
    tcg_gen_and_tl(tmp2, in, not_mask);    /* tmp2 = in & ~mask          */
    tcg_gen_or_tl(out, tmp1, tmp2);        /* out = tmp1 | tmp2          */
}

/*
 * Emit masked increment with overflow detection.
 * out = masked_inc(in, inc, mask); ovf = (out < in) ? 1 : 0
 */
static inline void gen_hmx_masked_inc_ovf(TCGv out, TCGv ovf,
                                          TCGv in, TCGv inc, TCGv mask)
{
    gen_hmx_masked_inc(out, in, inc, mask);
    tcg_gen_setcond_tl(TCG_COND_LT, ovf, out, in);
}

/*
 * Emit tap advance with dilate:
 *   out = masked_inc(in, inc, mask)
 *   if (inc == 0) out = -1
 *   if (dilate && out >= 0) out = masked_inc(out, inc, mask)
 *
 * x_dilate is a compile-time constant (from wei_mod).
 * y_dilate is a runtime TCGv (from hmx->y_dilate).
 */
static inline void gen_hmx_tap_advance(TCGv out, TCGv in,
                                       TCGv inc, TCGv mask,
                                       int x_dilate_const,
                                       TCGv y_dilate_rt)
{
    TCGv zero = tcg_constant_tl(0);
    TCGv neg_one = tcg_constant_tl(-1);

    gen_hmx_masked_inc(out, in, inc, mask);

    /* if inc == 0: out = -1 */
    tcg_gen_movcond_tl(TCG_COND_EQ, out, inc, zero, neg_one, out);

    /* Dilate: if dilate && out >= 0, double-step */
    if (x_dilate_const) {
        /* Compile-time dilate (x-tap) */
        TCGLabel *skip = gen_new_label();
        tcg_gen_brcondi_tl(TCG_COND_LT, out, 0, skip);
        gen_hmx_masked_inc(out, out, inc, mask);
        gen_set_label(skip);
    } else if (y_dilate_rt) {
        /* Runtime dilate (y-tap) */
        TCGLabel *skip = gen_new_label();
        tcg_gen_brcondi_tl(TCG_COND_EQ, y_dilate_rt, 0, skip);
        tcg_gen_brcondi_tl(TCG_COND_LT, out, 0, skip);
        gen_hmx_masked_inc(out, out, inc, mask);
        gen_set_label(skip);
    }
}

/*
 * Emit weight vector preload: load 32 x 4-byte words from guest memory
 * into env->hmx_wei_raw[].
 *
 * addr = wei_base + vec_idx * 128 + i * 4  for i in 0..31
 */
static inline void gen_hmx_load_weight_vec(DisasContext *ctx,
                                           TCGv wei_base, TCGv vec_idx)
{
    TCGv addr = tcg_temp_new();
    TCGv_i32 val = tcg_temp_new_i32();

    /* addr = wei_base + vec_idx * 128 */
    tcg_gen_shli_tl(addr, vec_idx, 7);
    tcg_gen_add_tl(addr, addr, wei_base);

    for (int i = 0; i < HMX_OUTPUT_CHANNELS; i++) {
        TCGv word_addr = tcg_temp_new();
        tcg_gen_addi_tl(word_addr, addr, i * 4);
        tcg_gen_qemu_ld_i32(val, word_addr, ctx->mem_idx,
                            MO_LE | MO_UL);
        tcg_gen_st_i32(val, tcg_env, HMX_WEI_RAW_OFS + i * 4);
    }
}

/*
 * Weight extraction via GVec.
 *
 * The wei_type is a compile-time constant per instruction variant.
 * sub_idx (which sub-element within the weight word) is runtime.
 *
 * All extraction reads from wei_raw and writes to wei_expanded.
 */

/*
 * Byte weights (cpv=4): shift right by sub_idx*8, mask 0xFF,
 * sign-extend from 8 bits.
 */
static inline void gen_hmx_extract_byte(TCGv_i32 sub_idx)
{
    TCGv_i32 shift = tcg_temp_new_i32();
    tcg_gen_shli_i32(shift, sub_idx, 3);   /* shift = sub_idx * 8 */

    tcg_gen_gvec_shrs(MO_32, HMX_WEI_EXP_OFS, HMX_WEI_RAW_OFS,
                      shift, HMX_GVEC_LEN, HMX_GVEC_LEN);
    tcg_gen_gvec_andi(MO_32, HMX_WEI_EXP_OFS, HMX_WEI_EXP_OFS,
                      0xFF, HMX_GVEC_LEN, HMX_GVEC_LEN);
    /* Sign-extend from 8 bits */
    tcg_gen_gvec_shli(MO_32, HMX_WEI_EXP_OFS, HMX_WEI_EXP_OFS,
                      24, HMX_GVEC_LEN, HMX_GVEC_LEN);
    tcg_gen_gvec_sari(MO_32, HMX_WEI_EXP_OFS, HMX_WEI_EXP_OFS,
                      24, HMX_GVEC_LEN, HMX_GVEC_LEN);
}

/*
 * Sign-magnitude byte weights (cpv=4): extract byte, decompose sign/mag.
 * val = byte & 0x7F; sign = byte >> 7; result = sign ? -val : val
 */
static inline void gen_hmx_extract_sm(TCGv_i32 sub_idx)
{
    TCGv_i32 shift = tcg_temp_new_i32();
    tcg_gen_shli_i32(shift, sub_idx, 3);   /* shift = sub_idx * 8 */

    /* Extract raw byte into wei_expanded */
    tcg_gen_gvec_shrs(MO_32, HMX_WEI_EXP_OFS, HMX_WEI_RAW_OFS,
                      shift, HMX_GVEC_LEN, HMX_GVEC_LEN);
    tcg_gen_gvec_andi(MO_32, HMX_WEI_EXP_OFS, HMX_WEI_EXP_OFS,
                      0xFF, HMX_GVEC_LEN, HMX_GVEC_LEN);

    /*
     * Sign-magnitude per reference: result = (((in >> 7) & 0x7F) ^ in).
     * Positive (bit7=0): result = in (magnitude unchanged).
     * Negative (bit7=1): result = 0x7F ^ in = -(magnitude+1).
     * Maps [0x80..0xFF] to [-1..-128], no negative zero.
     *
     * Implementation: sign-extend bit 7 to get mask (0 or -1),
     * mask &= 0x7F -> (0 or 0x7F), XOR with original value.
     */
    for (int o = 0; o < HMX_OUTPUT_CHANNELS; o++) {
        TCGv_i32 val = tcg_temp_new_i32();
        TCGv_i32 mask = tcg_temp_new_i32();

        tcg_gen_ld_i32(val, tcg_env, HMX_WEI_EXP_OFS + o * 4);
        /* Arithmetic shift right 7 within byte: get 0 or 0xFFFFFFFF */
        tcg_gen_shli_i32(mask, val, 24);
        tcg_gen_sari_i32(mask, mask, 31);
        tcg_gen_andi_i32(mask, mask, 0x7F);
        tcg_gen_xor_i32(val, val, mask);
        /* Sign-extend result from 8 bits to full i32 */
        tcg_gen_ext8s_i32(val, val);
        tcg_gen_st_i32(val, tcg_env, HMX_WEI_EXP_OFS + o * 4);
    }
}

/*
 * Nibble weights (cpv=8):
 * byte_in_word = sub_idx % 4, nibble_sel = sub_idx / 4
 * shift = byte_in_word * 8 + nibble_sel * 4
 * Extract 4-bit value, sign-extend from 4 bits.
 */
static inline void gen_hmx_extract_nibble(TCGv_i32 sub_idx)
{
    TCGv_i32 shift = tcg_temp_new_i32();
    TCGv_i32 byte_in_word = tcg_temp_new_i32();
    TCGv_i32 nibble_sel = tcg_temp_new_i32();

    tcg_gen_andi_i32(byte_in_word, sub_idx, 3);
    tcg_gen_shri_i32(nibble_sel, sub_idx, 2);
    /* shift = byte_in_word * 8 + nibble_sel * 4 */
    tcg_gen_shli_i32(shift, byte_in_word, 3);
    tcg_gen_shli_i32(nibble_sel, nibble_sel, 2);
    tcg_gen_add_i32(shift, shift, nibble_sel);

    tcg_gen_gvec_shrs(MO_32, HMX_WEI_EXP_OFS, HMX_WEI_RAW_OFS,
                      shift, HMX_GVEC_LEN, HMX_GVEC_LEN);
    tcg_gen_gvec_andi(MO_32, HMX_WEI_EXP_OFS, HMX_WEI_EXP_OFS,
                      0xF, HMX_GVEC_LEN, HMX_GVEC_LEN);
    /* Sign-extend from 4 bits */
    tcg_gen_gvec_shli(MO_32, HMX_WEI_EXP_OFS, HMX_WEI_EXP_OFS,
                      28, HMX_GVEC_LEN, HMX_GVEC_LEN);
    tcg_gen_gvec_sari(MO_32, HMX_WEI_EXP_OFS, HMX_WEI_EXP_OFS,
                      28, HMX_GVEC_LEN, HMX_GVEC_LEN);
}

/*
 * Crumb weights (cpv=16):
 * byte_in_word = sub_idx % 4, crumb_sel = sub_idx / 4
 * shift = byte_in_word * 8 + crumb_sel * 2
 * Extract 2-bit value, sign-extend from 2 bits: {0,1,2,3} -> {0,1,-2,-1}.
 */
static inline void gen_hmx_extract_crumb(TCGv_i32 sub_idx)
{
    TCGv_i32 shift = tcg_temp_new_i32();
    TCGv_i32 byte_in_word = tcg_temp_new_i32();
    TCGv_i32 crumb_sel = tcg_temp_new_i32();

    tcg_gen_andi_i32(byte_in_word, sub_idx, 3);
    tcg_gen_shri_i32(crumb_sel, sub_idx, 2);
    /* shift = byte_in_word * 8 + crumb_sel * 2 */
    tcg_gen_shli_i32(shift, byte_in_word, 3);
    tcg_gen_shli_i32(crumb_sel, crumb_sel, 1);
    tcg_gen_add_i32(shift, shift, crumb_sel);

    tcg_gen_gvec_shrs(MO_32, HMX_WEI_EXP_OFS, HMX_WEI_RAW_OFS,
                      shift, HMX_GVEC_LEN, HMX_GVEC_LEN);
    tcg_gen_gvec_andi(MO_32, HMX_WEI_EXP_OFS, HMX_WEI_EXP_OFS,
                      0x3, HMX_GVEC_LEN, HMX_GVEC_LEN);
    /* Sign-extend from 2 bits */
    tcg_gen_gvec_shli(MO_32, HMX_WEI_EXP_OFS, HMX_WEI_EXP_OFS,
                      30, HMX_GVEC_LEN, HMX_GVEC_LEN);
    tcg_gen_gvec_sari(MO_32, HMX_WEI_EXP_OFS, HMX_WEI_EXP_OFS,
                      30, HMX_GVEC_LEN, HMX_GVEC_LEN);
}

/*
 * Signed crumb weights (cpv=16):
 * {0,1,2,3} maps to {2,1,-2,-1}
 * Decomposition: mag = 2 - (crumb & 1), sign = crumb >> 1
 * result = sign ? -mag : mag
 *
 * Implemented element-wise since the lookup is hard to express in GVec.
 */
static inline void gen_hmx_extract_signed_crumb(TCGv_i32 sub_idx)
{
    /* First extract unsigned crumb into wei_expanded */
    gen_hmx_extract_crumb(sub_idx);

    /* Then apply signed crumb mapping element-wise */
    for (int o = 0; o < HMX_OUTPUT_CHANNELS; o++) {
        TCGv_i32 val = tcg_temp_new_i32();
        TCGv_i32 mag = tcg_temp_new_i32();
        TCGv_i32 neg_mag = tcg_temp_new_i32();
        TCGv_i32 sign = tcg_temp_new_i32();
        TCGv_i32 zero32 = tcg_constant_i32(0);

        tcg_gen_ld_i32(val, tcg_env, HMX_WEI_EXP_OFS + o * 4);
        /* mag = 2 - (val & 1) */
        tcg_gen_andi_i32(mag, val, 1);
        tcg_gen_subfi_i32(mag, 2, mag);
        /* sign = val >> 1 */
        tcg_gen_shri_i32(sign, val, 1);
        /* result = sign ? -mag : mag */
        tcg_gen_neg_i32(neg_mag, mag);
        tcg_gen_movcond_i32(TCG_COND_NE, val, sign, zero32,
                            neg_mag, mag);
        tcg_gen_st_i32(val, tcg_env, HMX_WEI_EXP_OFS + o * 4);
    }
}

/*
 * Unsigned bit weights (cpv=32):
 * byte_in_word = sub_idx % 4, bit_sel = sub_idx / 4
 * shift = byte_in_word * 8 + bit_sel
 * Extract 1-bit value, unsigned.
 */
static inline void gen_hmx_extract_bit(TCGv_i32 sub_idx)
{
    TCGv_i32 shift = tcg_temp_new_i32();
    TCGv_i32 byte_in_word = tcg_temp_new_i32();
    TCGv_i32 bit_sel = tcg_temp_new_i32();

    tcg_gen_andi_i32(byte_in_word, sub_idx, 3);
    tcg_gen_shri_i32(bit_sel, sub_idx, 2);
    /* shift = byte_in_word * 8 + bit_sel */
    tcg_gen_shli_i32(shift, byte_in_word, 3);
    tcg_gen_add_i32(shift, shift, bit_sel);

    tcg_gen_gvec_shrs(MO_32, HMX_WEI_EXP_OFS, HMX_WEI_RAW_OFS,
                      shift, HMX_GVEC_LEN, HMX_GVEC_LEN);
    tcg_gen_gvec_andi(MO_32, HMX_WEI_EXP_OFS, HMX_WEI_EXP_OFS,
                      0x1, HMX_GVEC_LEN, HMX_GVEC_LEN);
}

/*
 * Signed bit weights (cpv=32): {0,1} maps to {+1,-1}
 * result = 1 - 2 * bit
 */
static inline void gen_hmx_extract_signed_bit(TCGv_i32 sub_idx)
{
    gen_hmx_extract_bit(sub_idx);
    /* result = 1 - 2 * bit: shift left 1 to get 2*bit, then sub from 1 */
    tcg_gen_gvec_shli(MO_32, HMX_WEI_EXP_OFS, HMX_WEI_EXP_OFS,
                      1, HMX_GVEC_LEN, HMX_GVEC_LEN);
    /* negate: 0 - (2*bit) */
    tcg_gen_gvec_neg(MO_32, HMX_WEI_EXP_OFS, HMX_WEI_EXP_OFS,
                     HMX_GVEC_LEN, HMX_GVEC_LEN);
    /* add 1 */
    tcg_gen_gvec_addi(MO_32, HMX_WEI_EXP_OFS, HMX_WEI_EXP_OFS,
                      1, HMX_GVEC_LEN, HMX_GVEC_LEN);
}

/*
 * Dispatch weight extraction based on compile-time wei_type.
 */
static inline void gen_hmx_extract_weights(int wei_type, TCGv_i32 sub_idx)
{
    switch (wei_type) {
    case HMX_WEI_B:
        gen_hmx_extract_byte(sub_idx);
        break;
    case HMX_WEI_SM:
        gen_hmx_extract_sm(sub_idx);
        break;
    case HMX_WEI_N:
    case HMX_WEI_N_2X:
        gen_hmx_extract_nibble(sub_idx);
        break;
    case HMX_WEI_C:
        gen_hmx_extract_crumb(sub_idx);
        break;
    case HMX_WEI_SC:
        gen_hmx_extract_signed_crumb(sub_idx);
        break;
    case HMX_WEI_B1:
        gen_hmx_extract_bit(sub_idx);
        break;
    case HMX_WEI_SB1:
        gen_hmx_extract_signed_bit(sub_idx);
        break;
    }
}

/*
 * Emit GVec MAC: acc_row[o] += scalar * wei_expanded[o] for o in 0..31
 *
 * acc_row_ptr is a runtime TCGv_ptr pointing to the accumulator row.
 * scalar_i64 is a TCGv_i64 holding (int32_t)(act_val * negate).
 *
 * Step 1: tcg_gen_gvec_muls into mac_tmp (fixed offset, reuses wei_raw)
 * Step 2: tcg_gen_gvec_add_var from mac_tmp into accumulator row
 */
static inline void gen_hmx_mac32(TCGv_ptr acc_row_ptr, TCGv_i64 scalar_i64)
{
    /* scalar * wei_expanded to mac_tmp */
    tcg_gen_gvec_muls(MO_32, HMX_MAC_TMP_OFS, HMX_WEI_EXP_OFS,
                      scalar_i64, HMX_GVEC_LEN, HMX_GVEC_LEN);

    /* acc_row += mac_tmp */
    tcg_gen_gvec_add_var(MO_32,
                         acc_row_ptr, 0,      /* dst: accumulator row */
                         acc_row_ptr, 0,      /* src1: accumulator row */
                         tcg_env, HMX_MAC_TMP_OFS,  /* src2: mac_tmp */
                         HMX_GVEC_LEN, HMX_GVEC_LEN);
}

/*
 * Compute a runtime pointer to the accumulator row:
 *   &hmx->acc[acc_sel].fxp_primary.data[spatial][0]
 *
 * hmx_ptr is a TCGv_ptr holding the loaded env->hmx_state pointer.
 * offset = offsetof(HmxState, acc)
 *        + acc_sel * sizeof(HmxAccSet)
 *        + offsetof(HmxAccSet, fxp_primary)
 *        + spatial * HMX_OUTPUT_CHANNELS * 4
 */
static inline void gen_hmx_acc_row_ptr(TCGv_ptr out, TCGv_ptr hmx_ptr,
                                       TCGv acc_sel, TCGv spatial)
{
    TCGv ofs = tcg_temp_new();
    TCGv tmp = tcg_temp_new();

    /* ofs = acc_sel * sizeof(HmxAccSet) */
    tcg_gen_muli_tl(ofs, acc_sel, sizeof(HmxAccSet));

    /* ofs += spatial * (HMX_OUTPUT_CHANNELS * 4) */
    tcg_gen_muli_tl(tmp, spatial, HMX_OUTPUT_CHANNELS * 4);
    tcg_gen_add_tl(ofs, ofs, tmp);

    /* ofs += base offset of acc[0].fxp_primary.data */
    tcg_gen_addi_tl(ofs, ofs,
                    offsetof(HmxState, acc)
                    + offsetof(HmxAccSet, fxp_primary));

    /* out = hmx_ptr + ofs */
    tcg_gen_ext_i32_ptr(out, ofs);
    tcg_gen_add_ptr(out, out, hmx_ptr);
}

/*
 * Main entry point: emit TCG IR for the complete FXP matmul.
 *
 * This is called at translation time from gen_tcg_hmx.h.
 * wei_type and wei_mod are compile-time constants from the instruction.
 */
static inline void gen_hmx_matmul_fxp(DisasContext *ctx,
                                      TCGv rs, TCGv rt,
                                      int wei_type, int wei_mod)
{
    /*
     * The activation instruction normally precedes the weight instruction
     * in the same packet, making its encoding available at translation time.
     * If not (illegal unpaired weight), fall back to the C helper which
     * reads state from HmxState at runtime — matching expected behavior
     * of silently proceeding with stale/default state.
     */
    if (!ctx->hmx_pkt.act_valid) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "HMX: weight insn at PC 0x%" VADDR_PRIx
                      " without paired activation\n",
                      ctx->base.pc_next);
        gen_helper_hmx_matmul_fxp(tcg_env, rs, rt,
            tcg_constant_i32(HMX_PACK_WEI(wei_type, wei_mod)));
        return;
    }

    int imm_format_offset = ctx->hmx_pkt.format_offset;
    int imm_format_mask = (1 << imm_format_offset) - 1;
    int imm_y_dilate = (ctx->hmx_pkt.act_mod == HMX_ACT_BLK_D) ? 1 : 0;
    bool imm_blocks_always_one =
        (ctx->hmx_pkt.act_mod != HMX_ACT_NOBLK);

    /*
     * Load the shared HMX state pointer from env once for all accesses.
     */
    TCGv_ptr hmx_ptr = tcg_temp_new_ptr();
    tcg_gen_ld_ptr(hmx_ptr, tcg_env, HMX_STATE_PTR_OFS);

    TCGLabel *tcg_inline_path = gen_new_label();
    TCGLabel *done = gen_new_label();

    /*
     * Fall back to C helper for multi-crouton (blocks > 1) or group
     * convolution (group_count > 1).  Group conv requires per-group
     * weight reuse and output channel partitioning that the TCG inline
     * path doesn't handle.
     */
    if (imm_blocks_always_one) {
        /* blocks guaranteed == 1, only check group_count */
        TCGv group_count = tcg_temp_new();
        tcg_gen_ld_tl(group_count, hmx_ptr,
                      offsetof(HmxState, group_count));
        tcg_gen_brcondi_tl(TCG_COND_LE, group_count, 1,
                           tcg_inline_path);
        /* Fall through to C helper */
        gen_helper_hmx_matmul_fxp(tcg_env, rs, rt,
            tcg_constant_i32(HMX_PACK_WEI(wei_type, wei_mod)));
        tcg_gen_br(done);
    } else {
        /* NOBLK: blocks may be > 1, keep both runtime checks */
        TCGv blocks = tcg_temp_new();
        tcg_gen_ld_tl(blocks, hmx_ptr, offsetof(HmxState, blocks));
        TCGv group_count = tcg_temp_new();
        tcg_gen_ld_tl(group_count, hmx_ptr,
                      offsetof(HmxState, group_count));
        TCGLabel *use_chelper = gen_new_label();
        tcg_gen_brcondi_tl(TCG_COND_GT, blocks, 1, use_chelper);
        tcg_gen_brcondi_tl(TCG_COND_LE, group_count, 1,
                           tcg_inline_path);
        gen_set_label(use_chelper);
        gen_helper_hmx_matmul_fxp(tcg_env, rs, rt,
            tcg_constant_i32(HMX_PACK_WEI(wei_type, wei_mod)));
        tcg_gen_br(done);
    }

    gen_set_label(tcg_inline_path);

    static const int cpv_table[8] = { 4, 4, 8, 16, 16, 32, 32, 8 };
    int cpv = cpv_table[wei_type];

    /* Compile-time flags from wei_mod */
    int x_dilate_const = (wei_mod == HMX_MOD_DI);
    int drop_const = (wei_mod == HMX_MOD_DR);
    int deep_const = (wei_mod == HMX_MOD_DP);

    /* Weight address: Rs[31:7] is 128B-aligned */
    TCGv wei_base = tcg_temp_new();
    tcg_gen_andi_tl(wei_base, rs, 0xFFFFFF80);

    /*
     * Weight address range from Rt.  max_valid_vec = Rt >> 7 gives the
     * highest valid weight vector index.  Vectors beyond this range are
     * treated as zero when marked invalid.
     */
    TCGv max_valid_vec = tcg_temp_new();
    tcg_gen_shri_tl(max_valid_vec, rt, 7);

    /*
     * Weight negate: Rs[5] only applies to FP matmul, not FXP.
     * wgt_negate = 0 when !is_flt.
     */
    TCGv negate = tcg_constant_tl(1);

    /* Load runtime state from shared HMX state into TCGv temps */
    TCGv tile_x_mask = tcg_temp_new();
    TCGv tile_y_mask = tcg_temp_new();
    TCGv tile_x_inc = tcg_temp_new();
    TCGv tile_y_inc = tcg_temp_new();
    TCGv fx = tcg_temp_new();
    TCGv ch_start = tcg_temp_new();
    TCGv ch_stop = tcg_temp_new();
    TCGv y_start = tcg_temp_new();
    TCGv y_stop = tcg_temp_new();
    TCGv current_acc = tcg_temp_new();

    tcg_gen_ld_tl(tile_x_mask, hmx_ptr,
                  offsetof(HmxState, tile_x_mask));
    tcg_gen_ld_tl(tile_y_mask, hmx_ptr,
                  offsetof(HmxState, tile_y_mask));
    tcg_gen_ld_tl(tile_x_inc, hmx_ptr,
                  offsetof(HmxState, tile_x_inc));
    tcg_gen_ld_tl(tile_y_inc, hmx_ptr,
                  offsetof(HmxState, tile_y_inc));
    tcg_gen_ld_tl(fx, hmx_ptr, offsetof(HmxState, fx));
    tcg_gen_ld_tl(ch_start, hmx_ptr, offsetof(HmxState, ch_start));
    tcg_gen_ld_tl(ch_stop, hmx_ptr, offsetof(HmxState, ch_stop));

    /*
     * Sub-byte weight types iterate over all 32 input channels regardless
     * of the activation instruction's channel range (reference:
     * weight parameter handling).
     */
    if (cpv > 4) {
        tcg_gen_movi_tl(ch_start, 0);
        tcg_gen_movi_tl(ch_stop, 32);
    }

    tcg_gen_ld_tl(y_start, hmx_ptr, offsetof(HmxState, y_start));
    tcg_gen_ld_tl(y_stop, hmx_ptr, offsetof(HmxState, y_stop));
    tcg_gen_ld_tl(current_acc, hmx_ptr,
                  offsetof(HmxState, current_acc_set));

    /* Compute masks with MSB for termination */
    TCGv tile_x_mask_msb = tcg_temp_new();
    TCGv tile_y_mask_msb = tcg_temp_new();
    tcg_gen_ori_tl(tile_x_mask_msb, tile_x_mask, (int32_t)(1u << 31));
    tcg_gen_ori_tl(tile_y_mask_msb, tile_y_mask, (int32_t)(1u << 31));

    /* format_mask = (1 << format_offset) - 1 (translation-time constant) */
    TCGv format_mask = tcg_constant_tl(imm_format_mask);

    /*
     * X-dimension tap parameters.
     * x_start, x_stop depend on wei_mod (compile-time) and fx (runtime).
     */
    TCGv x_start = tcg_temp_new();
    TCGv x_stop = tcg_temp_new();

    switch (wei_mod) {
    case HMX_MOD_NORMAL:
        tcg_gen_movi_tl(x_start, 0);
        tcg_gen_mov_tl(x_stop, fx);
        break;
    case HMX_MOD_SINGLE:
        tcg_gen_mov_tl(x_start, fx);
        tcg_gen_mov_tl(x_stop, fx);
        break;
    case HMX_MOD_DR:
        tcg_gen_movi_tl(x_start, 0);
        tcg_gen_mov_tl(x_stop, fx);
        break;
    case HMX_MOD_DP:
        tcg_gen_movi_tl(x_start, 0);
        tcg_gen_movi_tl(x_stop, 0);
        break;
    case HMX_MOD_ABOVE:
        tcg_gen_mov_tl(x_start, fx);
        tcg_gen_mov_tl(x_stop, tile_x_mask);
        break;
    case HMX_MOD_DI:
        tcg_gen_movi_tl(x_start, 0);
        tcg_gen_mov_tl(x_stop, fx);
        break;
    }

    /* Weight stream index counter */
    TCGv wgt_stream_idx = tcg_temp_new();
    tcg_gen_movi_tl(wgt_stream_idx, 0);

    /* Previous vector index for preload optimization */
    TCGv prev_vec_idx = tcg_temp_new();
    tcg_gen_movi_tl(prev_vec_idx, -1);

    /* Loop labels */
    TCGLabel *loop_ytap = gen_new_label();
    TCGLabel *end_ytap = gen_new_label();
    TCGLabel *loop_xtap = gen_new_label();
    TCGLabel *end_xtap = gen_new_label();
    TCGLabel *loop_ch = gen_new_label();
    TCGLabel *end_ch = gen_new_label();
    TCGLabel *loop_iy = gen_new_label();
    TCGLabel *end_iy = gen_new_label();
    TCGLabel *loop_ix = gen_new_label();
    TCGLabel *end_ix = gen_new_label();

    /* Y-tap temps */
    TCGv y_tap = tcg_temp_new();
    TCGv y_tap_next = tcg_temp_new();

    /* X-tap temps */
    TCGv x_tap = tcg_temp_new();
    TCGv x_tap_next = tcg_temp_new();

    /* Channel loop temp */
    TCGv ch = tcg_temp_new();
    TCGv ch_addr = tcg_temp_new();

    /* Weight indexing temps */
    TCGv vec_idx = tcg_temp_new();
    TCGv_i32 sub_idx = tcg_temp_new_i32();

    /* Spatial loop temps */
    TCGv intra_y = tcg_temp_new();
    TCGv intra_x = tcg_temp_new();
    TCGv act_y = tcg_temp_new();
    TCGv y_ovf = tcg_temp_new();
    TCGv output_idx = tcg_temp_new();
    TCGv x_ovf = tcg_temp_new();
    TCGv acc_sel = tcg_temp_new();
    TCGv spatial = tcg_temp_new();
    TCGv act_idx = tcg_temp_new();
    TCGv act_val = tcg_temp_new();
    TCGv act_x_neg = tcg_temp_new();

    TCGv zero = tcg_constant_tl(0);
    TCGv neg_one = tcg_constant_tl(-1);

    /* Deep mode block counter (0=first block, 1=second block done) */
    TCGv deep_blk = tcg_temp_new();
    if (deep_const) {
        tcg_gen_movi_tl(deep_blk, 0);
    }

    /* ========== Y-TAP LOOP ========== */
    tcg_gen_mov_tl(y_tap, y_start);

    gen_set_label(loop_ytap);
    /* Terminate if y_tap < 0 */
    tcg_gen_brcondi_tl(TCG_COND_LT, y_tap, 0, end_ytap);

    /* ========== X-TAP LOOP ========== */
    tcg_gen_mov_tl(x_tap, x_start);

    gen_set_label(loop_xtap);
    /* Terminate if x_tap < 0 */
    tcg_gen_brcondi_tl(TCG_COND_LT, x_tap, 0, end_xtap);

    /* ========== CHANNEL LOOP ========== */
    tcg_gen_mov_tl(ch, ch_start);

    gen_set_label(loop_ch);
    tcg_gen_brcond_tl(TCG_COND_GE, ch, ch_stop, end_ch);

    /*
     * Skip multiply when weight vector is beyond valid range.
     * Checks wgt_cache[idx][0].valid and
     * skips the MAC entirely when invalid.  This matters for sub-byte
     * types like scrumb where zero raw data maps to non-zero weights.
     */
    {
        int log2_cpv = ctz32(cpv);
        tcg_gen_shri_tl(vec_idx, wgt_stream_idx, log2_cpv);
    }
    TCGLabel *skip_wgt_invalid = gen_new_label();
    tcg_gen_brcond_tl(TCG_COND_GT, vec_idx, max_valid_vec,
                       skip_wgt_invalid);

    /* ch_addr = ch << format_offset (translation-time constant shift) */
    tcg_gen_shli_tl(ch_addr, ch, imm_format_offset);

    /* --- Weight preload (conditional on vec_idx change) --- */
    {
        TCGLabel *skip_preload = gen_new_label();
        tcg_gen_brcond_tl(TCG_COND_EQ, vec_idx, prev_vec_idx, skip_preload);

        gen_hmx_load_weight_vec(ctx, wei_base, vec_idx);
        tcg_gen_mov_tl(prev_vec_idx, vec_idx);

        gen_set_label(skip_preload);
    }

    /* --- Weight extraction --- */
    {
        /* sub_idx = wgt_stream_idx % cpv (TCGv == TCGv_i32 on hexagon) */
        tcg_gen_andi_tl(sub_idx, wgt_stream_idx, cpv - 1);
    }
    gen_hmx_extract_weights(wei_type, sub_idx);

    /* ========== SPATIAL Y LOOP ========== */
    tcg_gen_movi_tl(intra_y, 0);

    gen_set_label(loop_iy);

    /* act_y = masked_inc_ovf(y_tap, intra_y, tile_y_mask, &y_ovf) */
    gen_hmx_masked_inc_ovf(act_y, y_ovf, y_tap, intra_y, tile_y_mask);
    /* act_y = (act_y & 0x7FFFFFFF) + y_ovf * 0x800 */
    {
        TCGv tmp = tcg_temp_new();
        tcg_gen_andi_tl(act_y, act_y, 0x7FFFFFFF);
        tcg_gen_shli_tl(tmp, y_ovf, 11);   /* y_ovf * 0x800 */
        tcg_gen_add_tl(act_y, act_y, tmp);
    }

    /* ========== SPATIAL X LOOP ========== */
    tcg_gen_movi_tl(intra_x, 0);

    gen_set_label(loop_ix);

    /* output_idx = masked_inc_ovf(x_tap, intra_x, tile_x_mask, &x_ovf) */
    gen_hmx_masked_inc_ovf(output_idx, x_ovf, x_tap, intra_x, tile_x_mask);

    /* acc_sel = x_ovf ? (current_acc ^ 1) : current_acc */
    tcg_gen_xor_tl(acc_sel, current_acc, x_ovf);
    tcg_gen_andi_tl(acc_sel, acc_sel, 1);

    /* Combine x and y into spatial address */
    tcg_gen_or_tl(output_idx, output_idx, intra_y);

    /*
     * spatial = ((output_idx >> 5) & ~format_mask)
     *         | (output_idx & format_mask)
     */
    {
        TCGv hi = tcg_temp_new();
        TCGv lo = tcg_temp_new();
        TCGv not_fmask = tcg_temp_new();
        tcg_gen_shri_tl(hi, output_idx, 5);
        tcg_gen_not_tl(not_fmask, format_mask);
        tcg_gen_and_tl(hi, hi, not_fmask);
        tcg_gen_and_tl(lo, output_idx, format_mask);
        tcg_gen_or_tl(spatial, hi, lo);
    }

    {
        TCGLabel *skip_mac = gen_new_label();

        /* Drop overflow in drop/deep mode */
        if (drop_const || deep_const) {
            tcg_gen_brcondi_tl(TCG_COND_NE, x_ovf, 0, skip_mac);
        }

        /* Bounds check: spatial >= 0 && spatial < 64 */
        tcg_gen_brcondi_tl(TCG_COND_LT, spatial, 0, skip_mac);
        tcg_gen_brcondi_tl(TCG_COND_GE, spatial, HMX_SPATIAL_DIM_FXP,
                           skip_mac);

        /* Activation lookup: act_idx = (act_y + intra_x + ch_addr) & 0xFFF */
        tcg_gen_add_tl(act_idx, act_y, intra_x);
        tcg_gen_add_tl(act_idx, act_idx, ch_addr);
        tcg_gen_andi_tl(act_idx, act_idx, 0xFFF);

        /* act_val = hmx->act_buffer[act_idx] (load byte via hmx_ptr) */
        {
            TCGv_ptr act_ptr = tcg_temp_new_ptr();
            tcg_gen_ext_i32_ptr(act_ptr, act_idx);
            tcg_gen_addi_ptr(act_ptr, act_ptr,
                             offsetof(HmxState, act_buffer));
            tcg_gen_add_ptr(act_ptr, act_ptr, hmx_ptr);
            tcg_gen_ld8u_i32(act_val, act_ptr, 0);
        }

        /* act_x_neg = (int32_t)act_val * negate */
        tcg_gen_mul_tl(act_x_neg, act_val, negate);

        /* GVec MAC */
        {
            TCGv_ptr acc_row_ptr = tcg_temp_new_ptr();
            TCGv_i64 scalar_i64 = tcg_temp_new_i64();

            gen_hmx_acc_row_ptr(acc_row_ptr, hmx_ptr, acc_sel, spatial);
            tcg_gen_ext_i32_i64(scalar_i64, act_x_neg);
            gen_hmx_mac32(acc_row_ptr, scalar_i64);
        }

        gen_set_label(skip_mac);
    }

    /* --- Advance spatial X --- */
    gen_hmx_masked_inc(intra_x, intra_x, tile_x_inc, tile_x_mask_msb);
    /* if inc == 0, force terminate */
    tcg_gen_movcond_tl(TCG_COND_EQ, intra_x, tile_x_inc, zero,
                       neg_one, intra_x);
    tcg_gen_brcondi_tl(TCG_COND_GE, intra_x, 0, loop_ix);
    gen_set_label(end_ix);

    /* --- Advance spatial Y --- */
    gen_hmx_masked_inc(intra_y, intra_y, tile_y_inc, tile_y_mask_msb);
    tcg_gen_movcond_tl(TCG_COND_EQ, intra_y, tile_y_inc, zero,
                       neg_one, intra_y);
    tcg_gen_brcondi_tl(TCG_COND_GE, intra_y, 0, loop_iy);
    gen_set_label(end_iy);

    /* --- Skip target for invalid weight vectors --- */
    gen_set_label(skip_wgt_invalid);

    /* --- Advance weight stream index and channel --- */
    tcg_gen_addi_tl(wgt_stream_idx, wgt_stream_idx, 1);
    tcg_gen_addi_tl(ch, ch, 1);
    tcg_gen_br(loop_ch);
    gen_set_label(end_ch);

    /* --- Advance X tap --- */
    gen_hmx_tap_advance(x_tap_next, x_tap, tile_x_inc, tile_x_mask_msb,
                        x_dilate_const, NULL);
    tcg_gen_brcond_tl(TCG_COND_GT, x_tap_next, x_stop, end_xtap);
    tcg_gen_brcondi_tl(TCG_COND_LT, x_tap_next, 0, end_xtap);
    tcg_gen_mov_tl(x_tap, x_tap_next);
    tcg_gen_br(loop_xtap);
    gen_set_label(end_xtap);

    /*
     * Deep mode: process a second block of weights with the secondary
     * accumulator.  Loops over
     * wgt_deep_idx (0..num_wgt_deep_blk-1) and flips primary_acc
     * after each block.
     *
     * Since deep_const is known at translation time, we conditionally
     * emit a one-shot repeat of the x_tap loop with flipped current_acc.
     * deep_blk is a TCG temp initialized to 0 before the y_tap loop.
     */
    if (deep_const) {
        TCGLabel *skip_deep_repeat = gen_new_label();

        /* After x_tap completes: check if second block is done */
        tcg_gen_brcondi_tl(TCG_COND_NE, deep_blk, 0, skip_deep_repeat);
        /* First block done: set flag, flip acc, restart x_tap loop */
        tcg_gen_movi_tl(deep_blk, 1);
        tcg_gen_xori_tl(current_acc, current_acc, 1);
        tcg_gen_mov_tl(x_tap, x_start);
        tcg_gen_br(loop_xtap);

        gen_set_label(skip_deep_repeat);
        /* Second block done: flip acc back, reset flag for next y_tap */
        tcg_gen_xori_tl(current_acc, current_acc, 1);
        tcg_gen_movi_tl(deep_blk, 0);
    }

    /* --- Advance Y tap --- */
    gen_hmx_tap_advance(y_tap_next, y_tap, tile_y_inc, tile_y_mask_msb,
                        imm_y_dilate, NULL);
    tcg_gen_brcond_tl(TCG_COND_GT, y_tap_next, y_stop, end_ytap);
    tcg_gen_brcondi_tl(TCG_COND_LT, y_tap_next, 0, end_ytap);
    tcg_gen_mov_tl(y_tap, y_tap_next);
    tcg_gen_br(loop_ytap);
    gen_set_label(end_ytap);

    gen_set_label(done);
}

#endif /* HEXAGON_GEN_TCG_HMX_MATMUL_H */
