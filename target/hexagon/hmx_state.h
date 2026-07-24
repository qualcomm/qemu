/*
 * Hexagon HMX (Matrix eXtensions) State Definitions
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * This file defines the HMX state structures for pure TCG implementation.
 */

#ifndef HEXAGON_HMX_STATE_H
#define HEXAGON_HMX_STATE_H

#include <stdint.h>

/*
 * HMX Architecture Constants
 */
#define HMX_SPATIAL_DIM_FXP     64
#define HMX_SPATIAL_DIM_FP      32
#define HMX_OUTPUT_CHANNELS     32
#define HMX_INPUT_CHANNELS      32
#define HMX_NUM_ACC_SETS        2
#define HMX_NUM_CVT_AGES        3
#define HMX_NUM_BIAS_SETS       4

#define HMX_ACT_CROUTON_SIZE    2048  /* 2KB crouton */
/* 4KB: 2 croutons */
#define HMX_ACT_CACHE_SIZE      (2 * HMX_ACT_CROUTON_SIZE)

typedef struct HmxAccFxp {
    int32_t data[HMX_SPATIAL_DIM_FXP][HMX_OUTPUT_CHANNELS];
} HmxAccFxp;

typedef struct HmxAccFp {
    uint64_t data[HMX_SPATIAL_DIM_FP][HMX_OUTPUT_CHANNELS];
} HmxAccFp;

typedef struct HmxAccSet {
    HmxAccFxp fxp_primary;
    HmxAccFxp fxp_secondary;
    HmxAccFp  fp_primary;
    HmxAccFp  fp_secondary;
} HmxAccSet;

typedef struct HmxCvtStateFxp {
    uint16_t data[HMX_SPATIAL_DIM_FXP][HMX_OUTPUT_CHANNELS];
} HmxCvtStateFxp;

/*
 * FP convert state.
 *
 * Each 16-bit slot's meaning depends on the convert mode that last wrote it:
 *
 *   FP16/HF convert:  data[s][o] holds one full 16-bit FP16 value.
 *
 *   FP8 convert:      data[s][o] packs the two FP8 results for the even and
 *                     odd spatial halves of crouton-pair s:
 *                       bits [7:0]  = FP8 byte for even spatial (crouton 2s)
 *                       bits [15:8] = FP8 byte for odd  spatial (crouton 2s+1)
 *                     The FP8 convert RMW-writes only the half selected by
 *                     fp8_odd_sel (Rs[11]) and preserves the other half, so
 *                     two converts (even then odd) populate both bytes.  The
 *                     F8 store then emits the low byte to crouton 2s and the
 *                     high byte to crouton 2s+1.  Unlike FP16, FP8 stores no
 *                     <<4-shifted 12-bit value here; each half is a raw FP8
 *                     byte (packs the same two halves into one memory
 *                     halfword).
 */
typedef struct HmxCvtStateFp {
    uint16_t data[HMX_SPATIAL_DIM_FP][HMX_OUTPUT_CHANNELS];
} HmxCvtStateFp;

/*
 * Complete HMX state structure
 */
typedef struct HmxState {
    /* Accumulators: 2 sets for swap operation */
    HmxAccSet acc[HMX_NUM_ACC_SETS];

    /* Convert state: 3 ages for feedback operations */
    HmxCvtStateFxp cvt_fxp[HMX_NUM_CVT_AGES];
    HmxCvtStateFp  cvt_fp[HMX_NUM_CVT_AGES];

    /* Working buffer for current convert (feedback source) */
    HmxCvtStateFxp cvt_future_fxp;

    /* Bias registers: raw 64-bit values as loaded from memory */
    uint64_t bias_raw[HMX_NUM_BIAS_SETS][HMX_OUTPUT_CHANNELS];

    /* Current accumulator set index (for swap) */
    uint32_t current_acc_set;

    /* Activation cache (up to 2 croutons = 4KB for multi-tap) */
    uint8_t act_buffer[HMX_ACT_CACHE_SIZE];

    /*
     * Latched activation parameters (set by activation instruction,
     * used by subsequent weight multiply instruction)
     */
    uint32_t act_rs;        /* Latched Rs from activation instruction */
    uint32_t act_rt;        /* Latched Rt from activation instruction */
    uint32_t act_format;    /* 0=spatial major, 1=channel major */
    uint32_t act_type;      /* HMX_ACT_UB, HMX_ACT_HF, HMX_ACT_F8 */
    uint8_t  is_f8_odd;     /* Rs[0]: FP8 activation reads odd byte of pair */
    uint8_t  fp8_odd_sel;   /* cvt Rs[11]: FP8 convert writes odd (1) half */

    /*
     * Multi-tap convolution state (computed by activation load,
     * used by weight multiply)
     */
    uint32_t format_offset;     /* SM=2, DM=0 */
    uint32_t tile_x_mask;
    uint32_t tile_y_mask;
    uint32_t tile_x_inc;
    uint32_t tile_y_inc;
    uint32_t fx;                /* filter x position from Rs */
    uint32_t fy;                /* filter y position from Rs */
    int32_t  ch_start;          /* pre-computed channel start (raw) */
    int32_t  ch_stop;           /* pre-computed channel stop (exclusive) */
    uint32_t y_start;           /* y tap start position */
    uint32_t y_stop;            /* y tap stop position */
    uint32_t y_dilate;          /* y dimension dilation flag */
    uint32_t blocks;            /* number of activation blocks (deep mode) */
    int32_t  dY;                /* offset to second activation crouton */
    uint32_t group_conv;        /* group conv flag (ch_start > ch_stop) */
    uint32_t group_size;        /* channels per group (32 if no group conv) */
    uint32_t group_count;       /* number of groups (1 if no group conv) */

    /*
     * Deferred CVT pipeline commit state.
     *
     * Defers pipeline aging to commit_regs time.
     * When fb=0 and fb=2 run in the same packet, fb=2 sets cvt_advance=1
     * which overrides fb=0's cvt_advance=0, suppressing pipeline aging.
     * We emulate this by deferring the age+copy until the next consumer
     * (store or next fb=0 convert) needs the committed state.
     */
    uint32_t cvt_fxp_pending;      /* 1 = future has uncommitted data */
    uint32_t cvt_fxp_pending_age;  /* 1 = pipeline should age at commit */
    uint32_t cvt_fxp_pending_pc;   /* PC of the fb=0 pass */

    /*
     * Deferred accumulator clear/flip.
     * In the reference, acc_clear runs in commit_mem (AFTER all
     * instructions execute).  When two CVT instructions share a
     * packet, both must read the same accumulator.  We defer the
     * clear+flip to the start of the next packet's first HMX op.
     */
    uint32_t cvt_acc_clear_pending;
    uint32_t cvt_acc_clear_set;    /* Which acc set to clear */
    uint32_t cvt_acc_clear_pc;     /* PC of the packet that requested it */
} HmxState;

/*
 * Spatial mask constants and helper functions for multi-tap convolution
 */
#define HMX_SPATIAL_MASK_BITS_CM  0x7E0
#define HMX_SPATIAL_MASK_BITS_SM  0x783
#define HMX_MAX_TAP_ARRAY        64

static inline uint32_t hmx_get_spatial_mask(uint32_t in, uint32_t format_offset)
{
    return in & (format_offset ? HMX_SPATIAL_MASK_BITS_SM
                               : HMX_SPATIAL_MASK_BITS_CM);
}

static inline uint32_t hmx_get_masked_inc(uint32_t mask)
{
    return mask & (~mask + 1);  /* lowest set bit */
}

static inline int32_t hmx_inc_with_spatial_mask(int32_t in, int32_t inc,
                                                 int32_t mask)
{
    return (((in | ~mask) + (inc & mask)) & mask) | (in & ~mask);
}

static inline int32_t hmx_inc_with_spatial_mask_ovf(int32_t in, int32_t inc,
                                                     int32_t mask,
                                                     int32_t *overflow)
{
    int32_t out = hmx_inc_with_spatial_mask(in, inc, mask);
    *overflow = out < in;
    return out;
}

static inline int32_t hmx_inc_tap_with_dilate(int32_t in, int32_t inc,
                                               int32_t mask, int32_t dilate)
{
    int32_t out = hmx_inc_with_spatial_mask(in, inc, mask);
    if (inc == 0) {
        return -1;
    }
    if (dilate && out >= 0) {
        out = hmx_inc_with_spatial_mask(out, inc, mask);
    }
    return out;
}

/*
 * Enumerate positions from start to stop using masked increment.
 * Returns the number of valid indices stored in array.
 */
static inline int hmx_compute_indices(int start, int stop, int inc,
                                       int mask_msb, int dilate,
                                       int *array, int max_entries)
{
    int counter = 0;
    int value = start;
    while (value >= 0 && counter < max_entries) {
        array[counter] = value;
        value = hmx_inc_tap_with_dilate(value, inc, mask_msb, dilate);
        if (value > stop) {
            value = -1;
        }
        counter++;
    }
    return counter;
}

/*
 * Instruction parameter encoding for C helpers.
 */

/* Weight types for matmul_fxp (bits 7:0) */
#define HMX_WEI_B      0
#define HMX_WEI_SM     1
#define HMX_WEI_N      2
#define HMX_WEI_C      3
#define HMX_WEI_SC     4
#define HMX_WEI_B1     5
#define HMX_WEI_SB1    6
#define HMX_WEI_N_2X   7

/* Weight types for matmul_fp (bits 7:0) */
#define HMX_WEI_HF     0
#define HMX_WEI_F8     1

/* Memory access modifier (bits 15:8) */
#define HMX_MOD_NORMAL  0
#define HMX_MOD_SINGLE  1
#define HMX_MOD_DR      2
#define HMX_MOD_DP      3
#define HMX_MOD_ABOVE   4
#define HMX_MOD_DI      5

/* Activation type (bits 3:0 of act_load params) */
#define HMX_ACT_UB     0
#define HMX_ACT_HF     1
#define HMX_ACT_F8     2

/* Activation format (bit 4 of act_load params) */
#define HMX_ACT_FMT_SM  0
#define HMX_ACT_FMT_DM  1

/* Activation modifier (bits 11:8 of act_load params) */
#define HMX_ACT_BLK    0
#define HMX_ACT_NOBLK  1
#define HMX_ACT_BLK_U  2
#define HMX_ACT_BLK_S  3
#define HMX_ACT_BLK_D  4

/* Convert direction (bits 1:0 of cvt_transfer params) */
#define HMX_CVT_LEFT   0
#define HMX_CVT_RIGHT  1
#define HMX_CVT_BOTTOM 2
#define HMX_CVT_ABOVE  3

/* Convert output format (bits 6:4 of cvt_transfer params) */
#define HMX_CVT_FMT_UB_SM  0
#define HMX_CVT_FMT_UB_DM  1
#define HMX_CVT_FMT_UH     2
#define HMX_CVT_FMT_UH2X2  3
#define HMX_CVT_FMT_HF     4

/* Convert flags (bits 9:8 of cvt_transfer params) */
#define HMX_CVT_RELU_BIT   8
#define HMX_CVT_RETAIN_BIT 9

/* Convert store format (bits 3:0 of cvt_store params) */
#define HMX_CVTST_NORMAL   0
#define HMX_CVTST_CM       1
#define HMX_CVTST_2X2      2
#define HMX_CVTST_F8       3

/* Convert store age (bits 11:8 of cvt_store params) */
#define HMX_CVTST_AGE0     0
#define HMX_CVTST_AGE1     1

/* Convert scalar type (for cvt_rs) */
#define HMX_CVT_RS_UB      0
#define HMX_CVT_RS_UB_SC0  1
#define HMX_CVT_RS_UB_SC1  2
#define HMX_CVT_RS_UH_2X1  3
#define HMX_CVT_RS_UH_2X2  4
#define HMX_CVT_RS_HF      5
#define HMX_CVT_RS_F8      6

/* Macros to pack/unpack helper parameters */
#define HMX_PACK_WEI(type, mod)   ((type) | ((mod) << 8))
#define HMX_UNPACK_WEI_TYPE(p)    ((p) & 0xFF)
#define HMX_UNPACK_MOD(p)         (((p) >> 8) & 0xFF)

#define HMX_PACK_ACT(type, fmt, mod) \
    ((type) | ((fmt) << 4) | ((mod) << 8))
#define HMX_UNPACK_ACT_TYPE(p)    ((p) & 0xF)
#define HMX_UNPACK_ACT_FMT(p)    (((p) >> 4) & 0xF)
#define HMX_UNPACK_ACT_MOD(p)    (((p) >> 8) & 0xF)

#define HMX_PACK_CVT(dir, fmt, relu, retain) \
    ((dir) | ((fmt) << 4) | ((relu) << HMX_CVT_RELU_BIT) | \
     ((retain) << HMX_CVT_RETAIN_BIT))
#define HMX_UNPACK_CVT_DIR(p)    ((p) & 0xF)
#define HMX_UNPACK_CVT_FMT(p)    (((p) >> 4) & 0xF)
#define HMX_UNPACK_CVT_RELU(p)   (((p) >> HMX_CVT_RELU_BIT) & 1)
#define HMX_UNPACK_CVT_RETAIN(p) (((p) >> HMX_CVT_RETAIN_BIT) & 1)

#define HMX_PACK_CVTST(fmt, age)  ((fmt) | ((age) << 8))
#define HMX_UNPACK_CVTST_FMT(p)   ((p) & 0xFF)
#define HMX_UNPACK_CVTST_AGE(p)   (((p) >> 8) & 0xFF)

/*
 * Activation buffer address functions.
 *
 * Spatial major: byte for spatial s, channel c is at:
 *   offset = (s[5:2] << 7) | (c[4:0] << 2) | s[1:0]
 *
 * Channel major: byte for spatial s, channel c is at:
 *   offset = (s[5:0] << 5) | c[4:0]
 */
static inline int hmx_act_offset_sm(int s, int c)
{
    return ((s >> 2) << 7) | (c << 2) | (s & 3);
}

static inline int hmx_act_offset_cm(int s, int c)
{
    return (s << 5) | c;
}

/*
 * Bias register field extraction from raw 64-bit value.
 *
 * Layout per spec (ch9_bias_reg):
 *   [63:32] = Input bias (signed 32-bit)
 *   [31]    = Scale[0]
 *   [30:23] = ~Output bias[11:4]
 *   [22]    = Output bias[3]
 *   [21:19] = Output bias[2:0]
 *   [18:17] = Shape[1:0]
 *   [16]    = ~Scale[11]
 *   [15]    = Shape[2]
 *   [14:10] = Exponent[4:0]
 *   [9:0]   = Scale[10:1]
 */
static inline int32_t hmx_bias_input_bias(uint64_t raw)
{
    /*
     * In 8x8 FXP mode (legacy), all 32 bits [63:32] are the input bias.
     * The 8x4 path masks with 0xFFFFFFF0 because bits
     * [35:32] overlap with FP fields, but the 8x8 path uses the full value.
     */
    return (int32_t)(raw >> 32);
}

static inline int hmx_bias_exponent(uint64_t raw)
{
    return (raw >> 10) & 0x1F;
}

static inline int hmx_bias_shape(uint64_t raw)
{
    int s10 = (raw >> 17) & 0x3;  /* Shape[1:0] */
    int s2 = (raw >> 15) & 0x1;   /* Shape[2] */
    return (s2 << 2) | s10;
}

static inline uint32_t hmx_bias_scale(uint64_t raw)
{
    int bit0 = (raw >> 31) & 1;         /* Scale[0] = BIAS[31] */
    int bits10_1 = raw & 0x3FF;         /* Scale[10:1] = BIAS[9:0] */
    int bit11 = !((raw >> 16) & 1);     /* Scale[11] = ~BIAS[16] */
    return (bit11 << 11) | (bits10_1 << 1) | bit0;
}

static inline int32_t hmx_bias_output_bias(uint64_t raw)
{
    int bits2_0 = (raw >> 19) & 0x7;    /* Out bias[2:0] = BIAS[21:19] */
    int bit3 = (raw >> 22) & 1;         /* Out bias[3] = BIAS[22] */
    uint32_t bits11_4 = (raw >> 23) & 0xFF; /* Out bias[11:4] = BIAS[30:23] */
    int32_t ob = (bits11_4 << 4) | (bit3 << 3) | bits2_0;
    /* Sign-extend from 12 bits */
    if (ob & 0x800) {
        ob |= 0xFFFFF000;
    }
    return ob;
}

static inline uint32_t hmx_bias_output_bias_unsigned(uint64_t raw)
{
    int bits2_0 = (raw >> 19) & 0x7;    /* Out bias[2:0] = BIAS[21:19] */
    int bit3 = (raw >> 22) & 1;         /* Out bias[3] = BIAS[22] */
    uint32_t bits11_4 = (raw >> 23) & 0xFF; /* Out bias[11:4] = BIAS[30:23] */
    return (bits11_4 << 4) | (bit3 << 3) | bits2_0;
}

#endif /* HEXAGON_HMX_STATE_H */
