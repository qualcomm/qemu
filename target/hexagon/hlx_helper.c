/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 *  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

/*
 * HLX (Hexagon eLement-wise eXtensions) Helper Functions
 *
 * This file implements runtime helpers for HLX operations.
 * HLX provides 16 registers (L0-L15) of 2048 bytes each for
 * element-wise vector operations.
 */

#include "qemu/osdep.h"
#include "cpu.h"
#include "accel/tcg/cpu-ldst.h"
#include "exec/helper-proto.h"
#include "hlx/hlx.h"
#include "hw/hexagon/hexagon_hlx.h"
#include "accel/tcg/probe.h"
#include "tcg/tcg-gvec-desc.h"

/*
 * HLX Memory Operations
 *
 * HLX memory operations have strict requirements:
 * - Must access VTCM (Vectorized Tightly Coupled Memory)
 * - Must be 2KB aligned (HLX_VEC_SIZE_BYTES)
 * - Load/store entire 2KB vectors atomically
 */

#define HLX_MEM_ALIGN_MASK (~(HLX_VEC_SIZE_BYTES - 1))

/*
 * Load HLX vector from memory
 * EA: Effective address (will be aligned to 2KB)
 * dst: Destination HLXVector pointer
 * above/below: Bank access flags (for VTCM banking - currently unused)
 *
 * This performs a 2KB vector load from memory. The address is aligned
 * to 2KB boundary as required by HLX architecture.
 */
void HELPER(hlx_load_vec)(CPUHexagonState *env, void *dst_void,
                          target_ulong ea, uint32_t above, uint32_t below)
{
    HLXVector *dst = (HLXVector *)dst_void;
    target_ulong aligned_ea = ea & HLX_MEM_ALIGN_MASK;
    uintptr_t raddr = GETPC();
    int mmu_idx = cpu_mmu_index(env_cpu(env), false);

    /*
     * HLX vectors are 2KB and must be 2KB aligned, so they never
     * cross page boundaries (assuming pages are at least 2KB).
     */
    void *host = probe_read(env, aligned_ea, HLX_VEC_SIZE_BYTES, mmu_idx,
                            raddr);

    if (host) {
        /* Fast path: host memory is directly accessible */
        memcpy(dst, host, HLX_VEC_SIZE_BYTES);
    } else {
        /* Slow path: go through MMU for each access */
        for (int i = 0; i < HLX_VEC_SIZE_BYTES; i += 8) {
            uint64_t val = cpu_ldq_le_data_ra(env, aligned_ea + i, raddr);
            memcpy(&dst->ub[i], &val, 8);
        }
    }

    /* Bank access flags (above/below) are for VTCM banking - not yet implemented */
    (void)above;
    (void)below;
}

/*
 * Store HLX vector to memory
 * EA: Effective address (will be aligned to 2KB)
 * src: Source HLXVector pointer
 * above/below: Bank access flags (for VTCM banking - currently unused)
 *
 * This performs a 2KB vector store to memory. The address is aligned
 * to 2KB boundary as required by HLX architecture.
 */
void HELPER(hlx_store_vec)(CPUHexagonState *env, target_ulong ea,
                           void *src_void, uint32_t above, uint32_t below)
{
    HLXVector *src = (HLXVector *)src_void;
    target_ulong aligned_ea = ea & HLX_MEM_ALIGN_MASK;
    uintptr_t raddr = GETPC();
    int mmu_idx = cpu_mmu_index(env_cpu(env), false);

    void *host = probe_write(env, aligned_ea, HLX_VEC_SIZE_BYTES, mmu_idx,
                             raddr);

    if (host) {
        /* Fast path: host memory is directly accessible */
        memcpy(host, src, HLX_VEC_SIZE_BYTES);
    } else {
        /* Slow path: go through MMU for each access */
        for (int i = 0; i < HLX_VEC_SIZE_BYTES; i += 8) {
            uint64_t val;
            memcpy(&val, &src->ub[i], 8);
            cpu_stq_le_data_ra(env, aligned_ea + i, val, raddr);
        }
    }

    (void)above;
    (void)below;
}

void HELPER(hlx_copy_live_to_future)(CPUHexagonState *env, uint32_t rnum)
{
    g_assert(env->hlx_ctx != NULL);
    memcpy(&env->future_LRegs[rnum], &env->hlx_ctx->LRegs[rnum],
           sizeof(HLXVector));
}

void HELPER(hlx_copy_live_to_tmp)(CPUHexagonState *env, uint32_t rnum)
{
    g_assert(env->hlx_ctx != NULL);
    memcpy(&env->tmp_LRegs[rnum], &env->hlx_ctx->LRegs[rnum],
           sizeof(HLXVector));
}

void HELPER(hlx_commit_reg)(CPUHexagonState *env, uint32_t rnum)
{
    g_assert(env->hlx_ctx != NULL);
    memcpy(&env->hlx_ctx->LRegs[rnum], &env->future_LRegs[rnum],
           sizeof(HLXVector));
}

void HELPER(hlx_commit_tmp_reg)(CPUHexagonState *env, uint32_t rnum)
{
    g_assert(env->hlx_ctx != NULL);
    memcpy(&env->hlx_ctx->LRegs[rnum], &env->tmp_LRegs[rnum],
           sizeof(HLXVector));
}

/*
 * GVec out-of-line (fno) helpers for signed-to-unsigned saturating
 * arithmetic.  These are needed because HLX vectors are 2048 bytes -
 * far beyond gvec's MAX_UNROLL limit - so the fniv path is never taken
 * and the fno fallback is always used.
 *
 * All three helpers follow the standard gvec helper ABI:
 *   void helper(void *d, [void *a,] void *b, uint32_t desc)
 * where desc encodes oprsz / maxsz via simd_oprsz() / simd_maxsz().
 */

/*
 * hlx_gvec_ushsat_add16 - d[i] = clamp(s16(a[i]) + s16(b[i]), 0, 65535)
 */
void HELPER(hlx_gvec_ushsat_add16)(void *d, void *a, void *b, uint32_t desc)
{
    intptr_t oprsz = simd_oprsz(desc);
    intptr_t i;

    for (i = 0; i < oprsz; i += sizeof(int16_t)) {
        int32_t r = (int32_t)(*(int16_t *)((char *)a + i))
                  + (int32_t)(*(int16_t *)((char *)b + i));
        if (r < 0) {
            r = 0;
        } else if (r > 0xFFFF) {
            r = 0xFFFF;
        }
        *(uint16_t *)((char *)d + i) = (uint16_t)r;
    }
}

/*
 * hlx_gvec_ushsat_sub16 - d[i] = clamp(s16(a[i]) - s16(b[i]), 0, 65535)
 */
void HELPER(hlx_gvec_ushsat_sub16)(void *d, void *a, void *b, uint32_t desc)
{
    intptr_t oprsz = simd_oprsz(desc);
    intptr_t i;

    for (i = 0; i < oprsz; i += sizeof(int16_t)) {
        int32_t r = (int32_t)(*(int16_t *)((char *)a + i))
                  - (int32_t)(*(int16_t *)((char *)b + i));
        if (r < 0) {
            r = 0;
        } else if (r > 0xFFFF) {
            r = 0xFFFF;
        }
        *(uint16_t *)((char *)d + i) = (uint16_t)r;
    }
}

/*
 * hlx_gvec_abs_uh_sat16 - d[i] = min(abs(s16(a[i])), INT16_MAX)
 *
 * For all inputs except INT16_MIN: abs(a) in [0, 32767] -> unchanged.
 * For INT16_MIN (-32768): abs wraps to -32768 in i16; clamp to INT16_MAX.
 */
void HELPER(hlx_gvec_abs_uh_sat16)(void *d, void *a, uint32_t desc)
{
    intptr_t oprsz = simd_oprsz(desc);
    intptr_t i;

    for (i = 0; i < oprsz; i += sizeof(int16_t)) {
        int16_t v = *(int16_t *)((char *)a + i);
        int32_t r = (v < 0) ? -(int32_t)v : (int32_t)v;
        if (r > INT16_MAX) {
            r = INT16_MAX;  /* saturate INT16_MIN -> INT16_MAX */
        }
        *(int16_t *)((char *)d + i) = (int16_t)r;
    }
}

/*
 * GVec fno helpers for halfword average ops (MO_16).
 *
 * HLX vectors are 2048 bytes - far beyond gvec's MAX_UNROLL limit - so
 * the fniv path is never taken and the fno fallback is always used.
 *
 * All use the classic overflow-free formulas:
 *   truncating: (a & b) + ((a ^ b) >> 1)
 *   rounded:    (a | b) - ((a ^ b) >> 1)
 * Signed variants use arithmetic right shift; unsigned use logical.
 */

/*
 * hlx_gvec_avg_sh16 - d[i] = trunc_avg(s16(a[i]), s16(b[i]))
 * Signed average, truncating: (a & b) + ((a ^ b) >>a 1)
 */
void HELPER(hlx_gvec_avg_sh16)(void *d, void *a, void *b, uint32_t desc)
{
    intptr_t oprsz = simd_oprsz(desc);
    intptr_t i;

    for (i = 0; i < oprsz; i += sizeof(int16_t)) {
        int16_t av = *(int16_t *)((char *)a + i);
        int16_t bv = *(int16_t *)((char *)b + i);
        int16_t r = (int16_t)((av & bv) + ((av ^ bv) >> 1));
        *(int16_t *)((char *)d + i) = r;
    }
}

/*
 * hlx_gvec_avg_sh16_rnd - d[i] = rnd_avg(s16(a[i]), s16(b[i]))
 * Signed average, rounded: (a | b) - ((a ^ b) >>a 1)
 */
void HELPER(hlx_gvec_avg_sh16_rnd)(void *d, void *a, void *b, uint32_t desc)
{
    intptr_t oprsz = simd_oprsz(desc);
    intptr_t i;

    for (i = 0; i < oprsz; i += sizeof(int16_t)) {
        int16_t av = *(int16_t *)((char *)a + i);
        int16_t bv = *(int16_t *)((char *)b + i);
        int16_t r = (int16_t)((av | bv) - ((av ^ bv) >> 1));
        *(int16_t *)((char *)d + i) = r;
    }
}

/*
 * hlx_gvec_avg_uh16 - d[i] = trunc_avg(u16(a[i]), u16(b[i]))
 * Unsigned average, truncating: (a & b) + ((a ^ b) >>l 1)
 */
void HELPER(hlx_gvec_avg_uh16)(void *d, void *a, void *b, uint32_t desc)
{
    intptr_t oprsz = simd_oprsz(desc);
    intptr_t i;

    for (i = 0; i < oprsz; i += sizeof(uint16_t)) {
        uint16_t av = *(uint16_t *)((char *)a + i);
        uint16_t bv = *(uint16_t *)((char *)b + i);
        uint16_t r = (uint16_t)((av & bv) + ((av ^ bv) >> 1));
        *(uint16_t *)((char *)d + i) = r;
    }
}

/*
 * hlx_gvec_avg_uh16_rnd - d[i] = rnd_avg(u16(a[i]), u16(b[i]))
 * Unsigned average, rounded: (a | b) - ((a ^ b) >>l 1)
 */
void HELPER(hlx_gvec_avg_uh16_rnd)(void *d, void *a, void *b, uint32_t desc)
{
    intptr_t oprsz = simd_oprsz(desc);
    intptr_t i;

    for (i = 0; i < oprsz; i += sizeof(uint16_t)) {
        uint16_t av = *(uint16_t *)((char *)a + i);
        uint16_t bv = *(uint16_t *)((char *)b + i);
        uint16_t r = (uint16_t)((av | bv) - ((av ^ bv) >> 1));
        *(uint16_t *)((char *)d + i) = r;
    }
}
