/*
 * HMX operation-family TCG test suite.
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>

int err;

#include "hex_test.h"

static void clear_accumulators(void)
{
    asm volatile(".word 0xa6e0c011    /* mxclracc */" : : : "memory");
}

/* hmx_bias */
#define HMX_BIAS_MX_COLS 32

static uint32_t HMX_BIAS_src[HMX_BIAS_MX_COLS] __attribute__((aligned(128)));
static uint32_t HMX_BIAS_dst[HMX_BIAS_MX_COLS] __attribute__((aligned(128)));

static int hmx_test_hmx_bias(void)
{
    err = 0;
    clear_accumulators();
    for (int i = 0; i < HMX_BIAS_MX_COLS; i++) {
        HMX_BIAS_src[i] = 0x10000000 * (i + 1) + i;
        HMX_BIAS_dst[i] = 0xdeadbeef;
    }

    /* bias = mxmem(r2), r2 = &HMX_BIAS_src */
    asm volatile("r2 = %0\n"
                 ".word 0x9202c3ff    /* bias=mxmem(r2) */\n"
                 :
                 : "r"(HMX_BIAS_src)
                 : "r2", "memory");

    /* mxmem(r3) = bias, r3 = &HMX_BIAS_dst */
    asm volatile("r3 = %0\n"
                 ".word 0xa6e3c010    /* mxmem(r3)=bias */\n"
                 :
                 : "r"(HMX_BIAS_dst)
                 : "r3", "memory");

    for (int i = 0; i < HMX_BIAS_MX_COLS; i++) {
        check32(HMX_BIAS_dst[i], HMX_BIAS_src[i]);
    }

    puts(err ? "FAIL" : "PASS");
    return err ? 1 : 0;
}

/* hmx_matmul_fxp */
#define F_ACT_SIZE  2048  /* one crouton: 64 spatial x 32 channels */
#define F_WEI_SIZE  1024  /* 8 vectors x 128 bytes: 32 input channels, byte */
#define F_OUT_SIZE  2048

static uint8_t F_act_buf[F_ACT_SIZE] __attribute__((aligned(2048)));
static uint8_t F_wei_buf[F_WEI_SIZE] __attribute__((aligned(128)));
static uint32_t F_bias_buf[32] __attribute__((aligned(128)));
static uint8_t F_out_buf[F_OUT_SIZE] __attribute__((aligned(2048)));

/*
 * Every activation byte is 3, every weight byte is 5.  With a single
 * spatial tap and no group convolution, each of the 64x32 accumulator
 * cells sums 32 input-channel MACs of 3*5=15, giving 480 uniformly.
 *
 * The bias register (input_bias=0, exponent=20, shape=0, scale=0x800,
 * out_bias=0, encoded directly against hmx_state.h's
 * hmx_bias_exponent()/hmx_bias_scale()/hmx_bias_shape()/
 * hmx_bias_output_bias_unsigned() extraction formulas -- NOT via
 * pack_fxp_bias()-style helpers, which invert the output-bias field
 * on the way in to match real hardware's wire encoding, and would
 * need their own out_bias to be pre-inverted to land on out_bias=0
 * here) was chosen, by running the real hmx_u8_cvt() algorithm
 * natively, to map accumulator value 480 through hmx_u8_cvt() to
 * exactly 480 again -- an identity point for this specific
 * accumulator value, not a general identity transform. The legacy
 * convert-and-store path then right-shifts the 12-bit hmx_u8_cvt()
 * result by 4 before storing the byte, so the final stored value is
 * 480 >> 4 = 30.
 */
#define F_EXPECTED_ACC     480
#define F_EXPECTED_BYTE    (F_EXPECTED_ACC >> 4)
#define F_BIAS_LOW32       0x5000u

static int hmx_test_hmx_matmul_fxp(void)
{
    err = 0;
    clear_accumulators();
    memset(F_act_buf, 3, sizeof(F_act_buf));
    memset(F_wei_buf, 5, sizeof(F_wei_buf));
    for (int i = 0; i < 32; i++) {
        F_bias_buf[i] = F_BIAS_LOW32;
    }
    memset(F_out_buf, 0xff, sizeof(F_out_buf));

    /* bias = mxmem(r8) */
    asm volatile("r8 = %0\n"
                 ".word 0x9208c3ff    /* bias=mxmem(r8) */\n"
                 :
                 : "r"(F_bias_buf)
                 : "r8", "memory");

    /*
     * activation.ub = mxmem(r2,r3) : r3 selects the full [0,32) channel
     * range and a single spatial tap (see hmx_matmul_fxp.c's HELPER
     * commit message for the derivation).
     */
    asm volatile("r2 = %0\n"
                 "r3 = #0x7c\n"
                 ".word 0x9202c3ec    /* activation.ub=mxmem(r2,r3) */\n"
                 :
                 : "r"(F_act_buf)
                 : "r2", "r3", "memory");

    /* weight.b = mxmem(r4,r5) -- triggers the FXP matrix multiply. */
    asm volatile("r4 = %0\n"
                 "r5 = #0xffff\n"
                 ".word 0x9204e5e0    /* weight.b=mxmem(r4,r5) */\n"
                 :
                 : "r"(F_wei_buf)
                 : "r4", "r5", "memory");

    /* mxmem(r6,r7):after:sat.ub=acc */
    asm volatile("r6 = %0\n"
                 "r7 = #0\n"
                 ".word 0xa6e6c704    "
                 "/* mxmem(r6,r7):after:sat.ub=acc */\n"
                 :
                 : "r"(F_out_buf)
                 : "r6", "r7", "memory");

    for (int i = 0; i < F_OUT_SIZE; i++) {
        check32(F_out_buf[i], F_EXPECTED_BYTE);
    }

    puts(err ? "FAIL" : "PASS");
    return err ? 1 : 0;
}

/* hmx_matmul_fxp_nibble */
#define N_ACT_SIZE  2048  /* one crouton: 64 spatial x 32 channels */
#define N_WEI_SIZE  512   /* 4 vectors x 128 bytes: 32 channels, cpv=8 */
#define N_OUT_SIZE  2048

static uint8_t N_act_buf[N_ACT_SIZE] __attribute__((aligned(2048)));
static uint8_t N_wei_buf[N_WEI_SIZE] __attribute__((aligned(128)));
static uint32_t N_bias_buf[32] __attribute__((aligned(128)));
static uint8_t N_out_buf[N_OUT_SIZE] __attribute__((aligned(2048)));

/*
 * Every activation byte is 3. Every weight byte is 0x33: nibble
 * unpack sign-extends each 4-bit nibble, and 0b0011 (3) is already
 * positive, so both nibbles of every weight byte decode to weight
 * value 3 for every one of the 32 input channels' stream indices.
 *
 * Single spatial tap, no group convolution (same setup as
 * hmx_matmul_fxp.c): each of the 64x32 accumulator cells sums 32
 * input-channel MACs of 3*3=9, giving 288 uniformly.
 *
 * Reusing hmx_matmul_fxp.c's bias register (exponent=20, scale=0x800,
 * out_bias=0): verified natively that this maps accumulator value 288
 * through hmx_u8_cvt() to exactly 288 again (the same identity point
 * that worked for 480 -- this configuration is linear across the
 * range tested, not just at 480). The legacy convert-and-store path
 * right-shifts that 12-bit result by 4 before storing the byte, so
 * the expected stored value is 288 >> 4 = 18.
 */
#define N_EXPECTED_ACC     288
#define N_EXPECTED_BYTE    (N_EXPECTED_ACC >> 4)
#define N_BIAS_LOW32       0x5000u

static int hmx_test_hmx_matmul_fxp_nibble(void)
{
    err = 0;
    clear_accumulators();
    memset(N_act_buf, 3, sizeof(N_act_buf));
    memset(N_wei_buf, 0x33, sizeof(N_wei_buf));
    for (int i = 0; i < 32; i++) {
        N_bias_buf[i] = N_BIAS_LOW32;
    }
    memset(N_out_buf, 0xff, sizeof(N_out_buf));

    /* bias = mxmem(r8) */
    asm volatile("r8 = %0\n"
                 ".word 0x9208c3ff    /* bias=mxmem(r8) */\n"
                 :
                 : "r"(N_bias_buf)
                 : "r8", "memory");

    /* activation.ub = mxmem(r2,r3) */
    asm volatile("r2 = %0\n"
                 "r3 = #0x7c\n"
                 ".word 0x9202c3ec    /* activation.ub=mxmem(r2,r3) */\n"
                 :
                 : "r"(N_act_buf)
                 : "r2", "r3", "memory");

    /* weight.n = mxmem(r4,r5) -- triggers the FXP matrix multiply. */
    asm volatile("r4 = %0\n"
                 "r5 = #0xffff\n"
                 ".word 0x9204e5e1    /* weight.n=mxmem(r4,r5) */\n"
                 :
                 : "r"(N_wei_buf)
                 : "r4", "r5", "memory");

    /* mxmem(r6,r7):after:sat.ub=acc */
    asm volatile("r6 = %0\n"
                 "r7 = #0\n"
                 ".word 0xa6e6c704    "
                 "/* mxmem(r6,r7):after:sat.ub=acc */\n"
                 :
                 : "r"(N_out_buf)
                 : "r6", "r7", "memory");

    for (int i = 0; i < N_OUT_SIZE; i++) {
        check32(N_out_buf[i], N_EXPECTED_BYTE);
    }

    puts(err ? "FAIL" : "PASS");
    return err ? 1 : 0;
}

/* hmx_matmul_fxp_dm */
#define D_ACT_SIZE  2048  /* one crouton: 64 spatial x 32 channels */
#define D_WEI_SIZE  1024  /* 8 vectors x 128 bytes: 32 channels, byte */
#define D_OUT_SIZE  2048

static uint8_t D_act_buf[D_ACT_SIZE] __attribute__((aligned(2048)));
static uint8_t D_wei_buf[D_WEI_SIZE] __attribute__((aligned(128)));
static uint32_t D_bias_buf[32] __attribute__((aligned(128)));
static uint8_t D_out_buf[D_OUT_SIZE] __attribute__((aligned(2048)));

/*
 * Same act=3/weight=5 uniform pattern as hmx_matmul_fxp.c. DM's
 * spatial mask (HMX_SPATIAL_MASK_BITS_CM=0x7E0) has the same 6-bit
 * hamming weight as SM's (0x783), i.e. the same 64 spatial positions,
 * so the expected accumulator (480, uniformly) and bias/expected byte
 * (30) are identical -- only the address *arithmetic* getting there
 * differs.
 */
#define D_EXPECTED_ACC     480
#define D_EXPECTED_BYTE    (D_EXPECTED_ACC >> 4)
#define D_BIAS_LOW32       0x5000u

static int hmx_test_hmx_matmul_fxp_dm(void)
{
    err = 0;
    clear_accumulators();
    memset(D_act_buf, 3, sizeof(D_act_buf));
    memset(D_wei_buf, 5, sizeof(D_wei_buf));
    for (int i = 0; i < 32; i++) {
        D_bias_buf[i] = D_BIAS_LOW32;
    }
    memset(D_out_buf, 0xff, sizeof(D_out_buf));

    /* bias = mxmem(r8) */
    asm volatile("r8 = %0\n"
                 ".word 0x9208c3ff    /* bias=mxmem(r8) */\n"
                 :
                 : "r"(D_bias_buf)
                 : "r8", "memory");

    /*
     * activation.ub = mxmem(r2,r3):cm -- r3=0x1f selects the full
     * [0,32) channel range and a single spatial tap in DM format
     * (see this file's header comment).
     */
    asm volatile("r2 = %0\n"
                 "r3 = #0x1f\n"
                 ".word 0x9202c3ed    /* activation.ub=mxmem(r2,r3):cm */\n"
                 :
                 : "r"(D_act_buf)
                 : "r2", "r3", "memory");

    /*
     * weight.b = mxmem(r4,r5) -- format-agnostic; reads the DM mode
     * hmx_act_load() just latched.
     */
    asm volatile("r4 = %0\n"
                 "r5 = #0xffff\n"
                 ".word 0x9204e5e0    /* weight.b=mxmem(r4,r5) */\n"
                 :
                 : "r"(D_wei_buf)
                 : "r4", "r5", "memory");

    /* mxmem(r6,r7):after:sat.ub:cm=acc */
    asm volatile("r6 = %0\n"
                 "r7 = #0\n"
                 ".word 0xa6e6c705    "
                 "/* mxmem(r6,r7):after:sat.ub:cm=acc */\n"
                 :
                 : "r"(D_out_buf)
                 : "r6", "r7", "memory");

    for (int i = 0; i < D_OUT_SIZE; i++) {
        check32(D_out_buf[i], D_EXPECTED_BYTE);
    }

    puts(err ? "FAIL" : "PASS");
    return err ? 1 : 0;
}

/* hmx_matmul_fxp_signed_crumb */
#define SC_ACT_SIZE  2048  /* one crouton: 64 spatial x 32 channels */
#define SC_WEI_SIZE  256   /* 2 vectors x 128 bytes: 32 channels, cpv=16 */
#define SC_OUT_SIZE  2048

static uint8_t SC_act_buf[SC_ACT_SIZE] __attribute__((aligned(2048)));
static uint8_t SC_wei_buf[SC_WEI_SIZE] __attribute__((aligned(128)));
static uint32_t SC_bias_buf[32] __attribute__((aligned(128)));
static uint8_t SC_out_buf[SC_OUT_SIZE] __attribute__((aligned(2048)));

/*
 * Every activation byte is 3. Every weight byte is 0x00: each 2-bit
 * crumb is 0b00, and the signed-crumb lookup table maps crumb value 0
 * to weight value 2 (not 0 -- this is the point of testing it: a
 * plain sign-extending unpack would treat 0b00 as weight 0, which
 * would silently pass even a broken lookup-table implementation that
 * just zeroed everything).
 *
 * Single spatial tap, no group convolution (same setup as
 * hmx_matmul_fxp.c): each of the 64x32 accumulator cells sums 32
 * input-channel MACs of 3*2=6, giving 192 uniformly.
 *
 * Reusing hmx_matmul_fxp.c's bias register (exponent=20, scale=0x800,
 * out_bias=0): verified natively that this identity configuration
 * also holds for accumulator value 192. Expected stored byte:
 * 192 >> 4 = 12.
 */
#define SC_EXPECTED_ACC     192
#define SC_EXPECTED_BYTE    (SC_EXPECTED_ACC >> 4)
#define SC_BIAS_LOW32       0x5000u

static int hmx_test_hmx_matmul_fxp_signed_crumb(void)
{
    err = 0;
    clear_accumulators();
    memset(SC_act_buf, 3, sizeof(SC_act_buf));
    memset(SC_wei_buf, 0x00, sizeof(SC_wei_buf));
    for (int i = 0; i < 32; i++) {
        SC_bias_buf[i] = SC_BIAS_LOW32;
    }
    memset(SC_out_buf, 0xff, sizeof(SC_out_buf));

    /* bias = mxmem(r8) */
    asm volatile("r8 = %0\n"
                 ".word 0x9208c3ff    /* bias=mxmem(r8) */\n"
                 :
                 : "r"(SC_bias_buf)
                 : "r8", "memory");

    /* activation.ub = mxmem(r2,r3) */
    asm volatile("r2 = %0\n"
                 "r3 = #0x7c\n"
                 ".word 0x9202c3ec    /* activation.ub=mxmem(r2,r3) */\n"
                 :
                 : "r"(SC_act_buf)
                 : "r2", "r3", "memory");

    /* weight.sc = mxmem(r4,r5) -- triggers the FXP matrix multiply. */
    asm volatile("r4 = %0\n"
                 "r5 = #0xffff\n"
                 ".word 0x9204e5f0    /* weight.sc=mxmem(r4,r5) */\n"
                 :
                 : "r"(SC_wei_buf)
                 : "r4", "r5", "memory");

    /* mxmem(r6,r7):after:sat.ub=acc */
    asm volatile("r6 = %0\n"
                 "r7 = #0\n"
                 ".word 0xa6e6c704    "
                 "/* mxmem(r6,r7):after:sat.ub=acc */\n"
                 :
                 : "r"(SC_out_buf)
                 : "r6", "r7", "memory");

    for (int i = 0; i < SC_OUT_SIZE; i++) {
        check32(SC_out_buf[i], SC_EXPECTED_BYTE);
    }

    puts(err ? "FAIL" : "PASS");
    return err ? 1 : 0;
}

/* hmx_matmul_fxp_sign_magnitude */
#define SM_ACT_SIZE  2048  /* one crouton: 64 spatial x 32 channels */
#define SM_WEI_SIZE  1024  /* 8 vectors x 128 bytes: 32 channels, cpv=4 */
#define SM_OUT_SIZE  2048

static uint8_t SM_act_buf[SM_ACT_SIZE] __attribute__((aligned(2048)));
static uint8_t SM_wei_buf[SM_WEI_SIZE] __attribute__((aligned(128)));
static uint32_t SM_bias_buf[32] __attribute__((aligned(128)));
static uint8_t SM_out_buf[SM_OUT_SIZE] __attribute__((aligned(2048)));

/*
 * Every activation byte is 3. Every weight byte is 0x80, which
 * hmx_extract_weights_sm() decodes to weight value -1 for every one
 * of the 32 input channels.
 *
 * Single spatial tap, no group convolution (same setup as every
 * other HMX matmul test): each of the 64x32 accumulator cells sums
 * 32 input-channel MACs of 3*(-1)=-3, giving -96 uniformly.
 *
 * Reusing the identity bias register (exponent=20, scale=0x800,
 * out_bias=0) with the *non*-saturating convert (M8_mxcvtr_ub, not
 * _sat_): a saturating convert would just clamp -96 to 0, which
 * wouldn't distinguish a correctly-signed -96 from an
 * incorrectly-zeroed or garbage accumulator. Non-saturating masks to
 * 12 bits (two's complement): -96 mod 4096 = 4000 = 0xFA0, and the
 * legacy convert-and-store path right-shifts that by 4 before storing
 * the byte, so the expected stored value is 0xFA0 >> 4 = 0xFA = 250 --
 * verified against the real hmx_u8_cvt()/hmx_sat_to_max() algorithm
 * run natively, not hand-derived.
 */
#define SM_EXPECTED_BYTE    250
#define SM_BIAS_LOW32       0x5000u

static int hmx_test_hmx_matmul_fxp_sign_magnitude(void)
{
    err = 0;
    clear_accumulators();
    memset(SM_act_buf, 3, sizeof(SM_act_buf));
    memset(SM_wei_buf, 0x80, sizeof(SM_wei_buf));
    for (int i = 0; i < 32; i++) {
        SM_bias_buf[i] = SM_BIAS_LOW32;
    }
    memset(SM_out_buf, 0xff, sizeof(SM_out_buf));

    /* bias = mxmem(r8) */
    asm volatile("r8 = %0\n"
                 ".word 0x9208c3ff    /* bias=mxmem(r8) */\n"
                 :
                 : "r"(SM_bias_buf)
                 : "r8", "memory");

    /* activation.ub = mxmem(r2,r3) */
    asm volatile("r2 = %0\n"
                 "r3 = #0x7c\n"
                 ".word 0x9202c3ec    /* activation.ub=mxmem(r2,r3) */\n"
                 :
                 : "r"(SM_act_buf)
                 : "r2", "r3", "memory");

    /* weight.sm = mxmem(r4,r5) -- triggers the FXP matrix multiply. */
    asm volatile("r4 = %0\n"
                 "r5 = #0xffff\n"
                 ".word 0x9204e5f1    /* weight.sm=mxmem(r4,r5) */\n"
                 :
                 : "r"(SM_wei_buf)
                 : "r4", "r5", "memory");

    /* mxmem(r6,r7):after.ub=acc (non-saturating) */
    asm volatile("r6 = %0\n"
                 "r7 = #0\n"
                 ".word 0xa6e6c706    /* mxmem(r6,r7):after.ub=acc */\n"
                 :
                 : "r"(SM_out_buf)
                 : "r6", "r7", "memory");

    for (int i = 0; i < SM_OUT_SIZE; i++) {
        check32(SM_out_buf[i], SM_EXPECTED_BYTE);
    }

    puts(err ? "FAIL" : "PASS");
    return err ? 1 : 0;
}

/* hmx_cvt_rs */
#define C_ACT_SIZE  2048  /* one crouton: 64 spatial x 32 channels */
#define C_WEI_SIZE  1024  /* 8 vectors x 128 bytes: 32 channels, byte */
#define C_OUT_SIZE  2048

static uint8_t C_act_buf[C_ACT_SIZE] __attribute__((aligned(2048)));
static uint8_t C_wei_buf[C_WEI_SIZE] __attribute__((aligned(128)));
static uint32_t C_bias_buf[32] __attribute__((aligned(128)));
static uint8_t C_out_buf[C_OUT_SIZE] __attribute__((aligned(2048)));

#define C_EXPECTED_ACC     480
#define C_EXPECTED_BYTE    (C_EXPECTED_ACC >> 4)
#define C_BIAS_LOW32       0x5000u

static int hmx_test_hmx_cvt_rs(void)
{
    err = 0;
    clear_accumulators();
    memset(C_act_buf, 3, sizeof(C_act_buf));
    memset(C_wei_buf, 5, sizeof(C_wei_buf));
    for (int i = 0; i < 32; i++) {
        C_bias_buf[i] = C_BIAS_LOW32;
    }
    memset(C_out_buf, 0xff, sizeof(C_out_buf));

    /* bias = mxmem(r8) */
    asm volatile("r8 = %0\n"
                 ".word 0x9208c3ff    /* bias=mxmem(r8) */\n"
                 :
                 : "r"(C_bias_buf)
                 : "r8", "memory");

    /* activation.ub = mxmem(r2,r3) */
    asm volatile("r2 = %0\n"
                 "r3 = #0x7c\n"
                 ".word 0x9202c3ec    /* activation.ub=mxmem(r2,r3) */\n"
                 :
                 : "r"(C_act_buf)
                 : "r2", "r3", "memory");

    /* weight.b = mxmem(r4,r5) -- triggers the FXP matrix multiply. */
    asm volatile("r4 = %0\n"
                 "r5 = #0xffff\n"
                 ".word 0x9204e5e0    /* weight.b=mxmem(r4,r5) */\n"
                 :
                 : "r"(C_wei_buf)
                 : "r4", "r5", "memory");

    /*
     * cvt.ub=acc(r9), r9=1 (bit0=1: retain acc; bit1=0: relu=1/
     * saturating; fb_dst=0: no feedback; bias_sel=0). Its own packet,
     * so HELPER(hmx_commit_packet) flushes the pending convert at the
     * end of this instruction before the next one runs.
     */
    asm volatile("r9 = #1\n"
                 ".word 0xa6e9d710    /* cvt.ub=acc(r9) */\n"
                 :
                 :
                 : "r9", "memory");

    /* mxmem(r6,r7)=cvt -- reads the now-flushed convert result. */
    asm volatile("r6 = %0\n"
                 "r7 = #0\n"
                 ".word 0xa6e6c718    /* mxmem(r6,r7)=cvt */\n"
                 :
                 : "r"(C_out_buf)
                 : "r6", "r7", "memory");

    for (int i = 0; i < C_OUT_SIZE; i++) {
        check32(C_out_buf[i], C_EXPECTED_BYTE);
    }

    puts(err ? "FAIL" : "PASS");
    return err ? 1 : 0;
}

/* hmx_matmul_fxp_bit */
#define BT_ACT_SIZE  2048  /* one crouton: 64 spatial x 32 channels */
#define BT_WEI_SIZE  128   /* 1 vector: 32 channels, cpv=32 */
#define BT_OUT_SIZE  2048

static uint8_t BT_act_buf[BT_ACT_SIZE] __attribute__((aligned(2048)));
static uint8_t BT_wei_buf[BT_WEI_SIZE] __attribute__((aligned(128)));
static uint32_t BT_bias_buf[32] __attribute__((aligned(128)));
static uint8_t BT_out_buf[BT_OUT_SIZE] __attribute__((aligned(2048)));

/*
 * Every activation byte is 3. Every weight bit is 1: with cpv=32,
 * every one of the 32 channels' stream indices reads a different bit
 * position across the vector's 4 words, so making every extraction
 * come out to weight=1 needs every bit of the whole 128-byte vector
 * set (0xFF throughout), not just one word.
 *
 * Single spatial tap, no group convolution: each of the 64x32
 * accumulator cells sums 32 input-channel MACs of 3*1=3, giving 96
 * uniformly. Reusing the identity bias register (exponent=20,
 * scale=0x800, out_bias=0): verified natively that it also holds for
 * accumulator value 96. Expected stored byte: 96 >> 4 = 6.
 */
#define BT_EXPECTED_ACC     96
#define BT_EXPECTED_BYTE    (BT_EXPECTED_ACC >> 4)
#define BT_BIAS_LOW32       0x5000u

static int hmx_test_hmx_matmul_fxp_bit(void)
{
    err = 0;
    clear_accumulators();
    memset(BT_act_buf, 3, sizeof(BT_act_buf));
    memset(BT_wei_buf, 0xff, sizeof(BT_wei_buf));
    for (int i = 0; i < 32; i++) {
        BT_bias_buf[i] = BT_BIAS_LOW32;
    }
    memset(BT_out_buf, 0xff, sizeof(BT_out_buf));

    /* bias = mxmem(r8) */
    asm volatile("r8 = %0\n"
                 ".word 0x9208c3ff    /* bias=mxmem(r8) */\n"
                 :
                 : "r"(BT_bias_buf)
                 : "r8", "memory");

    /* activation.ub = mxmem(r2,r3) */
    asm volatile("r2 = %0\n"
                 "r3 = #0x7c\n"
                 ".word 0x9202c3ec    /* activation.ub=mxmem(r2,r3) */\n"
                 :
                 : "r"(BT_act_buf)
                 : "r2", "r3", "memory");

    /* weight.ubit = mxmem(r4,r5) -- triggers the FXP matrix multiply. */
    asm volatile("r4 = %0\n"
                 "r5 = #0xffff\n"
                 ".word 0x9204e5e3    /* weight.ubit=mxmem(r4,r5) */\n"
                 :
                 : "r"(BT_wei_buf)
                 : "r4", "r5", "memory");

    /* mxmem(r6,r7):after:sat.ub=acc */
    asm volatile("r6 = %0\n"
                 "r7 = #0\n"
                 ".word 0xa6e6c704    "
                 "/* mxmem(r6,r7):after:sat.ub=acc */\n"
                 :
                 : "r"(BT_out_buf)
                 : "r6", "r7", "memory");

    for (int i = 0; i < BT_OUT_SIZE; i++) {
        check32(BT_out_buf[i], BT_EXPECTED_BYTE);
    }

    puts(err ? "FAIL" : "PASS");
    return err ? 1 : 0;
}

/* hmx_matmul_fxp_signed_bit */
#define SB_ACT_SIZE  2048  /* one crouton: 64 spatial x 32 channels */
#define SB_WEI_SIZE  128   /* 1 vector: 32 channels, cpv=32 */
#define SB_OUT_SIZE  2048

static uint8_t SB_act_buf[SB_ACT_SIZE] __attribute__((aligned(2048)));
static uint8_t SB_wei_buf[SB_WEI_SIZE] __attribute__((aligned(128)));
static uint32_t SB_bias_buf[32] __attribute__((aligned(128)));
static uint8_t SB_out_buf[SB_OUT_SIZE] __attribute__((aligned(2048)));

/*
 * Every activation byte is 3. Every weight bit is 1 (whole vector
 * 0xFF, same reasoning as hmx_matmul_fxp_bit.c's cpv=32 addressing),
 * which hmx_extract_weights_signed_bit() decodes to weight -1 (bit=1
 * is the *negative* case, the opposite of the unsigned-bit type's
 * plain bit=1 -> weight=1).
 *
 * Single spatial tap, no group convolution: each of the 64x32
 * accumulator cells sums 32 input-channel MACs of 3*(-1)=-3, giving
 * -96 uniformly -- same accumulator value as
 * hmx_matmul_fxp_sign_magnitude.c, so the same non-saturating convert
 * (M8_mxcvtr_ub) and expected byte (250) apply; see that file for the
 * two's-complement masking rationale.
 */
#define SB_EXPECTED_BYTE    250
#define SB_BIAS_LOW32       0x5000u

static int hmx_test_hmx_matmul_fxp_signed_bit(void)
{
    err = 0;
    clear_accumulators();
    memset(SB_act_buf, 3, sizeof(SB_act_buf));
    memset(SB_wei_buf, 0xff, sizeof(SB_wei_buf));
    for (int i = 0; i < 32; i++) {
        SB_bias_buf[i] = SB_BIAS_LOW32;
    }
    memset(SB_out_buf, 0xff, sizeof(SB_out_buf));

    /* bias = mxmem(r8) */
    asm volatile("r8 = %0\n"
                 ".word 0x9208c3ff    /* bias=mxmem(r8) */\n"
                 :
                 : "r"(SB_bias_buf)
                 : "r8", "memory");

    /* activation.ub = mxmem(r2,r3) */
    asm volatile("r2 = %0\n"
                 "r3 = #0x7c\n"
                 ".word 0x9202c3ec    /* activation.ub=mxmem(r2,r3) */\n"
                 :
                 : "r"(SB_act_buf)
                 : "r2", "r3", "memory");

    /* weight.sbit = mxmem(r4,r5) -- triggers the FXP matrix multiply. */
    asm volatile("r4 = %0\n"
                 "r5 = #0xffff\n"
                 ".word 0x9204e5e4    /* weight.sbit=mxmem(r4,r5) */\n"
                 :
                 : "r"(SB_wei_buf)
                 : "r4", "r5", "memory");

    /* mxmem(r6,r7):after.ub=acc (non-saturating) */
    asm volatile("r6 = %0\n"
                 "r7 = #0\n"
                 ".word 0xa6e6c706    /* mxmem(r6,r7):after.ub=acc */\n"
                 :
                 : "r"(SB_out_buf)
                 : "r6", "r7", "memory");

    for (int i = 0; i < SB_OUT_SIZE; i++) {
        check32(SB_out_buf[i], SB_EXPECTED_BYTE);
    }

    puts(err ? "FAIL" : "PASS");
    return err ? 1 : 0;
}

/* hmx_matmul_fxp_uh */
#define U_ACT_SIZE  2048  /* one crouton: 64 spatial x 32 channels */
#define U_WEI_SIZE  1024  /* 8 vectors x 128 bytes: 32 channels, byte */
#define U_OUT_SIZE  2048

static uint8_t U_act_buf[U_ACT_SIZE] __attribute__((aligned(2048)));
static uint8_t U_wei_buf[U_WEI_SIZE] __attribute__((aligned(128)));
static uint32_t U_bias_buf[32] __attribute__((aligned(128)));
static uint8_t U_out_buf[U_OUT_SIZE] __attribute__((aligned(2048)));

/*
 * Every activation byte is 3, every weight byte is 5: same setup as
 * hmx_matmul_fxp.c, giving accumulator 480 uniformly across all 64x32
 * cells. hmx_u16_cvt() combines each adjacent pair of spatial rows
 * (s, s+1) -- both 480 here, since the accumulator is uniform -- into
 * one 20-bit result, split into a "lo" 12 bits (stored at row s) and
 * a "hi" 12 bits (stored at row s+1). With bias exponent=12 (not 20 --
 * a different exponent than every UB test, chosen because it's what
 * produces a clean split): the identity-style bias config from the UB
 * tests gives lo=30, hi=0. Since every pair sees the same input
 * (480, 480), this split value alternates uniformly by spatial parity
 * rather than being the same everywhere, which is what actually
 * demonstrates the pair-combination logic ran (not just replicated a
 * single value) -- verified against the real hmx_u16_cvt() algorithm
 * run natively, not hand-derived.
 */
#define U_EXPECTED_EVEN_BYTE  30  /* s even: "lo" half */
#define U_EXPECTED_ODD_BYTE   0   /* s odd: "hi" half */
#define U_BIAS_LOW32          0x3000u

static int hmx_test_hmx_matmul_fxp_uh(void)
{
    err = 0;
    clear_accumulators();
    memset(U_act_buf, 3, sizeof(U_act_buf));
    memset(U_wei_buf, 5, sizeof(U_wei_buf));
    for (int i = 0; i < 32; i++) {
        U_bias_buf[i] = U_BIAS_LOW32;
    }
    memset(U_out_buf, 0xff, sizeof(U_out_buf));

    /* bias = mxmem(r8) */
    asm volatile("r8 = %0\n"
                 ".word 0x9208c3ff    /* bias=mxmem(r8) */\n"
                 :
                 : "r"(U_bias_buf)
                 : "r8", "memory");

    /* activation.ub = mxmem(r2,r3) */
    asm volatile("r2 = %0\n"
                 "r3 = #0x7c\n"
                 ".word 0x9202c3ec    /* activation.ub=mxmem(r2,r3) */\n"
                 :
                 : "r"(U_act_buf)
                 : "r2", "r3", "memory");

    /* weight.b = mxmem(r4,r5) -- triggers the FXP matrix multiply. */
    asm volatile("r4 = %0\n"
                 "r5 = #0xffff\n"
                 ".word 0x9204e5e0    /* weight.b=mxmem(r4,r5) */\n"
                 :
                 : "r"(U_wei_buf)
                 : "r4", "r5", "memory");

    /* mxmem(r6,r7):after:sat.uh=acc */
    asm volatile("r6 = %0\n"
                 "r7 = #0\n"
                 ".word 0xa6e6e70a    "
                 "/* mxmem(r6,r7):after:sat.uh=acc */\n"
                 :
                 : "r"(U_out_buf)
                 : "r6", "r7", "memory");

    /* Spatial-major offset: byte for (s,c=0) is at ((s>>2)<<7)|(s&3). */
    for (int s = 0; s < 64; s++) {
        int off = ((s >> 2) << 7) | (s & 3);
        int expected = (s % 2 == 0) ? U_EXPECTED_EVEN_BYTE
                                    : U_EXPECTED_ODD_BYTE;
        check32(U_out_buf[off], expected);
    }

    puts(err ? "FAIL" : "PASS");
    return err ? 1 : 0;
}

/* hmx_matmul_fxp_uh2x2 */
#define U2_ACT_SIZE  2048  /* one crouton: 64 spatial x 32 channels */
#define U2_WEI_SIZE  1024  /* 8 vectors x 128 bytes: 32 channels, byte */
#define U2_OUT_SIZE  2048

static uint8_t U2_act_buf[U2_ACT_SIZE] __attribute__((aligned(2048)));
static uint8_t U2_wei_buf[U2_WEI_SIZE] __attribute__((aligned(128)));
static uint32_t U2_bias_buf[32] __attribute__((aligned(128)));
static uint8_t U2_out_buf[U2_OUT_SIZE] __attribute__((aligned(2048)));

/*
 * Every activation byte is 3, every weight byte is 5: same setup as
 * hmx_matmul_fxp.c, giving accumulator 480 uniformly. Every bias
 * register is the same value (exponent=12, scale=0x800, matching
 * hmx_matmul_fxp_uh.c's identity-style config) -- verified natively
 * that a 2x2 block of four identical 480s, with two identical
 * concatenated bias registers, produces the same clean 30/0 split
 * hmx_u16x16_cvt() gave for the 1D pair case, but only at the [s][o]
 * ("lo") corner of each 2x2 block: cvt->data[s+1][o] gets the "hi"
 * half (0 here), and cvt->data[s][o+1]/cvt->data[s+1][o+1] are
 * explicitly zeroed by the convert itself (see hmx_helper.c's
 * HELPER(hmx_cvt_transfer) UH2X2 branch) -- so the expected pattern is
 * byte 30 only where both the spatial row and the channel are even,
 * 0 everywhere else. That 3-out-of-4 zero pattern is what actually
 * demonstrates the 2x2 combine ran, not just a wider version of the
 * 2x1 case.
 */
#define U2_BIAS_LOW32  0x3000u

static int hmx_test_hmx_matmul_fxp_uh2x2(void)
{
    err = 0;
    clear_accumulators();
    memset(U2_act_buf, 3, sizeof(U2_act_buf));
    memset(U2_wei_buf, 5, sizeof(U2_wei_buf));
    for (int i = 0; i < 32; i++) {
        U2_bias_buf[i] = U2_BIAS_LOW32;
    }
    memset(U2_out_buf, 0xff, sizeof(U2_out_buf));

    /* bias = mxmem(r8) */
    asm volatile("r8 = %0\n"
                 ".word 0x9208c3ff    /* bias=mxmem(r8) */\n"
                 :
                 : "r"(U2_bias_buf)
                 : "r8", "memory");

    /* activation.ub = mxmem(r2,r3) */
    asm volatile("r2 = %0\n"
                 "r3 = #0x7c\n"
                 ".word 0x9202c3ec    /* activation.ub=mxmem(r2,r3) */\n"
                 :
                 : "r"(U2_act_buf)
                 : "r2", "r3", "memory");

    /* weight.b = mxmem(r4,r5) -- triggers the FXP matrix multiply. */
    asm volatile("r4 = %0\n"
                 "r5 = #0xffff\n"
                 ".word 0x9204e5e0    /* weight.b=mxmem(r4,r5) */\n"
                 :
                 : "r"(U2_wei_buf)
                 : "r4", "r5", "memory");

    /* mxmem(r6,r7):after:sat.uh2x2=acc */
    asm volatile("r6 = %0\n"
                 "r7 = #0\n"
                 ".word 0xa6e6e71a    "
                 "/* mxmem(r6,r7):after:sat.uh2x2=acc */\n"
                 :
                 : "r"(U2_out_buf)
                 : "r6", "r7", "memory");

    /* Spatial-major offset: byte for (s,c) is at ((s>>2)<<7)|(c<<2)|(s&3). */
    for (int s = 0; s < 64; s++) {
        for (int o = 0; o < 32; o++) {
            int off = ((s >> 2) << 7) | (o << 2) | (s & 3);
            int expected = (s % 2 == 0 && o % 2 == 0) ? 30 : 0;
            check32(U2_out_buf[off], expected);
        }
    }

    puts(err ? "FAIL" : "PASS");
    return err ? 1 : 0;
}

/* hmx_cvt_rs_uh2x2 */
#define CV_ACT_SIZE  2048  /* one crouton: 64 spatial x 32 channels */
#define CV_WEI_SIZE  1024  /* 8 vectors x 128 bytes: 32 channels, byte */
#define CV_OUT_SIZE  2048

static uint8_t CV_act_buf[CV_ACT_SIZE] __attribute__((aligned(2048)));
static uint8_t CV_wei_buf[CV_WEI_SIZE] __attribute__((aligned(128)));
static uint32_t CV_bias_buf[32] __attribute__((aligned(128)));
static uint8_t CV_out_buf[CV_OUT_SIZE] __attribute__((aligned(2048)));

#define CV_BIAS_LOW32  0x3000u

static int hmx_test_hmx_cvt_rs_uh2x2(void)
{
    err = 0;
    clear_accumulators();
    memset(CV_act_buf, 3, sizeof(CV_act_buf));
    memset(CV_wei_buf, 5, sizeof(CV_wei_buf));
    for (int i = 0; i < 32; i++) {
        CV_bias_buf[i] = CV_BIAS_LOW32;
    }
    memset(CV_out_buf, 0xff, sizeof(CV_out_buf));

    /* bias = mxmem(r8) */
    asm volatile("r8 = %0\n"
                 ".word 0x9208c3ff    /* bias=mxmem(r8) */\n"
                 :
                 : "r"(CV_bias_buf)
                 : "r8", "memory");

    /* activation.ub = mxmem(r2,r3) */
    asm volatile("r2 = %0\n"
                 "r3 = #0x7c\n"
                 ".word 0x9202c3ec    /* activation.ub=mxmem(r2,r3) */\n"
                 :
                 : "r"(CV_act_buf)
                 : "r2", "r3", "memory");

    /* weight.b = mxmem(r4,r5) -- triggers the FXP matrix multiply. */
    asm volatile("r4 = %0\n"
                 "r5 = #0xffff\n"
                 ".word 0x9204e5e0    /* weight.b=mxmem(r4,r5) */\n"
                 :
                 : "r"(CV_wei_buf)
                 : "r4", "r5", "memory");

    /*
     * cvt.uh2x2=acc(r9), r9=0x601: bit0=1 (retain), bit1=0 (relu=1,
     * i.e. sat=1), fb_dst=0, ch_sel=3 (bits[10:9]=11), bias_sel=0.
     * Its own packet, so HELPER(hmx_commit_packet) flushes the
     * pending convert before the next instruction runs.
     */
    asm volatile("r9 = ##0x601\n"
                 ".word 0xa6e9d910    /* cvt.uh2x2=acc(r9) */\n"
                 :
                 :
                 : "r9", "memory");

    /* mxmem(r6,r7):2x2=cvt -- reads the now-flushed convert result. */
    asm volatile("r6 = %0\n"
                 "r7 = #0\n"
                 ".word 0xa6e6c71a    /* mxmem(r6,r7):2x2=cvt */\n"
                 :
                 : "r"(CV_out_buf)
                 : "r6", "r7", "memory");

    /* Spatial-major offset: byte for (s,c) is at ((s>>2)<<7)|(c<<2)|(s&3). */
    for (int s = 0; s < 64; s++) {
        for (int o = 0; o < 32; o++) {
            int off = ((s >> 2) << 7) | (o << 2) | (s & 3);
            int expected = (s % 2 == 0 && o % 2 == 1) ? 30 : 0;
            check32(CV_out_buf[off], expected);
        }
    }

    puts(err ? "FAIL" : "PASS");
    return err ? 1 : 0;
}

/* hmx_matmul_fxp_limit */
#define L_ACT_SIZE  2048  /* one crouton: 64 spatial x 32 channels */
#define L_WEI_SIZE  1024  /* 8 vectors x 128 bytes: 32 channels, byte */
#define L_OUT_SIZE  2048

static uint8_t L_act_buf[L_ACT_SIZE] __attribute__((aligned(2048)));
static uint8_t L_wei_buf[L_WEI_SIZE] __attribute__((aligned(128)));
static uint32_t L_bias_buf[32] __attribute__((aligned(128)));
static uint8_t L_out_buf[L_OUT_SIZE] __attribute__((aligned(2048)));

/*
 * Every activation byte is 3, every weight byte is 5 -- same pattern
 * as hmx_matmul_fxp.c, which uses weight Rt=0xffff and sees all 8
 * weight vectors (32 input channels) contribute, giving accumulator
 * 480 (32 * 3*5) uniformly.
 *
 * Here weight Rt=0 instead: hmx_fxp_max_valid_vec() computes
 * max_valid_vec=0 for byte weights (cpv=4) with Rt=0, so only the
 * first weight vector (input channels 0-3) is valid -- channels 4-31
 * are skipped by HELPER(hmx_matmul_fxp)'s 'vec_idx > max_valid_vec'
 * check entirely, not just multiplied by a zero weight. Only 4
 * channels contribute: accumulator = 4 * 3*5 = 60 uniformly. The
 * identity bias register (exponent=20, scale=0x800, out_bias=0,
 * reused from every UB matmul test) holds for this value too
 * (verified natively): expected stored byte = 60 >> 4 = 3.
 */
#define L_EXPECTED_ACC     60
#define L_EXPECTED_BYTE    (L_EXPECTED_ACC >> 4)
#define L_BIAS_LOW32       0x5000u

static int hmx_test_hmx_matmul_fxp_limit(void)
{
    err = 0;
    clear_accumulators();
    memset(L_act_buf, 3, sizeof(L_act_buf));
    memset(L_wei_buf, 5, sizeof(L_wei_buf));
    for (int i = 0; i < 32; i++) {
        L_bias_buf[i] = L_BIAS_LOW32;
    }
    memset(L_out_buf, 0xff, sizeof(L_out_buf));

    /* bias = mxmem(r8) */
    asm volatile("r8 = %0\n"
                 ".word 0x9208c3ff    /* bias=mxmem(r8) */\n"
                 :
                 : "r"(L_bias_buf)
                 : "r8", "memory");

    /* activation.ub = mxmem(r2,r3) */
    asm volatile("r2 = %0\n"
                 "r3 = #0x7c\n"
                 ".word 0x9202c3ec    /* activation.ub=mxmem(r2,r3) */\n"
                 :
                 : "r"(L_act_buf)
                 : "r2", "r3", "memory");

    /*
     * weight.b = mxmem(r4,r5), r5=0 (not the usual 0xffff): clamps
     * the valid weight-vector range to just the first vector.
     */
    asm volatile("r4 = %0\n"
                 "r5 = #0\n"
                 ".word 0x9204e5e0    /* weight.b=mxmem(r4,r5) */\n"
                 :
                 : "r"(L_wei_buf)
                 : "r4", "r5", "memory");

    /* mxmem(r6,r7):after:sat.ub=acc */
    asm volatile("r6 = %0\n"
                 "r7 = #0\n"
                 ".word 0xa6e6c704    "
                 "/* mxmem(r6,r7):after:sat.ub=acc */\n"
                 :
                 : "r"(L_out_buf)
                 : "r6", "r7", "memory");

    for (int i = 0; i < L_OUT_SIZE; i++) {
        check32(L_out_buf[i], L_EXPECTED_BYTE);
    }

    puts(err ? "FAIL" : "PASS");
    return err ? 1 : 0;
}

int main(void)
{
    if (hmx_test_hmx_bias()) {
        return 1;
    }
    if (hmx_test_hmx_matmul_fxp()) {
        return 1;
    }
    if (hmx_test_hmx_matmul_fxp_nibble()) {
        return 1;
    }
    if (hmx_test_hmx_matmul_fxp_dm()) {
        return 1;
    }
    if (hmx_test_hmx_matmul_fxp_signed_crumb()) {
        return 1;
    }
    if (hmx_test_hmx_matmul_fxp_sign_magnitude()) {
        return 1;
    }
    if (hmx_test_hmx_cvt_rs()) {
        return 1;
    }
    if (hmx_test_hmx_matmul_fxp_bit()) {
        return 1;
    }
    if (hmx_test_hmx_matmul_fxp_signed_bit()) {
        return 1;
    }
    if (hmx_test_hmx_matmul_fxp_uh()) {
        return 1;
    }
    if (hmx_test_hmx_matmul_fxp_uh2x2()) {
        return 1;
    }
    if (hmx_test_hmx_cvt_rs_uh2x2()) {
        return 1;
    }
    if (hmx_test_hmx_matmul_fxp_limit()) {
        return 1;
    }
    return 0;
}
