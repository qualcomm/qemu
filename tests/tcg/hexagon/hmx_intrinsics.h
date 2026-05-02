/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * HMX intrinsic polyfill for linux-user tests.
 *
 * Provides Q6_* macros matching the official hmx_hexagon_protos.h naming
 * convention.  Each macro emits a .word-encoded HMX instruction since the
 * upstream cross-compiler does not yet support HMX builtins.
 *
 * When the toolchain gains HMX support, replace this header with:
 *   #include <hmx_hexagon_protos.h>
 *
 * Encoding reference: target/hexagon/imported/hmx/encode_ext.def
 *
 * Standalone instructions (Q6_* macros) pin Rs=r0, Rt=r1 with
 * PP bits [15:14] = 11 (end-of-packet).
 *
 * Paired activation+weight instructions use _HMX_PAIRED which emits
 * both words in a single packet: activation (Rs=r0, Rt=r1, PP=01)
 * followed by weight (Rs=r2, Rt=r3, PP=11).
 */

#ifndef HMX_INTRINSICS_H
#define HMX_INTRINSICS_H

#include <stdint.h>

/*
 * _HMX_WORD -- emit a raw .word instruction encoding.
 * The memory clobber ensures the compiler does not reorder
 * memory operations across the HMX instruction.
 */
#define _HMX_WORD(encoding)  \
    asm volatile(".word " #encoding "\n" : : : "memory")

/* Two-operand helper: Rs pinned to r0, Rt pinned to r1 */
#define _HMX_RS_RT(encoding, RS_VAR, RT_VAR)                           \
    do {                                                                \
        register uint32_t _rs asm("r0") = (uint32_t)(uintptr_t)(RS_VAR);\
        register uint32_t _rt asm("r1") = (uint32_t)(uintptr_t)(RT_VAR);\
        asm volatile(                                                   \
            ".word " #encoding "\n"                                     \
            : : "r"(_rs), "r"(_rt) : "memory");                        \
    } while (0)

/* One-operand helper: Rs pinned to r0 */
#define _HMX_RS(encoding, RS_VAR)                                       \
    do {                                                                \
        register uint32_t _rs asm("r0") = (uint32_t)(uintptr_t)(RS_VAR);\
        asm volatile(                                                   \
            ".word " #encoding "\n"                                     \
            : : "r"(_rs) : "memory");                                  \
    } while (0)

/*
 * _HMX_PAIRED -- emit a paired activation+weight packet.
 *
 * The ISA requires activation and weight instructions to be in the
 * same packet.  The activation word uses PP=01 (not end-of-packet)
 * and the weight word uses PP=11 (end-of-packet).
 *
 * Activation operands: Rs=r0 (address), Rt=r1 (range)
 * Weight operands:     Rs=r2 (address), Rt=r3 (range)
 *
 * Both words are emitted in a single asm block to guarantee they
 * remain contiguous in the instruction stream.
 */
#define _HMX_STR(x)   #x
#define _HMX_XSTR(x)  _HMX_STR(x)

#define _HMX_PAIRED(act_enc, wei_enc, ACT_ADDR, ACT_RANGE,             \
                    WEI_ADDR, WEI_RANGE)                                \
    do {                                                                \
        register uint32_t _a0 asm("r0") =                              \
            (uint32_t)(uintptr_t)(ACT_ADDR);                           \
        register uint32_t _a1 asm("r1") =                              \
            (uint32_t)(uintptr_t)(ACT_RANGE);                          \
        register uint32_t _w0 asm("r2") =                              \
            (uint32_t)(uintptr_t)(WEI_ADDR);                           \
        register uint32_t _w1 asm("r3") =                              \
            (uint32_t)(uintptr_t)(WEI_RANGE);                          \
        asm volatile(                                                   \
            ".word " _HMX_XSTR(act_enc) "\n"                           \
            ".word " _HMX_XSTR(wei_enc) "\n"                           \
            : : "r"(_a0), "r"(_a1), "r"(_w0), "r"(_w1)                \
            : "memory");                                               \
    } while (0)

/* Paired activation encodings (PP=01, not end-of-packet) */
#define _HMX_ACT_UB_PAIRED         0x920041EC
#define _HMX_ACT_HF_PAIRED         0x920041E4
#define _HMX_ACT_UB_DEEP_PAIRED    0x920041E0
#define _HMX_ACT_UB_SINGLE_PAIRED  0x920041F0
#define _HMX_ACT_UB_ABOVE_PAIRED   0x920041EE
#define _HMX_ACT_UB_CM_PAIRED      0x920041ED

/* Paired weight encodings (PP=11, end-of-packet) */
#define _HMX_WEI_B_PAIRED        0x9202E3E0
#define _HMX_WEI_HF_PAIRED       0x9202E3EF
#define _HMX_WEI_N_PAIRED        0x9202E3E1
#define _HMX_WEI_C_PAIRED        0x9202E3E2
#define _HMX_WEI_F8_PAIRED       0x9202E347
#define _HMX_WEI_SM_PAIRED       0x9202E3F1
#define _HMX_WEI_SC_PAIRED       0x9202E3F0
#define _HMX_WEI_UBIT_PAIRED     0x9202E3E3
#define _HMX_WEI_SBIT_PAIRED     0x9202E3E4
#define _HMX_WEI_B_SINGLE_PAIRED 0x9202E3E6
#define _HMX_WEI_B_AFTER_PAIRED  0x9202E3E9
#define _HMX_WEI_B_DROP_PAIRED   0x9202E3E7
#define _HMX_WEI_B_DILATE_PAIRED 0x9202E3EB

/* ================================================================== */
/* Accumulator management (no register operands)                      */
/* ================================================================== */

/* M8_mxclracc: mxclracc */
#define Q6_mxclracc() \
    _HMX_WORD(0xA6E0C011)

/* M8_mxclracc_hf: mxclracc.hf */
#define Q6_mxclracc_hf() \
    _HMX_WORD(0xA6E0C013)

/* M8_mxswap: mxswapacc */
#define Q6_mxswapacc() \
    _HMX_WORD(0xA6E0C014)

/* M8_mxaccshl: mxaccshl */
#define Q6_mxaccshl() \
    _HMX_WORD(0xA6E0C017)

/* ================================================================== */
/* Activation loads (Rs=address, Rt=range)                            */
/* ================================================================== */

/* M8_mxmem_blk_sm_act_ub: activation.ub=mxmem(Rs32,Rt32) */
#define Q6_activation_ub_mxmem_RR(Rs, Rt) \
    _HMX_RS_RT(0x9200C1EC, Rs, Rt)

/* M8_mxmem_blk_dm_act_ub: activation.ub=mxmem(Rs32,Rt32):cm */
#define Q6_activation_ub_mxmem_RR_cm(Rs, Rt) \
    _HMX_RS_RT(0x9200C1ED, Rs, Rt)

/* M8_mxmem_blk_sm_act_hf: activation.hf=mxmem(Rs32,Rt32) */
#define Q6_activation_hf_mxmem_RR(Rs, Rt) \
    _HMX_RS_RT(0x9200C1E4, Rs, Rt)

/* M8_mxmem_sm_act_ub: activation.ub=mxmem(Rs32,Rt32):deep */
#define Q6_activation_ub_mxmem_RR_deep(Rs, Rt) \
    _HMX_RS_RT(0x9200C1E0, Rs, Rt)

/* ================================================================== */
/* Weight loads (Rs=address, Rt=range)                                */
/* ================================================================== */

/* M8_mxmem_wei_b: weight.b=mxmem(Rs32,Rt32) */
#define Q6_weight_b_mxmem_RR(Rs, Rt) \
    _HMX_RS_RT(0x9200E1E0, Rs, Rt)

/* M8_mxmem_wei_hf: weight.hf=mxmem(Rs32,Rt32) */
#define Q6_weight_hf_mxmem_RR(Rs, Rt) \
    _HMX_RS_RT(0x9200E1EF, Rs, Rt)

/* M8_mxmem_wei_n: weight.n=mxmem(Rs32,Rt32)  (nibble) */
#define Q6_weight_n_mxmem_RR(Rs, Rt) \
    _HMX_RS_RT(0x9200E1E1, Rs, Rt)

/* M8_mxmem_wei_c: weight.c=mxmem(Rs32,Rt32)  (crumb) */
#define Q6_weight_c_mxmem_RR(Rs, Rt) \
    _HMX_RS_RT(0x9200E1E2, Rs, Rt)

/* M8_mxmem_wei_f8: weight.f8=mxmem(Rs32,Rt32) */
#define Q6_weight_f8_mxmem_RR(Rs, Rt) \
    _HMX_RS_RT(0x9200E147, Rs, Rt)

/* M8_mxmem_wei_sm: weight.sm=mxmem(Rs32,Rt32) */
#define Q6_weight_sm_mxmem_RR(Rs, Rt) \
    _HMX_RS_RT(0x9200E1F1, Rs, Rt)

/* M8_mxmem_wei_sc: weight.sc=mxmem(Rs32,Rt32)  (scrumb) */
#define Q6_weight_sc_mxmem_RR(Rs, Rt) \
    _HMX_RS_RT(0x9200E1F0, Rs, Rt)

/* M8_mxmem_wei_b1: weight.ubit=mxmem(Rs32,Rt32) */
#define Q6_weight_ubit_mxmem_RR(Rs, Rt) \
    _HMX_RS_RT(0x9200E1E3, Rs, Rt)

/* M8_mxmem_wei_sb1: weight.sbit=mxmem(Rs32,Rt32) */
#define Q6_weight_sbit_mxmem_RR(Rs, Rt) \
    _HMX_RS_RT(0x9200E1E4, Rs, Rt)

/* ================================================================== */
/* Weight load modifiers (Rs=address, Rt=range)                       */
/* ================================================================== */

/* M8_mxmems_wei_b: weight.b=mxmem(Rs32,Rt32):single */
#define Q6_weight_b_mxmem_RR_single(Rs, Rt) \
    _HMX_RS_RT(0x9200E1E6, Rs, Rt)

/* M8_mxmemdr_wei_b: weight.b=mxmem(Rs32,Rt32):drop */
#define Q6_weight_b_mxmem_RR_drop(Rs, Rt) \
    _HMX_RS_RT(0x9200E1E7, Rs, Rt)

/* M8_mxmema_wei_b: weight.b=mxmem(Rs32,Rt32):after */
#define Q6_weight_b_mxmem_RR_after(Rs, Rt) \
    _HMX_RS_RT(0x9200E1E9, Rs, Rt)

/* M8_mxmemdi_wei_b: weight.b=mxmem(Rs32,Rt32):dilate */
#define Q6_weight_b_mxmem_RR_dilate(Rs, Rt) \
    _HMX_RS_RT(0x9200E1EB, Rs, Rt)

/* ================================================================== */
/* Activation load modifiers (Rs=address, Rt=range)                   */
/* ================================================================== */

/* M8_mxmems_blk_sm_act_ub: activation.ub=mxmem(Rs32,Rt32):single */
#define Q6_activation_ub_mxmem_RR_single(Rs, Rt) \
    _HMX_RS_RT(0x9200C1F0, Rs, Rt)

/* M8_mxmemu_blk_sm_act_ub: activation.ub=mxmem(Rs32,Rt32):above */
#define Q6_activation_ub_mxmem_RR_above(Rs, Rt) \
    _HMX_RS_RT(0x9200C1EE, Rs, Rt)

/* ================================================================== */
/* Bias operations (Rs=address)                                       */
/* ================================================================== */

/* M8_mxmem2_bias: bias=mxmem2(Rs32) */
#define Q6_bias_mxmem2_A(Rs) \
    _HMX_RS(0x9200C3FE, Rs)

/* ================================================================== */
/* Legacy convert-and-store (Rs=address, Rt=range)                    */
/* ================================================================== */

/* M8_mxcvtr_sat_ub: mxmem(Rs32,Rt32):after:sat.ub=acc */
#define Q6_mxmem_AR_after_sat_ub(Rs, Rt) \
    _HMX_RS_RT(0xA6E0C104, Rs, Rt)

/* M8_mxcvtr_dm_sat_ub: mxmem(Rs32,Rt32):after:cm:sat.ub=acc */
#define Q6_mxmem_AR_after_cm_sat_ub(Rs, Rt) \
    _HMX_RS_RT(0xA6E0C105, Rs, Rt)

/* M8_mxcvtr_sat_ub_r: mxmem(Rs32,Rt32):after:retain:sat.ub=acc */
#define Q6_mxmem_AR_after_retain_sat_ub(Rs, Rt) \
    _HMX_RS_RT(0xA6E0C10C, Rs, Rt)

/* M8_mxcvtr_sat_hf: mxmem(Rs32,Rt32):after.hf=acc */
#define Q6_mxmem_AR_after_hf(Rs, Rt) \
    _HMX_RS_RT(0xA6E0E104, Rs, Rt)

/* M8_mxcvtb_sat_uh: mxmem(Rs32,Rt32):bottom:sat.uh=acc */
#define Q6_mxmem_bottom_sat_uh(Rs, Rt) \
    _HMX_RS_RT(0xA6E0E102, Rs, Rt)

/* M8_mxcvta_sat_uh: mxmem(Rs32,Rt32):above:sat.uh=acc */
#define Q6_mxmem_above_sat_uh(Rs, Rt) \
    _HMX_RS_RT(0xA6E0E10A, Rs, Rt)

/* M8_mxcvtb_sat_uh2x2: mxmem(Rs32,Rt32):bottom:sat.uh2x2=acc */
#define Q6_mxmem_bottom_sat_uh2x2(Rs, Rt) \
    _HMX_RS_RT(0xA6E0E112, Rs, Rt)

/* M8_mxcvta_sat_uh2x2: mxmem(Rs32,Rt32):above:sat.uh2x2=acc */
#define Q6_mxmem_above_sat_uh2x2(Rs, Rt) \
    _HMX_RS_RT(0xA6E0E11A, Rs, Rt)

/* ================================================================== */
/* Non-legacy convert (v73+)                                          */
/* ================================================================== */

/* M8_cvt_rs_ub: cvt.ub=acc(Rs32) */
#define Q6_cvt_ub_acc_R(Rs) \
    _HMX_RS(0xA6E0D710, Rs)

/* M8_mxmem: mxmem(Rs32,Rt32)=cvt */
#define Q6_mxmem_cvt_RR(Rs, Rt) \
    _HMX_RS_RT(0xA6E0C118, Rs, Rt)

/* M8_cvt_rs_f8: cvt.f8=acc(Rs32) */
#define Q6_cvt_f8_acc_R(Rs) \
    _HMX_RS(0xA6E0DB10, Rs)

/* M8_mxmem_f8: mxmem(Rs32,Rt32).f8=cvt */
#define Q6_mxmem_cvt_F8_RR(Rs, Rt) \
    _HMX_RS_RT(0xA6E0C11D, Rs, Rt)

#endif /* HMX_INTRINSICS_H */
