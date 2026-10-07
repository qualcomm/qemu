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
    asm volatile(".word 0xa6e0c013    /* mxclracc.hf */" : : : "memory");
    /* Initialize both halves of the F8 convert-result slot to zero. */
    asm volatile("r9 = #1\n"
                 ".word 0xa6e9db10    /* cvt.f8=acc(r9) */\n"
                 :
                 :
                 : "r9", "memory");
    asm volatile("r9 = #0x801\n"
                 ".word 0xa6e9db10    /* cvt.f8=acc(r9) */\n"
                 :
                 :
                 : "r9", "memory");
}

/* hmx_matmul_fp_hf */
#define H_ACT_SIZE  1024  /* one crouton, as uint16_t: 2048 bytes / 2 */
#define H_WEI_SIZE  1024  /* 16 vectors x 128 bytes, as uint16_t */
#define H_OUT_SIZE  2048

static uint16_t H_act_buf[H_ACT_SIZE] __attribute__((aligned(2048)));
static uint16_t H_wei_buf[H_WEI_SIZE] __attribute__((aligned(128)));
static uint32_t H_bias_buf[32] __attribute__((aligned(128)));
static uint8_t H_out_buf[H_OUT_SIZE] __attribute__((aligned(2048)));

#define H_FP16_ONE    0x3C00u
#define H_BIAS_LOW32  H_FP16_ONE   /* scale=1.0, everything else 0 */
#define H_EXPECTED_LO 0x00u
#define H_EXPECTED_HI 0x50u      /* 32.0 as FP16 = 0x5000 */

static int hmx_test_hmx_matmul_fp_hf(void)
{
    err = 0;
    clear_accumulators();
    for (int i = 0; i < H_ACT_SIZE; i++) {
        H_act_buf[i] = H_FP16_ONE;
    }
    for (int i = 0; i < H_WEI_SIZE; i++) {
        H_wei_buf[i] = H_FP16_ONE;
    }
    for (int i = 0; i < 32; i++) {
        H_bias_buf[i] = H_BIAS_LOW32;
    }
    memset(H_out_buf, 0xff, sizeof(H_out_buf));

    /* bias = mxmem(r8) */
    asm volatile("r8 = %0\n"
                 ".word 0x9208c3ff    /* bias=mxmem(r8) */\n"
                 :
                 : "r"(H_bias_buf)
                 : "r8", "memory");

    /* activation.hf = mxmem(r2,r3) */
    asm volatile("r2 = %0\n"
                 "r3 = #0x7c\n"
                 ".word 0x9202c3e4    /* activation.hf=mxmem(r2,r3) */\n"
                 :
                 : "r"(H_act_buf)
                 : "r2", "r3", "memory");

    /* weight.hf = mxmem(r4,r5) -- triggers the FP matrix multiply. */
    asm volatile("r4 = %0\n"
                 "r5 = #0xffff\n"
                 ".word 0x9204e5ef    /* weight.hf=mxmem(r4,r5) */\n"
                 :
                 : "r"(H_wei_buf)
                 : "r4", "r5", "memory");

    /*
     * cvt.hf=acc(r9), r9=0 (acc_clear/relu/fb_dst/fb_limit/maxnorm/
     * bias_sel all zero -- bias_sel=0 matches the bias load above).
     * Its own packet, so HELPER(hmx_commit_packet) flushes the pending
     * convert at the end of this instruction before the next one runs.
     */
    asm volatile("r9 = #0\n"
                 ".word 0xa6e9da10    /* cvt.hf=acc(r9) */\n"
                 :
                 :
                 : "r9", "memory");

    /* mxmem(r6,r7)=cvt -- reads the now-flushed convert result. */
    asm volatile("r6 = %0\n"
                 "r7 = #0\n"
                 ".word 0xa6e6c718    /* mxmem(r6,r7)=cvt */\n"
                 :
                 : "r"(H_out_buf)
                 : "r6", "r7", "memory");

    for (int i = 0; i < H_OUT_SIZE; i += 2) {
        check32(H_out_buf[i], H_EXPECTED_LO);
        check32(H_out_buf[i + 1], H_EXPECTED_HI);
    }

    puts(err ? "FAIL" : "PASS");
    return err ? 1 : 0;
}

/* hmx_matmul_fp_f8 */
#define F_ACT_SIZE  1024  /* one crouton, as uint16_t: 2048 bytes / 2 */
#define F_WEI_SIZE  1024  /* 8 vectors x 128 bytes, F8: 4 channels/word */
#define F_OUT_SIZE  2048

static uint16_t F_act_buf[F_ACT_SIZE] __attribute__((aligned(2048)));
static uint8_t F_wei_buf[F_WEI_SIZE] __attribute__((aligned(128)));
static uint32_t F_bias_buf[32] __attribute__((aligned(128)));
static uint8_t F_out_buf[F_OUT_SIZE] __attribute__((aligned(2048)));

#define F_FP16_ONE    0x3C00u
#define F_F8_ONE      0x78u
#define F_BIAS_LOW32  F_FP16_ONE   /* scale=1.0, everything else 0 */
#define F_EXPECTED_LO 0x00u
#define F_EXPECTED_HI 0x50u      /* 32.0 as FP16 = 0x5000 */

static int hmx_test_hmx_matmul_fp_f8(void)
{
    err = 0;
    clear_accumulators();
    for (int i = 0; i < F_ACT_SIZE; i++) {
        F_act_buf[i] = F_FP16_ONE;
    }
    memset(F_wei_buf, F_F8_ONE, sizeof(F_wei_buf));
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

    /* activation.hf = mxmem(r2,r3) */
    asm volatile("r2 = %0\n"
                 "r3 = #0x7c\n"
                 ".word 0x9202c3e4    /* activation.hf=mxmem(r2,r3) */\n"
                 :
                 : "r"(F_act_buf)
                 : "r2", "r3", "memory");

    /* weight.f8 = mxmem(r4,r5) -- triggers the FP matrix multiply. */
    asm volatile("r4 = %0\n"
                 "r5 = #0xffff\n"
                 ".word 0x9204e547    /* weight.f8=mxmem(r4,r5) */\n"
                 :
                 : "r"(F_wei_buf)
                 : "r4", "r5", "memory");

    /*
     * cvt.hf=acc(r9), r9=0 (see hmx_matmul_fp_hf.c for the bit-field
     * derivation).
     */
    asm volatile("r9 = #0\n"
                 ".word 0xa6e9da10    /* cvt.hf=acc(r9) */\n"
                 :
                 :
                 : "r9", "memory");

    /* mxmem(r6,r7)=cvt -- reads the now-flushed convert result. */
    asm volatile("r6 = %0\n"
                 "r7 = #0\n"
                 ".word 0xa6e6c718    /* mxmem(r6,r7)=cvt */\n"
                 :
                 : "r"(F_out_buf)
                 : "r6", "r7", "memory");

    for (int i = 0; i < F_OUT_SIZE; i += 2) {
        check32(F_out_buf[i], F_EXPECTED_LO);
        check32(F_out_buf[i + 1], F_EXPECTED_HI);
    }

    puts(err ? "FAIL" : "PASS");
    return err ? 1 : 0;
}

/* hmx_matmul_fp_bf16 */
#define B_ACT_SIZE  1024  /* one crouton, as uint16_t: 2048 bytes / 2 */
#define B_WEI_SIZE  1024  /* 16 vectors x 128 bytes, as uint16_t */
#define B_OUT_SIZE  2048

static uint16_t B_act_buf[B_ACT_SIZE] __attribute__((aligned(2048)));
static uint16_t B_wei_buf[B_WEI_SIZE] __attribute__((aligned(128)));
static uint32_t B_bias_buf[32] __attribute__((aligned(128)));
static uint8_t B_out_buf[B_OUT_SIZE] __attribute__((aligned(2048)));

#define B_BF16_ONE    0x3F80u
#define B_FP16_ONE    0x3C00u
#define B_BIAS_LOW32  B_FP16_ONE   /* scale=1.0 (FP16 convert), rest 0 */
#define B_EXPECTED_LO 0x00u
#define B_EXPECTED_HI 0x50u      /* 32.0 as FP16 = 0x5000 */

static int hmx_test_hmx_matmul_fp_bf16(void)
{
    err = 0;
    clear_accumulators();
    for (int i = 0; i < B_ACT_SIZE; i++) {
        B_act_buf[i] = B_BF16_ONE;
    }
    for (int i = 0; i < B_WEI_SIZE; i++) {
        B_wei_buf[i] = B_BF16_ONE;
    }
    for (int i = 0; i < 32; i++) {
        B_bias_buf[i] = B_BIAS_LOW32;
    }
    memset(B_out_buf, 0xff, sizeof(B_out_buf));

    /* bias = mxmem(r8) */
    asm volatile("r8 = %0\n"
                 ".word 0x9208c3ff    /* bias=mxmem(r8) */\n"
                 :
                 : "r"(B_bias_buf)
                 : "r8", "memory");

    /*
     * activation.hf = mxmem(r2,r3) -- buffer holds BF16 values, but
     * the load itself is type-agnostic (raw 16-bit copy);
     * interpretation as BF16 happens later, gated by the weight-load's
     * Rs[6].
     */
    asm volatile("r2 = %0\n"
                 "r3 = #0x7c\n"
                 ".word 0x9202c3e4    /* activation.hf=mxmem(r2,r3) */\n"
                 :
                 : "r"(B_act_buf)
                 : "r2", "r3", "memory");

    /*
     * weight.hf = mxmem(r4,r5) -- Rs[6]=1 selects BF16 for both weight
     * and activation decode in the matmul this triggers.
     */
    asm volatile("r4 = %0\n"
                 "r4 = or(r4, #64)\n"
                 "r5 = #0xffff\n"
                 ".word 0x9204e5ef    /* weight.hf=mxmem(r4,r5) */\n"
                 :
                 : "r"(B_wei_buf)
                 : "r4", "r5", "memory");

    /*
     * cvt.hf=acc(r9), r9=0 (see hmx_matmul_fp_hf.c for the bit-field
     * derivation).
     */
    asm volatile("r9 = #0\n"
                 ".word 0xa6e9da10    /* cvt.hf=acc(r9) */\n"
                 :
                 :
                 : "r9", "memory");

    /* mxmem(r6,r7)=cvt -- reads the now-flushed convert result. */
    asm volatile("r6 = %0\n"
                 "r7 = #0\n"
                 ".word 0xa6e6c718    /* mxmem(r6,r7)=cvt */\n"
                 :
                 : "r"(B_out_buf)
                 : "r6", "r7", "memory");

    for (int i = 0; i < B_OUT_SIZE; i += 2) {
        check32(B_out_buf[i], B_EXPECTED_LO);
        check32(B_out_buf[i + 1], B_EXPECTED_HI);
    }

    puts(err ? "FAIL" : "PASS");
    return err ? 1 : 0;
}

/* hmx_cvt_rs_bf16 */
#define CB_ACT_SIZE  1024  /* one crouton, as uint16_t: 2048 bytes / 2 */
#define CB_WEI_SIZE  1024  /* 16 vectors x 128 bytes, as uint16_t */
#define CB_OUT_SIZE  2048

static uint16_t CB_act_buf[CB_ACT_SIZE] __attribute__((aligned(2048)));
static uint16_t CB_wei_buf[CB_WEI_SIZE] __attribute__((aligned(128)));
static uint32_t CB_bias_buf[32] __attribute__((aligned(128)));
static uint8_t CB_out_buf[CB_OUT_SIZE] __attribute__((aligned(2048)));

#define CB_FP16_ONE    0x3C00u
#define CB_BF16_ONE    0x3F80u
/* scale=1.0 BF16-encoded (see file comment), everything else 0. */
#define CB_BIAS_LOW32  CB_BF16_ONE
#define CB_EXPECTED_LO 0x00u
#define CB_EXPECTED_HI 0x42u      /* 32.0 as BF16 = 0x4200 */

static int hmx_test_hmx_cvt_rs_bf16(void)
{
    err = 0;
    clear_accumulators();
    for (int i = 0; i < CB_ACT_SIZE; i++) {
        CB_act_buf[i] = CB_FP16_ONE;
    }
    for (int i = 0; i < CB_WEI_SIZE; i++) {
        CB_wei_buf[i] = CB_FP16_ONE;
    }
    for (int i = 0; i < 32; i++) {
        CB_bias_buf[i] = CB_BIAS_LOW32;
    }
    memset(CB_out_buf, 0xff, sizeof(CB_out_buf));

    /* bias = mxmem(r8) */
    asm volatile("r8 = %0\n"
                 ".word 0x9208c3ff    /* bias=mxmem(r8) */\n"
                 :
                 : "r"(CB_bias_buf)
                 : "r8", "memory");

    /* activation.hf = mxmem(r2,r3) */
    asm volatile("r2 = %0\n"
                 "r3 = #0x7c\n"
                 ".word 0x9202c3e4    /* activation.hf=mxmem(r2,r3) */\n"
                 :
                 : "r"(CB_act_buf)
                 : "r2", "r3", "memory");

    /* weight.hf = mxmem(r4,r5) -- triggers the FP matrix multiply. */
    asm volatile("r4 = %0\n"
                 "r5 = #0xffff\n"
                 ".word 0x9204e5ef    /* weight.hf=mxmem(r4,r5) */\n"
                 :
                 : "r"(CB_wei_buf)
                 : "r4", "r5", "memory");

    /*
     * cvt.hf=acc(r9), r9=0x80 (Rs[7]=1: is_bf16_out; everything else
     * 0 -- see hmx_matmul_fp_hf.c for the other bit-field derivations).
     */
    asm volatile("r9 = #0x80\n"
                 ".word 0xa6e9da10    /* cvt.hf=acc(r9) */\n"
                 :
                 :
                 : "r9", "memory");

    /* mxmem(r6,r7)=cvt -- reads the now-flushed convert result. */
    asm volatile("r6 = %0\n"
                 "r7 = #0\n"
                 ".word 0xa6e6c718    /* mxmem(r6,r7)=cvt */\n"
                 :
                 : "r"(CB_out_buf)
                 : "r6", "r7", "memory");

    for (int i = 0; i < CB_OUT_SIZE; i += 2) {
        check32(CB_out_buf[i], CB_EXPECTED_LO);
        check32(CB_out_buf[i + 1], CB_EXPECTED_HI);
    }

    puts(err ? "FAIL" : "PASS");
    return err ? 1 : 0;
}

/* hmx_cvt_rs_f8 */
#define CF_ACT_SIZE  1024  /* one crouton, as uint16_t: 2048 bytes / 2 */
#define CF_WEI_SIZE  1024  /* 16 vectors x 128 bytes, as uint16_t */
#define CF_OUT_SIZE  2048

static uint16_t CF_act_buf[CF_ACT_SIZE] __attribute__((aligned(2048)));
static uint16_t CF_wei_buf[CF_WEI_SIZE] __attribute__((aligned(128)));
static uint32_t CF_bias_buf[32] __attribute__((aligned(128)));
static uint8_t CF_out_buf[CF_OUT_SIZE] __attribute__((aligned(2048)));

#define CF_FP16_ONE    0x3C00u
#define CF_BIAS_LOW32  CF_FP16_ONE   /* scale=1.0, everything else 0 */
#define CF_EXPECTED_EVEN 0x60u    /* 32.0 as F8 (e4m3) */
#define CF_EXPECTED_ODD  0x00u    /* untouched half (fp8_odd_sel=0) */

static int hmx_test_hmx_cvt_rs_f8(void)
{
    err = 0;
    clear_accumulators();
    for (int i = 0; i < CF_ACT_SIZE; i++) {
        CF_act_buf[i] = CF_FP16_ONE;
    }
    for (int i = 0; i < CF_WEI_SIZE; i++) {
        CF_wei_buf[i] = CF_FP16_ONE;
    }
    for (int i = 0; i < 32; i++) {
        CF_bias_buf[i] = CF_BIAS_LOW32;
    }
    memset(CF_out_buf, 0xff, sizeof(CF_out_buf));

    /* bias = mxmem(r8) */
    asm volatile("r8 = %0\n"
                 ".word 0x9208c3ff    /* bias=mxmem(r8) */\n"
                 :
                 : "r"(CF_bias_buf)
                 : "r8", "memory");

    /* activation.hf = mxmem(r2,r3) */
    asm volatile("r2 = %0\n"
                 "r3 = #0x7c\n"
                 ".word 0x9202c3e4    /* activation.hf=mxmem(r2,r3) */\n"
                 :
                 : "r"(CF_act_buf)
                 : "r2", "r3", "memory");

    /* weight.hf = mxmem(r4,r5) -- triggers the FP matrix multiply. */
    asm volatile("r4 = %0\n"
                 "r5 = #0xffff\n"
                 ".word 0x9204e5ef    /* weight.hf=mxmem(r4,r5) */\n"
                 :
                 : "r"(CF_wei_buf)
                 : "r4", "r5", "memory");

    /*
     * cvt.f8=acc(r9), r9=0 (acc_clear/relu/fb_dst/maxnorm/fp8_odd_sel/
     * bias_sel all zero -- fp8_odd_sel=0 selects the even half).
     */
    asm volatile("r9 = #0\n"
                 ".word 0xa6e9db10    /* cvt.f8=acc(r9) */\n"
                 :
                 :
                 : "r9", "memory");

    /* mxmem(r6,r7).f8=cvt -- reads the F8 convert result. */
    asm volatile("r6 = %0\n"
                 "r7 = #0\n"
                 ".word 0xa6e6c71d    /* mxmem(r6,r7).f8=cvt */\n"
                 :
                 : "r"(CF_out_buf)
                 : "r6", "r7", "memory");

    for (int i = 0; i < CF_OUT_SIZE; i += 2) {
        check32(CF_out_buf[i], CF_EXPECTED_EVEN);
        check32(CF_out_buf[i + 1], CF_EXPECTED_ODD);
    }

    puts(err ? "FAIL" : "PASS");
    return err ? 1 : 0;
}

/* hmx_matmul_fp_bf16_cvt */
#define BC_ACT_SIZE  1024  /* one crouton, as uint16_t: 2048 bytes / 2 */
#define BC_WEI_SIZE  1024  /* 16 vectors x 128 bytes, as uint16_t */
#define BC_OUT_SIZE  2048

static uint16_t BC_act_buf[BC_ACT_SIZE] __attribute__((aligned(2048)));
static uint16_t BC_wei_buf[BC_WEI_SIZE] __attribute__((aligned(128)));
static uint32_t BC_bias_buf[32] __attribute__((aligned(128)));
static uint8_t BC_out_buf[BC_OUT_SIZE] __attribute__((aligned(2048)));

#define BC_BF16_ONE    0x3F80u
/* scale=1.0, BF16-encoded (see file comment). */
#define BC_BIAS_LOW32  BC_BF16_ONE
#define BC_EXPECTED_LO 0x00u
#define BC_EXPECTED_HI 0x42u      /* 32.0 as BF16 = 0x4200 */

static int hmx_test_hmx_matmul_fp_bf16_cvt(void)
{
    err = 0;
    clear_accumulators();
    for (int i = 0; i < BC_ACT_SIZE; i++) {
        BC_act_buf[i] = BC_BF16_ONE;
    }
    for (int i = 0; i < BC_WEI_SIZE; i++) {
        BC_wei_buf[i] = BC_BF16_ONE;
    }
    for (int i = 0; i < 32; i++) {
        BC_bias_buf[i] = BC_BIAS_LOW32;
    }
    memset(BC_out_buf, 0xff, sizeof(BC_out_buf));

    /* bias = mxmem(r8) */
    asm volatile("r8 = %0\n"
                 ".word 0x9208c3ff    /* bias=mxmem(r8) */\n"
                 :
                 : "r"(BC_bias_buf)
                 : "r8", "memory");

    /*
     * activation.hf = mxmem(r2,r3) -- buffer holds BF16 values, see
     * hmx_matmul_fp_bf16.c.
     */
    asm volatile("r2 = %0\n"
                 "r3 = #0x7c\n"
                 ".word 0x9202c3e4    /* activation.hf=mxmem(r2,r3) */\n"
                 :
                 : "r"(BC_act_buf)
                 : "r2", "r3", "memory");

    /* weight.hf = mxmem(r4,r5) -- Rs[6]=1 selects BF16 matmul input. */
    asm volatile("r4 = %0\n"
                 "r4 = or(r4, #64)\n"
                 "r5 = #0xffff\n"
                 ".word 0x9204e5ef    /* weight.hf=mxmem(r4,r5) */\n"
                 :
                 : "r"(BC_wei_buf)
                 : "r4", "r5", "memory");

    /* cvt.hf=acc(r9), r9=0x80 -- Rs[7]=1 selects BF16 convert output. */
    asm volatile("r9 = #0x80\n"
                 ".word 0xa6e9da10    /* cvt.hf=acc(r9) */\n"
                 :
                 :
                 : "r9", "memory");

    /* mxmem(r6,r7)=cvt -- reads the now-flushed convert result. */
    asm volatile("r6 = %0\n"
                 "r7 = #0\n"
                 ".word 0xa6e6c718    /* mxmem(r6,r7)=cvt */\n"
                 :
                 : "r"(BC_out_buf)
                 : "r6", "r7", "memory");

    for (int i = 0; i < BC_OUT_SIZE; i += 2) {
        check32(BC_out_buf[i], BC_EXPECTED_LO);
        check32(BC_out_buf[i + 1], BC_EXPECTED_HI);
    }

    puts(err ? "FAIL" : "PASS");
    return err ? 1 : 0;
}

/* hmx_cvt_rs_f8_both_halves */
#define BH_ACT_SIZE  1024  /* one crouton, as uint16_t: 2048 bytes / 2 */
#define BH_WEI_SIZE  1024  /* 16 vectors x 128 bytes, as uint16_t */
#define BH_OUT_SIZE  2048

static uint16_t BH_act_buf[BH_ACT_SIZE] __attribute__((aligned(2048)));
static uint16_t BH_wei_buf[BH_WEI_SIZE] __attribute__((aligned(128)));
static uint32_t BH_bias_buf[32] __attribute__((aligned(128)));
static uint8_t BH_out_buf[BH_OUT_SIZE] __attribute__((aligned(2048)));

#define BH_FP16_ONE    0x3C00u
#define BH_BIAS_LOW32  BH_FP16_ONE   /* scale=1.0, everything else 0 */
#define BH_EXPECTED    0x60u      /* 32.0 as F8 (e4m3), both halves */

static int hmx_test_hmx_cvt_rs_f8_both_halves(void)
{
    err = 0;
    clear_accumulators();
    for (int i = 0; i < BH_ACT_SIZE; i++) {
        BH_act_buf[i] = BH_FP16_ONE;
    }
    for (int i = 0; i < BH_WEI_SIZE; i++) {
        BH_wei_buf[i] = BH_FP16_ONE;
    }
    for (int i = 0; i < 32; i++) {
        BH_bias_buf[i] = BH_BIAS_LOW32;
    }
    memset(BH_out_buf, 0xff, sizeof(BH_out_buf));

    /* bias = mxmem(r8) */
    asm volatile("r8 = %0\n"
                 ".word 0x9208c3ff    /* bias=mxmem(r8) */\n"
                 :
                 : "r"(BH_bias_buf)
                 : "r8", "memory");

    /* activation.hf = mxmem(r2,r3) */
    asm volatile("r2 = %0\n"
                 "r3 = #0x7c\n"
                 ".word 0x9202c3e4    /* activation.hf=mxmem(r2,r3) */\n"
                 :
                 : "r"(BH_act_buf)
                 : "r2", "r3", "memory");

    /* weight.hf = mxmem(r4,r5) -- triggers the FP matrix multiply. */
    asm volatile("r4 = %0\n"
                 "r5 = #0xffff\n"
                 ".word 0x9204e5ef    /* weight.hf=mxmem(r4,r5) */\n"
                 :
                 : "r"(BH_wei_buf)
                 : "r4", "r5", "memory");

    /*
     * cvt.f8=acc(r9), r9=1 -- fp8_odd_sel=0: writes the even half.
     * Rs[0]=1 (retain) is essential here: Rs[0]=0 would mark the
     * accumulator for a deferred clear-and-flip-current_acc_set at
     * this packet's boundary (HELPER(hmx_commit_packet)), which would
     * make the second cvt.f8 below read from the *other*,
     * never-populated accumulator set instead of the matmul's result
     * (an earlier version of this test used r9=0 here and got exactly
     * that: every other byte read back 0x00 -- accumulator 0.0's F8
     * encoding -- instead of 0x60, a test bug caught by comparing
     * against the real deferred-clear semantics, not an
     * implementation bug).
     */
    asm volatile("r9 = #1\n"
                 ".word 0xa6e9db10    /* cvt.f8=acc(r9) */\n"
                 :
                 :
                 : "r9", "memory");

    /*
     * cvt.f8=acc(r9), r9=0x800 (Rs[11]=1) -- fp8_odd_sel=1: writes the
     * odd half, preserving the even half just written above.
     */
    asm volatile("r9 = #0x800\n"
                 ".word 0xa6e9db10    /* cvt.f8=acc(r9) */\n"
                 :
                 :
                 : "r9", "memory");

    /* mxmem(r6,r7).f8=cvt -- reads the F8 convert result. */
    asm volatile("r6 = %0\n"
                 "r7 = #0\n"
                 ".word 0xa6e6c71d    /* mxmem(r6,r7).f8=cvt */\n"
                 :
                 : "r"(BH_out_buf)
                 : "r6", "r7", "memory");

    for (int i = 0; i < BH_OUT_SIZE; i++) {
        check32(BH_out_buf[i], BH_EXPECTED);
    }

    puts(err ? "FAIL" : "PASS");
    return err ? 1 : 0;
}

/* hmx_cvt_rs_negate_outbias */
#define NO_ACT_SIZE  1024  /* one crouton, as uint16_t: 2048 bytes / 2 */
#define NO_WEI_SIZE  1024  /* 16 vectors x 128 bytes, as uint16_t */
#define NO_OUT_SIZE  2048

static uint16_t NO_act_buf[NO_ACT_SIZE] __attribute__((aligned(2048)));
static uint16_t NO_wei_buf[NO_WEI_SIZE] __attribute__((aligned(128)));
/* bias.mxmem2 layout: 32 low words, then 32 high words at +128. */
static uint32_t NO_bias_buf[64] __attribute__((aligned(256)));
static uint8_t NO_out_buf[NO_OUT_SIZE] __attribute__((aligned(2048)));

#define NO_FP16_ONE      0x3C00u
#define NO_OUT_BIAS_FP16 0x5640u  /* 100.0 */
#define NO_NEGATE_BIT42  0x400u   /* bit 42 overall = bit 10 of hi word */
#define NO_EXPECTED_LO   0x40u
#define NO_EXPECTED_HI   0x54u    /* 68.0 as FP16 = 0x5440 */

static int hmx_test_hmx_cvt_rs_negate_outbias(void)
{
    err = 0;
    clear_accumulators();
    for (int i = 0; i < NO_ACT_SIZE; i++) {
        NO_act_buf[i] = NO_FP16_ONE;
    }
    for (int i = 0; i < NO_WEI_SIZE; i++) {
        NO_wei_buf[i] = NO_FP16_ONE;
    }
    for (int i = 0; i < 32; i++) {
        NO_bias_buf[i] = ((uint32_t)NO_OUT_BIAS_FP16 << 16) | NO_FP16_ONE;
        NO_bias_buf[32 + i] = NO_NEGATE_BIT42;
    }
    memset(NO_out_buf, 0xff, sizeof(NO_out_buf));

    /*
     * bias = mxmem2(r8) -- wide bias load, needed to reach negate
     * (bit 42, beyond the low-32-bit-only bias.mxmem every prior test
     * used).
     */
    asm volatile("r8 = %0\n"
                 ".word 0x9208c3fe    /* bias=mxmem2(r8) */\n"
                 :
                 : "r"(NO_bias_buf)
                 : "r8", "memory");

    /* activation.hf = mxmem(r2,r3) */
    asm volatile("r2 = %0\n"
                 "r3 = #0x7c\n"
                 ".word 0x9202c3e4    /* activation.hf=mxmem(r2,r3) */\n"
                 :
                 : "r"(NO_act_buf)
                 : "r2", "r3", "memory");

    /* weight.hf = mxmem(r4,r5) -- triggers the FP matrix multiply. */
    asm volatile("r4 = %0\n"
                 "r5 = #0xffff\n"
                 ".word 0x9204e5ef    /* weight.hf=mxmem(r4,r5) */\n"
                 :
                 : "r"(NO_wei_buf)
                 : "r4", "r5", "memory");

    /*
     * cvt.hf=acc(r9), r9=0 (see hmx_matmul_fp_hf.c for the bit-field
     * derivation).
     */
    asm volatile("r9 = #0\n"
                 ".word 0xa6e9da10    /* cvt.hf=acc(r9) */\n"
                 :
                 :
                 : "r9", "memory");

    /* mxmem(r6,r7)=cvt -- reads the now-flushed convert result. */
    asm volatile("r6 = %0\n"
                 "r7 = #0\n"
                 ".word 0xa6e6c718    /* mxmem(r6,r7)=cvt */\n"
                 :
                 : "r"(NO_out_buf)
                 : "r6", "r7", "memory");

    for (int i = 0; i < NO_OUT_SIZE; i += 2) {
        check32(NO_out_buf[i], NO_EXPECTED_LO);
        check32(NO_out_buf[i + 1], NO_EXPECTED_HI);
    }

    puts(err ? "FAIL" : "PASS");
    return err ? 1 : 0;
}

/* hmx_cvt_rs_shape_clamp */
#define SH_ACT_SIZE  1024  /* one crouton, as uint16_t: 2048 bytes / 2 */
#define SH_WEI_SIZE  1024  /* 16 vectors x 128 bytes, as uint16_t */
#define SH_OUT_SIZE  2048

static uint16_t SH_act_buf[SH_ACT_SIZE] __attribute__((aligned(2048)));
static uint16_t SH_wei_buf[SH_WEI_SIZE] __attribute__((aligned(128)));
/* bias.mxmem2 layout: 32 low words, then 32 high words at +128. */
static uint32_t SH_bias_buf[64] __attribute__((aligned(256)));
static uint8_t SH_out_buf[SH_OUT_SIZE] __attribute__((aligned(2048)));

#define SH_FP16_ONE   0x3C00u
#define SH_SHAPE_MIN  0x100u      /* shape=1 (min clamp) at bits[41:40] */
#define SH_EXPECTED   0x00u       /* 0.0 as FP16 = 0x0000, both bytes */

static int hmx_test_hmx_cvt_rs_shape_clamp(void)
{
    err = 0;
    clear_accumulators();
    for (int i = 0; i < SH_ACT_SIZE; i++) {
        SH_act_buf[i] = SH_FP16_ONE;
    }
    for (int i = 0; i < SH_WEI_SIZE; i++) {
        SH_wei_buf[i] = SH_FP16_ONE;
    }
    for (int i = 0; i < 32; i++) {
        SH_bias_buf[i] = SH_FP16_ONE;         /* scale=1.0, out_bias=0 */
        SH_bias_buf[32 + i] = SH_SHAPE_MIN;
    }
    memset(SH_out_buf, 0xff, sizeof(SH_out_buf));

    /*
     * bias = mxmem2(r8) -- wide bias load, needed to reach shape
     * (bits[41:40], beyond the low-32-bit-only bias.mxmem every prior
     * FP matmul test used).
     */
    asm volatile("r8 = %0\n"
                 ".word 0x9208c3fe    /* bias=mxmem2(r8) */\n"
                 :
                 : "r"(SH_bias_buf)
                 : "r8", "memory");

    /* activation.hf = mxmem(r2,r3) */
    asm volatile("r2 = %0\n"
                 "r3 = #0x7c\n"
                 ".word 0x9202c3e4    /* activation.hf=mxmem(r2,r3) */\n"
                 :
                 : "r"(SH_act_buf)
                 : "r2", "r3", "memory");

    /* weight.hf = mxmem(r4,r5) -- triggers the FP matrix multiply. */
    asm volatile("r4 = %0\n"
                 "r5 = #0xffff\n"
                 ".word 0x9204e5ef    /* weight.hf=mxmem(r4,r5) */\n"
                 :
                 : "r"(SH_wei_buf)
                 : "r4", "r5", "memory");

    /*
     * cvt.hf=acc(r9), r9=0 (see hmx_matmul_fp_hf.c for the bit-field
     * derivation).
     */
    asm volatile("r9 = #0\n"
                 ".word 0xa6e9da10    /* cvt.hf=acc(r9) */\n"
                 :
                 :
                 : "r9", "memory");

    /* mxmem(r6,r7)=cvt -- reads the now-flushed convert result. */
    asm volatile("r6 = %0\n"
                 "r7 = #0\n"
                 ".word 0xa6e6c718    /* mxmem(r6,r7)=cvt */\n"
                 :
                 : "r"(SH_out_buf)
                 : "r6", "r7", "memory");

    for (int i = 0; i < SH_OUT_SIZE; i++) {
        check32(SH_out_buf[i], SH_EXPECTED);
    }

    puts(err ? "FAIL" : "PASS");
    return err ? 1 : 0;
}

/* hmx_cvt_rs_acc_bias */
#define AB_ACT_SIZE  1024  /* one crouton, as uint16_t: 2048 bytes / 2 */
#define AB_WEI_SIZE  1024  /* 16 vectors x 128 bytes, as uint16_t */
#define AB_OUT_SIZE  2048

static uint16_t AB_act_buf[AB_ACT_SIZE] __attribute__((aligned(2048)));
static uint16_t AB_wei_buf[AB_WEI_SIZE] __attribute__((aligned(128)));
/* bias.mxmem2 layout: 32 low words, then 32 high words at +128. */
static uint32_t AB_bias_buf[64] __attribute__((aligned(256)));
static uint8_t AB_out_buf[AB_OUT_SIZE] __attribute__((aligned(2048)));

#define AB_FP16_ONE       0x3C00u
#define AB_ACC_BIAS_FP16  0xD000u  /* -32.0, at bits[63:48] -> high[31:16] */
#define AB_EXPECTED       0x00u    /* 0.0 as FP16 = 0x0000, both bytes */

static int hmx_test_hmx_cvt_rs_acc_bias(void)
{
    err = 0;
    clear_accumulators();
    for (int i = 0; i < AB_ACT_SIZE; i++) {
        AB_act_buf[i] = AB_FP16_ONE;
    }
    for (int i = 0; i < AB_WEI_SIZE; i++) {
        AB_wei_buf[i] = AB_FP16_ONE;
    }
    for (int i = 0; i < 32; i++) {
        AB_bias_buf[i] = AB_FP16_ONE; /* scale=1.0, out_bias=0 */
        AB_bias_buf[32 + i] = (uint32_t)AB_ACC_BIAS_FP16 << 16;
    }
    memset(AB_out_buf, 0xff, sizeof(AB_out_buf));

    /*
     * bias = mxmem2(r8) -- wide bias load, needed to reach acc_bias
     * (bits[63:48], beyond the low-32-bit-only bias.mxmem every prior
     * FP matmul test used).
     */
    asm volatile("r8 = %0\n"
                 ".word 0x9208c3fe    /* bias=mxmem2(r8) */\n"
                 :
                 : "r"(AB_bias_buf)
                 : "r8", "memory");

    /* activation.hf = mxmem(r2,r3) */
    asm volatile("r2 = %0\n"
                 "r3 = #0x7c\n"
                 ".word 0x9202c3e4    /* activation.hf=mxmem(r2,r3) */\n"
                 :
                 : "r"(AB_act_buf)
                 : "r2", "r3", "memory");

    /* weight.hf = mxmem(r4,r5) -- triggers the FP matrix multiply. */
    asm volatile("r4 = %0\n"
                 "r5 = #0xffff\n"
                 ".word 0x9204e5ef    /* weight.hf=mxmem(r4,r5) */\n"
                 :
                 : "r"(AB_wei_buf)
                 : "r4", "r5", "memory");

    /*
     * cvt.hf=acc(r9), r9=0 (see hmx_matmul_fp_hf.c for the bit-field
     * derivation).
     */
    asm volatile("r9 = #0\n"
                 ".word 0xa6e9da10    /* cvt.hf=acc(r9) */\n"
                 :
                 :
                 : "r9", "memory");

    /* mxmem(r6,r7)=cvt -- reads the now-flushed convert result. */
    asm volatile("r6 = %0\n"
                 "r7 = #0\n"
                 ".word 0xa6e6c718    /* mxmem(r6,r7)=cvt */\n"
                 :
                 : "r"(AB_out_buf)
                 : "r6", "r7", "memory");

    for (int i = 0; i < AB_OUT_SIZE; i++) {
        check32(AB_out_buf[i], AB_EXPECTED);
    }

    puts(err ? "FAIL" : "PASS");
    return err ? 1 : 0;
}

/* hmx_cvt_rs_fp16_overflow */
#define OV_ACT_SIZE  1024  /* one crouton, as uint16_t: 2048 bytes / 2 */
#define OV_WEI_SIZE  1024  /* 16 vectors x 128 bytes, as uint16_t */
#define OV_OUT_SIZE  2048

static uint16_t OV_act_buf[OV_ACT_SIZE] __attribute__((aligned(2048)));
static uint16_t OV_wei_buf[OV_WEI_SIZE] __attribute__((aligned(128)));
static uint32_t OV_bias_buf[32] __attribute__((aligned(128)));
static uint8_t OV_out_buf[OV_OUT_SIZE] __attribute__((aligned(2048)));

#define OV_FP16_ONE       0x3C00u
#define OV_SCALE_FP16_MAX 0x7BFFu  /* FP16 max finite, 65504.0 */
#define OV_EXPECTED_LO    0xFFu
#define OV_EXPECTED_HI    0x7Fu    /* XFP overflow result, see file comment */

static int hmx_test_hmx_cvt_rs_fp16_overflow(void)
{
    err = 0;
    clear_accumulators();
    for (int i = 0; i < OV_ACT_SIZE; i++) {
        OV_act_buf[i] = OV_FP16_ONE;
    }
    for (int i = 0; i < OV_WEI_SIZE; i++) {
        OV_wei_buf[i] = OV_FP16_ONE;
    }
    for (int i = 0; i < 32; i++) {
        OV_bias_buf[i] = OV_SCALE_FP16_MAX;   /* scale=65504.0, out_bias=0 */
    }
    memset(OV_out_buf, 0xff, sizeof(OV_out_buf));

    /* bias = mxmem(r8) */
    asm volatile("r8 = %0\n"
                 ".word 0x9208c3ff    /* bias=mxmem(r8) */\n"
                 :
                 : "r"(OV_bias_buf)
                 : "r8", "memory");

    /* activation.hf = mxmem(r2,r3) */
    asm volatile("r2 = %0\n"
                 "r3 = #0x7c\n"
                 ".word 0x9202c3e4    /* activation.hf=mxmem(r2,r3) */\n"
                 :
                 : "r"(OV_act_buf)
                 : "r2", "r3", "memory");

    /* weight.hf = mxmem(r4,r5) -- triggers the FP matrix multiply. */
    asm volatile("r4 = %0\n"
                 "r5 = #0xffff\n"
                 ".word 0x9204e5ef    /* weight.hf=mxmem(r4,r5) */\n"
                 :
                 : "r"(OV_wei_buf)
                 : "r4", "r5", "memory");

    /*
     * cvt.hf=acc(r9), r9=0x100 (Rs[8]=fp_rnd=1: normal rounded 16-bit
     * output -- the XFP convert path's overflow/max-value fill width
     * depends on this bit; maxnorm=0 too, Mode 0 fixup doesn't
     * consult it, see hmx_matmul_fp_hf.c for the other bit-field
     * derivations).
     */
    asm volatile("r9 = #0x100\n"
                 ".word 0xa6e9da10    /* cvt.hf=acc(r9) */\n"
                 :
                 :
                 : "r9", "memory");

    /* mxmem(r6,r7)=cvt -- reads the now-flushed convert result. */
    asm volatile("r6 = %0\n"
                 "r7 = #0\n"
                 ".word 0xa6e6c718    /* mxmem(r6,r7)=cvt */\n"
                 :
                 : "r"(OV_out_buf)
                 : "r6", "r7", "memory");

    for (int i = 0; i < OV_OUT_SIZE; i += 2) {
        check32(OV_out_buf[i], OV_EXPECTED_LO);
        check32(OV_out_buf[i + 1], OV_EXPECTED_HI);
    }

    puts(err ? "FAIL" : "PASS");
    return err ? 1 : 0;
}

/* hmx_matmul_fp_dp */
#define DP_ACT_SIZE  1024  /* one crouton, as uint16_t: 2048 bytes / 2 */
/*
 * 32 vectors x 128 bytes, as uint16_t -- double hmx_matmul_fp_hf.c's
 * 16. wgt_stream_idx is a function-level local in
 * hmx_matmul_fp_dbl(), not reset per deep_blk, so deep_blk=1's pass
 * continues incrementing it from where deep_blk=0 left off (32) and
 * reads vec_idx 16..31, not 0..15 again -- the buffer needs to cover
 * both halves.
 */
#define DP_WEI_SIZE  2048
#define DP_OUT_SIZE  2048

static uint16_t DP_act_buf[DP_ACT_SIZE] __attribute__((aligned(2048)));
static uint16_t DP_wei_buf[DP_WEI_SIZE] __attribute__((aligned(128)));
static uint32_t DP_bias_buf[32] __attribute__((aligned(128)));
static uint8_t DP_out_buf0[DP_OUT_SIZE] __attribute__((aligned(2048)));
static uint8_t DP_out_buf1[DP_OUT_SIZE] __attribute__((aligned(2048)));

#define DP_FP16_ONE    0x3C00u
#define DP_BIAS_LOW32  DP_FP16_ONE   /* scale=1.0, everything else 0 */
#define DP_EXPECTED_LO 0x00u
#define DP_EXPECTED_HI 0x50u      /* 32.0 as FP16 = 0x5000 */

static int hmx_test_hmx_matmul_fp_dp(void)
{
    err = 0;
    clear_accumulators();
    for (int i = 0; i < DP_ACT_SIZE; i++) {
        DP_act_buf[i] = DP_FP16_ONE;
    }
    for (int i = 0; i < DP_WEI_SIZE; i++) {
        DP_wei_buf[i] = DP_FP16_ONE;
    }
    for (int i = 0; i < 32; i++) {
        DP_bias_buf[i] = DP_BIAS_LOW32;
    }
    memset(DP_out_buf0, 0xff, sizeof(DP_out_buf0));
    memset(DP_out_buf1, 0xff, sizeof(DP_out_buf1));

    /* bias = mxmem(r8) */
    asm volatile("r8 = %0\n"
                 ".word 0x9208c3ff    /* bias=mxmem(r8) */\n"
                 :
                 : "r"(DP_bias_buf)
                 : "r8", "memory");

    /* activation.hf = mxmem(r2,r3) */
    asm volatile("r2 = %0\n"
                 "r3 = #0x7c\n"
                 ".word 0x9202c3e4    /* activation.hf=mxmem(r2,r3) */\n"
                 :
                 : "r"(DP_act_buf)
                 : "r2", "r3", "memory");

    /*
     * weight.hf = mxmemdp(r4,r5) -- DP modifier, triggers the deep FP
     * matrix multiply (writes both accumulator sets).
     */
    asm volatile("r4 = %0\n"
                 "r5 = #0xffff\n"
                 ".word 0x9204e5f4    /* weight.hf=mxmemdp(r4,r5) */\n"
                 :
                 : "r"(DP_wei_buf)
                 : "r4", "r5", "memory");

    /*
     * cvt.hf=acc(r9), r9=1 (retain -- keep both accumulator sets
     * intact for the second convert below).
     */
    asm volatile("r9 = #1\n"
                 ".word 0xa6e9da10    /* cvt.hf=acc(r9) */\n"
                 :
                 :
                 : "r9", "memory");

    /* mxmem(r6,r7)=cvt -- reads accumulator set 0's convert result. */
    asm volatile("r6 = %0\n"
                 "r7 = #0\n"
                 ".word 0xa6e6c718    /* mxmem(r6,r7)=cvt */\n"
                 :
                 : "r"(DP_out_buf0)
                 : "r6", "r7", "memory");

    /* mxswapacc.hf -- toggle current_acc_set (0 -> 1). */
    asm volatile(".word 0xa6e0c015    /* mxswapacc.hf */\n"
                 :
                 :
                 : "memory");

    /* cvt.hf=acc(r9), r9=1 (retain) -- convert accumulator set 1. */
    asm volatile("r9 = #1\n"
                 ".word 0xa6e9da10    /* cvt.hf=acc(r9) */\n"
                 :
                 :
                 : "r9", "memory");

    /* mxmem(r6,r7)=cvt -- reads accumulator set 1's convert result. */
    asm volatile("r6 = %0\n"
                 "r7 = #0\n"
                 ".word 0xa6e6c718    /* mxmem(r6,r7)=cvt */\n"
                 :
                 : "r"(DP_out_buf1)
                 : "r6", "r7", "memory");

    for (int i = 0; i < DP_OUT_SIZE; i += 2) {
        check32(DP_out_buf0[i], DP_EXPECTED_LO);
        check32(DP_out_buf0[i + 1], DP_EXPECTED_HI);
        check32(DP_out_buf1[i], DP_EXPECTED_LO);
        check32(DP_out_buf1[i + 1], DP_EXPECTED_HI);
    }

    puts(err ? "FAIL" : "PASS");
    return err ? 1 : 0;
}

/* hmx_cvt_rs_feedback_scale */
#define FS_ACT_SIZE  1024  /* one crouton, as uint16_t: 2048 bytes / 2 */
#define FS_WEI_SIZE  1024  /* 16 vectors x 128 bytes, as uint16_t */
#define FS_OUT_SIZE  2048

static uint16_t FS_act_buf[FS_ACT_SIZE] __attribute__((aligned(2048)));
static uint16_t FS_wei_buf[FS_WEI_SIZE] __attribute__((aligned(128)));
static uint32_t FS_bias_buf[32] __attribute__((aligned(128)));
static uint8_t FS_out_buf[FS_OUT_SIZE] __attribute__((aligned(2048)));

#define FS_FP16_ONE    0x3C00u
#define FS_SCALE_TWO   0x4000u    /* FP16 2.0 */
#define FS_BIAS_LOW32  FS_SCALE_TWO  /* scale=2.0, out_bias=0 */
#define FS_EXPECTED_LO 0x00u
#define FS_EXPECTED_HI 0x68u      /* 2048.0 as FP16 = 0x6800 */

static int hmx_test_hmx_cvt_rs_feedback_scale(void)
{
    err = 0;
    clear_accumulators();
    for (int i = 0; i < FS_ACT_SIZE; i++) {
        FS_act_buf[i] = FS_FP16_ONE;
    }
    for (int i = 0; i < FS_WEI_SIZE; i++) {
        FS_wei_buf[i] = FS_FP16_ONE;
    }
    for (int i = 0; i < 32; i++) {
        FS_bias_buf[i] = FS_BIAS_LOW32;
    }
    memset(FS_out_buf, 0xff, sizeof(FS_out_buf));

    /* bias = mxmem(r8) */
    asm volatile("r8 = %0\n"
                 ".word 0x9208c3ff    /* bias=mxmem(r8) */\n"
                 :
                 : "r"(FS_bias_buf)
                 : "r8", "memory");

    /* activation.hf = mxmem(r2,r3) */
    asm volatile("r2 = %0\n"
                 "r3 = #0x7c\n"
                 ".word 0x9202c3e4    /* activation.hf=mxmem(r2,r3) */\n"
                 :
                 : "r"(FS_act_buf)
                 : "r2", "r3", "memory");

    /* weight.hf = mxmem(r4,r5) -- triggers the FP matrix multiply. */
    asm volatile("r4 = %0\n"
                 "r5 = #0xffff\n"
                 ".word 0x9204e5ef    /* weight.hf=mxmem(r4,r5) */\n"
                 :
                 : "r"(FS_wei_buf)
                 : "r4", "r5", "memory");

    /*
     * cvt.hf=acc(r9), r9=1 (retain, fb_dst=0) -- populates the
     * feedback source (64.0) but its own result is never stored.
     */
    asm volatile("r9 = #1\n"
                 ".word 0xa6e9da10    /* cvt.hf=acc(r9) */\n"
                 :
                 :
                 : "r9", "memory");

    /*
     * cvt.hf=acc(r9), r9=0x19 (retain, fb_dst=SCALE, fb_limit=1) --
     * clamps scale against the feedback from the convert above.
     */
    asm volatile("r9 = #0x19\n"
                 ".word 0xa6e9da10    /* cvt.hf=acc(r9) */\n"
                 :
                 :
                 : "r9", "memory");

    /* mxmem(r6,r7)=cvt -- reads the second convert's result. */
    asm volatile("r6 = %0\n"
                 "r7 = #0\n"
                 ".word 0xa6e6c718    /* mxmem(r6,r7)=cvt */\n"
                 :
                 : "r"(FS_out_buf)
                 : "r6", "r7", "memory");

    for (int i = 0; i < FS_OUT_SIZE; i += 2) {
        check32(FS_out_buf[i], FS_EXPECTED_LO);
        check32(FS_out_buf[i + 1], FS_EXPECTED_HI);
    }

    puts(err ? "FAIL" : "PASS");
    return err ? 1 : 0;
}

/* hmx_matmul_fp_xfp_batch_boundary */
#define XB_ACT_SIZE  1024  /* one crouton, as uint16_t: 2048 bytes / 2 */
#define XB_WEI_SIZE  128    /* 2 vectors x 128 bytes, as uint16_t */
#define XB_OUT_SIZE  2048

static uint16_t XB_act_buf[XB_ACT_SIZE] __attribute__((aligned(2048)));
static uint16_t XB_wei_buf[XB_WEI_SIZE] __attribute__((aligned(128)));
static uint32_t XB_bias_buf[32] __attribute__((aligned(128)));
static uint8_t XB_out_buf[XB_OUT_SIZE] __attribute__((aligned(2048)));

#define XB_FP16_ONE    0x3C00u
#define XB_BIAS_LOW32  XB_FP16_ONE   /* scale=1.0, everything else 0 */
#define XB_EXPECTED_LO 0x00u
#define XB_EXPECTED_HI 0x44u      /* 4.0 as FP16 = 0x4400 */

static int hmx_test_hmx_matmul_fp_xfp_batch_boundary(void)
{
    err = 0;
    clear_accumulators();
    for (int i = 0; i < XB_ACT_SIZE; i++) {
        XB_act_buf[i] = XB_FP16_ONE;
    }
    for (int i = 0; i < XB_WEI_SIZE; i++) {
        XB_wei_buf[i] = XB_FP16_ONE;
    }
    for (int i = 0; i < 32; i++) {
        XB_bias_buf[i] = XB_BIAS_LOW32;
    }
    memset(XB_out_buf, 0xff, sizeof(XB_out_buf));

    /* bias = mxmem(r8) */
    asm volatile("r8 = %0\n"
                 ".word 0x9208c3ff    /* bias=mxmem(r8) */\n"
                 :
                 : "r"(XB_bias_buf)
                 : "r8", "memory");

    /* activation.hf = mxmem(r2,r3) */
    asm volatile("r2 = %0\n"
                 "r3 = #0x7c\n"
                 ".word 0x9202c3e4    /* activation.hf=mxmem(r2,r3) */\n"
                 :
                 : "r"(XB_act_buf)
                 : "r2", "r3", "memory");

    /*
     * weight.hf = mxmem(r4,r5) -- r5=128, not this suite's usual
     * 0xffff, gives max_valid_vec=1 (see file comment).
     */
    asm volatile("r4 = %0\n"
                 "r5 = #128\n"
                 ".word 0x9204e5ef    /* weight.hf=mxmem(r4,r5) */\n"
                 :
                 : "r"(XB_wei_buf)
                 : "r4", "r5", "memory");

    /*
     * cvt.hf=acc(r9), r9=0x100 (fp_rnd=1, see hmx_cvt_rs_fp16_overflow.c
     * for why that matters on the XFP path; everything else 0).
     */
    asm volatile("r9 = #0x100\n"
                 ".word 0xa6e9da10    /* cvt.hf=acc(r9) */\n"
                 :
                 :
                 : "r9", "memory");

    /* mxmem(r6,r7)=cvt -- reads the now-flushed convert result. */
    asm volatile("r6 = %0\n"
                 "r7 = #0\n"
                 ".word 0xa6e6c718    /* mxmem(r6,r7)=cvt */\n"
                 :
                 : "r"(XB_out_buf)
                 : "r6", "r7", "memory");

    for (int i = 0; i < XB_OUT_SIZE; i += 2) {
        check32(XB_out_buf[i], XB_EXPECTED_LO);
        check32(XB_out_buf[i + 1], XB_EXPECTED_HI);
    }

    puts(err ? "FAIL" : "PASS");
    return err ? 1 : 0;
}

/* hmx_matmul_fp_group_conv */
#define GC_ACT_SIZE  1024  /* one crouton, as uint16_t: 2048 bytes / 2 */
#define GC_WEI_SIZE  256    /* 4 vectors x 128 bytes, as uint16_t */
#define GC_OUT_SIZE  2048

static uint16_t GC_act_buf[GC_ACT_SIZE] __attribute__((aligned(2048)));
static uint16_t GC_wei_buf[GC_WEI_SIZE] __attribute__((aligned(128)));
static uint32_t GC_bias_buf[32] __attribute__((aligned(128)));
static uint8_t GC_out_buf[GC_OUT_SIZE] __attribute__((aligned(2048)));

#define GC_FP16_ONE    0x3C00u
#define GC_BIAS_LOW32  GC_FP16_ONE   /* scale=1.0, everything else 0 */
#define GC_EXPECTED_LO 0x00u
#define GC_EXPECTED_HI 0x48u      /* 8.0 as FP16 = 0x4800 */

static int hmx_test_hmx_matmul_fp_group_conv(void)
{
    err = 0;
    clear_accumulators();
    for (int i = 0; i < GC_ACT_SIZE; i++) {
        GC_act_buf[i] = GC_FP16_ONE;
    }
    for (int i = 0; i < GC_WEI_SIZE; i++) {
        GC_wei_buf[i] = GC_FP16_ONE;
    }
    for (int i = 0; i < 32; i++) {
        GC_bias_buf[i] = GC_BIAS_LOW32;
    }
    memset(GC_out_buf, 0xff, sizeof(GC_out_buf));

    /* bias = mxmem(r8) */
    asm volatile("r8 = %0\n"
                 ".word 0x9208c3ff    /* bias=mxmem(r8) */\n"
                 :
                 : "r"(GC_bias_buf)
                 : "r8", "memory");

    /*
     * activation.hf = mxmem(r2,r3) -- r2's channel-range bits (bit[6:2],
     * OR'd in after the buffer address) set raw_start=8; r3=0 sets
     * raw_stop=0. raw_start>raw_stop triggers group_conv (see file
     * comment for the group_size/group_count derivation).
     */
    asm volatile("r2 = %0\n"
                 "r2 = or(r2, #0x20)\n"
                 "r3 = #0\n"
                 ".word 0x9202c3e4    /* activation.hf=mxmem(r2,r3) */\n"
                 :
                 : "r"(GC_act_buf)
                 : "r2", "r3", "memory");

    /* weight.hf = mxmem(r4,r5) -- triggers the FP matrix multiply. */
    asm volatile("r4 = %0\n"
                 "r5 = #0xffff\n"
                 ".word 0x9204e5ef    /* weight.hf=mxmem(r4,r5) */\n"
                 :
                 : "r"(GC_wei_buf)
                 : "r4", "r5", "memory");

    /*
     * cvt.hf=acc(r9), r9=0x100 (fp_rnd=1, see hmx_cvt_rs_fp16_overflow.c
     * for why that matters on the XFP path; everything else 0).
     */
    asm volatile("r9 = #0x100\n"
                 ".word 0xa6e9da10    /* cvt.hf=acc(r9) */\n"
                 :
                 :
                 : "r9", "memory");

    /* mxmem(r6,r7)=cvt -- reads the now-flushed convert result. */
    asm volatile("r6 = %0\n"
                 "r7 = #0\n"
                 ".word 0xa6e6c718    /* mxmem(r6,r7)=cvt */\n"
                 :
                 : "r"(GC_out_buf)
                 : "r6", "r7", "memory");

    for (int i = 0; i < GC_OUT_SIZE; i += 2) {
        check32(GC_out_buf[i], GC_EXPECTED_LO);
        check32(GC_out_buf[i + 1], GC_EXPECTED_HI);
    }

    puts(err ? "FAIL" : "PASS");
    return err ? 1 : 0;
}

int main(void)
{
    if (hmx_test_hmx_matmul_fp_hf()) {
        return 1;
    }
    if (hmx_test_hmx_matmul_fp_f8()) {
        return 1;
    }
    if (hmx_test_hmx_matmul_fp_bf16()) {
        return 1;
    }
    if (hmx_test_hmx_cvt_rs_bf16()) {
        return 1;
    }
    if (hmx_test_hmx_cvt_rs_f8()) {
        return 1;
    }
    if (hmx_test_hmx_matmul_fp_bf16_cvt()) {
        return 1;
    }
    if (hmx_test_hmx_cvt_rs_f8_both_halves()) {
        return 1;
    }
    if (hmx_test_hmx_cvt_rs_negate_outbias()) {
        return 1;
    }
    if (hmx_test_hmx_cvt_rs_shape_clamp()) {
        return 1;
    }
    if (hmx_test_hmx_cvt_rs_acc_bias()) {
        return 1;
    }
    if (hmx_test_hmx_cvt_rs_fp16_overflow()) {
        return 1;
    }
    if (hmx_test_hmx_matmul_fp_dp()) {
        return 1;
    }
    if (hmx_test_hmx_cvt_rs_feedback_scale()) {
        return 1;
    }
    if (hmx_test_hmx_matmul_fp_xfp_batch_boundary()) {
        return 1;
    }
    if (hmx_test_hmx_matmul_fp_group_conv()) {
        return 1;
    }
    return 0;
}
