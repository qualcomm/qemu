/*
 *  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 * Shared helpers for HLX (Hexagon eLement-wise eXtensions) user-mode tests
 *
 * All HLX tests compile with:
 *   hexagon-unknown-linux-musl-clang -mv85 -mhlx -O2 -static
 * and run with:
 *   qemu-hexagon -cpu v85 <test>
 */

#ifndef HLX_TEST_H
#define HLX_TEST_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <hexagon_types.h>
#include <hlx_hexagon_protos.h>

static int err;

/*
 * HLX is enabled by QEMU's user-mode init (CCR.XE3 = 1).
 * No privileged CCR write needed - keep this as a no-op for
 * source compatibility with the system-mode tests.
 */
static inline void enable_hlx(void) { }

/* Verify halfword (uint16_t) array against expected values */
static void check_h(const char *name, const uint16_t *out,
                     const uint16_t *expected, int count)
{
    for (int i = 0; i < count; i++) {
        if (out[i] != expected[i]) {
            printf("FAIL %s [%d]: expected 0x%04x, got 0x%04x\n",
                   name, i, expected[i], out[i]);
            err++;
            if (err > 20) {
                printf("Too many errors, stopping %s\n", name);
                return;
            }
        }
    }
}

/* Verify word (uint32_t) array against expected values */
static void check_w(const char *name, const uint32_t *out,
                     const uint32_t *expected, int count)
{
    for (int i = 0; i < count; i++) {
        if (out[i] != expected[i]) {
            printf("FAIL %s [%d]: expected 0x%08lx, got 0x%08lx\n",
                   name, i, (unsigned long)expected[i],
                   (unsigned long)out[i]);
            err++;
            if (err > 20) {
                printf("Too many errors, stopping %s\n", name);
                return;
            }
        }
    }
}

/*
 * Verify word-pair values by reconstructing 32-bit words from the lo/hi
 * L-register buffers.  In HLX, a word pair is halfword-interleaved:
 *   word[i] = (hi.h[i] << 16) | lo.h[i]
 * Each L-register has HLX_HALF_ELEMS halfwords, giving HLX_HALF_ELEMS
 * 32-bit word elements per pair.
 */
static void check_wp(const char *name, const uint32_t *out_lo,
                      const uint32_t *out_hi, const uint32_t *expected,
                      int count)
{
    const uint16_t *lo = (const uint16_t *)out_lo;
    const uint16_t *hi = (const uint16_t *)out_hi;
    for (int i = 0; i < count; i++) {
        uint32_t got = ((uint32_t)hi[i] << 16) | lo[i];
        if (got != expected[i]) {
            printf("FAIL %s [%d]: expected 0x%08lx, got 0x%08lx\n",
                   name, i, (unsigned long)expected[i],
                   (unsigned long)got);
            err++;
            if (err > 20) {
                printf("Too many errors, stopping %s\n", name);
                return;
            }
        }
    }
}

/* Saturation helpers for reference computations */
static inline int16_t sat_h(int32_t val)
{
    if (val > 32767) {
        return 32767;
    }
    if (val < -32768) {
        return -32768;
    }
    return (int16_t)val;
}

static inline uint16_t sat_uh(int32_t val)
{
    if (val > 65535) {
        return 65535;
    }
    if (val < 0) {
        return 0;
    }
    return (uint16_t)val;
}

static inline int32_t sat_w(int64_t val)
{
    if (val > 2147483647LL) {
        return 2147483647;
    }
    if (val < -2147483648LL) {
        return -2147483648;
    }
    return (int32_t)val;
}

static inline uint32_t sat_uw(int64_t val)
{
    if (val > 4294967295LL) {
        return 4294967295U;
    }
    if (val < 0) {
        return 0;
    }
    return (uint32_t)val;
}

/*
 * HLX register size: 2048 bytes = 1024 halfwords = 512 words
 * All buffers must be 2048-byte aligned for L-register loads/stores.
 */
#define HLX_HALF_ELEMS  1024
#define HLX_WORD_ELEMS  512
#define HLX_VEC_BYTES   2048
#define HLX_ALIGN       __attribute__((aligned(2048)))

/*
 * User-mode memory allocation for HLX buffers.
 *
 * In system-mode, HLX memory operations use VTCM (Vectorized Tightly
 * Coupled Memory) with explicit TLB mappings.  In user-mode, regular
 * aligned heap memory works - QEMU handles the memory access uniformly.
 */

/* No-op in user-mode - no VTCM discovery or TLB setup needed */
static inline void init_vtcm(void) { }

/* No-op in user-mode - coherent memory, no cache flush needed */
static inline void vtcm_flush(const void *ptr, size_t bytes)
{
    (void)ptr;
    (void)bytes;
}

/* Allocate a 2KB-aligned buffer from the heap */
static inline void *vtcm_alloc(size_t bytes)
{
    size_t aligned_bytes = (bytes + (HLX_VEC_BYTES - 1))
                           & ~(size_t)(HLX_VEC_BYTES - 1);
    void *ptr = aligned_alloc(HLX_VEC_BYTES, aligned_bytes);
    if (!ptr) {
        fprintf(stderr, "aligned_alloc failed for %zu bytes\n", aligned_bytes);
        exit(1);
    }
    memset(ptr, 0, aligned_bytes);
    return ptr;
}

/*
 * Set up the Vector Stack Pointer (VSP) for L-register spills.
 *
 * The compiler uses VSP when it needs to spill HLX_Vector values.
 * VSP must point to memory with at least HLX_VEC_BYTES of space
 * per spill slot, aligned to HLX_VEC_BYTES.
 *
 * Reserves HLX_VSP_SIZE bytes for the spill area.
 */
#define HLX_VSP_SIZE  (16 * HLX_VEC_BYTES) /* 32 KB for spill slots */

static inline void init_hlx_vsp(void)
{
    void *vsp_area = vtcm_alloc(HLX_VSP_SIZE);
    /* VSP grows downward, so point it to the END of the area */
    uint32_t vsp_top = (uint32_t)(uintptr_t)vsp_area + HLX_VSP_SIZE;
    asm volatile("vsp = %0\n" : : "r"(vsp_top));
}

#endif /* HLX_TEST_H */
