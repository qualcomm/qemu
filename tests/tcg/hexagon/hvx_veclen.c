/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 * Test HVX vector length modes
 *
 * This test exercises basic HVX operations at whatever vector length the
 * compiler targets (__HVX_LENGTH__).  It verifies vector loads, stores,
 * arithmetic, and predicate operations all operate on the correct number
 * of bytes.  Build once with -mhvx-length=64b and once with -mhvx-length=128b
 * to cover both modes.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#ifndef __HVX_LENGTH__
#error "This test must be compiled with -mhvx"
#endif

#define VEC_LEN __HVX_LENGTH__

int err;

typedef union {
    uint64_t ud[VEC_LEN / 8];
    int64_t   d[VEC_LEN / 8];
    uint32_t uw[VEC_LEN / 4];
    int32_t   w[VEC_LEN / 4];
    uint16_t uh[VEC_LEN / 2];
    int16_t   h[VEC_LEN / 2];
    uint8_t  ub[VEC_LEN];
    int8_t    b[VEC_LEN];
} HVXVec;

static inline void check(int line, int i, int j,
                         uint64_t result, uint64_t expect)
{
    if (result != expect) {
        printf("ERROR at line %d: [%d][%d] 0x%016llx != 0x%016llx\n",
               line, i, j,
               (unsigned long long)result, (unsigned long long)expect);
        err++;
    }
}

#define BUFSIZE 4

HVXVec buffer0[BUFSIZE] __attribute__((aligned(VEC_LEN)));
HVXVec buffer1[BUFSIZE] __attribute__((aligned(VEC_LEN)));
HVXVec output[BUFSIZE]  __attribute__((aligned(VEC_LEN)));
HVXVec expect[BUFSIZE]  __attribute__((aligned(VEC_LEN)));

static void init_buffers(void)
{
    int counter0 = 0;
    int counter1 = 17;
    int i, j;

    for (i = 0; i < BUFSIZE; i++) {
        for (j = 0; j < VEC_LEN; j++) {
            buffer0[i].b[j] = counter0++;
            buffer1[i].b[j] = counter1++;
        }
    }
}

/* Test basic vector load and store */
static void test_load_store(void)
{
    void *p0 = buffer0;
    void *pout = output;
    int i, j;

    for (i = 0; i < BUFSIZE; i++) {
        asm("v2 = vmem(%0 + #0)\n\t"
            "vmem(%1 + #0) = v2\n\t"
            : : "r"(p0), "r"(pout) : "v2", "memory");
        p0 += VEC_LEN;
        pout += VEC_LEN;
    }

    for (i = 0; i < BUFSIZE; i++) {
        for (j = 0; j < VEC_LEN / 4; j++) {
            check(__LINE__, i, j, output[i].uw[j], buffer0[i].uw[j]);
        }
    }
}

/* Test vector add (word) */
static void test_vadd_w(void)
{
    void *p0 = buffer0;
    void *p1 = buffer1;
    void *pout = output;
    int i, j;

    for (i = 0; i < BUFSIZE; i++) {
        asm("v2 = vmem(%0 + #0)\n\t"
            "v3 = vmem(%1 + #0)\n\t"
            "v2.w = vadd(v2.w, v3.w)\n\t"
            "vmem(%2 + #0) = v2\n\t"
            : : "r"(p0), "r"(p1), "r"(pout) : "v2", "v3", "memory");
        p0 += VEC_LEN;
        p1 += VEC_LEN;
        pout += VEC_LEN;
    }

    for (i = 0; i < BUFSIZE; i++) {
        for (j = 0; j < VEC_LEN / 4; j++) {
            expect[i].w[j] = buffer0[i].w[j] + buffer1[i].w[j];
        }
    }
    for (i = 0; i < BUFSIZE; i++) {
        for (j = 0; j < VEC_LEN / 4; j++) {
            check(__LINE__, i, j, output[i].uw[j], expect[i].uw[j]);
        }
    }
}

/* Test vector sub (halfword) */
static void test_vsub_h(void)
{
    void *p0 = buffer0;
    void *p1 = buffer1;
    void *pout = output;
    int i, j;

    for (i = 0; i < BUFSIZE; i++) {
        asm("v2 = vmem(%0 + #0)\n\t"
            "v3 = vmem(%1 + #0)\n\t"
            "v2.h = vsub(v2.h, v3.h)\n\t"
            "vmem(%2 + #0) = v2\n\t"
            : : "r"(p0), "r"(p1), "r"(pout) : "v2", "v3", "memory");
        p0 += VEC_LEN;
        p1 += VEC_LEN;
        pout += VEC_LEN;
    }

    for (i = 0; i < BUFSIZE; i++) {
        for (j = 0; j < VEC_LEN / 2; j++) {
            expect[i].h[j] = buffer0[i].h[j] - buffer1[i].h[j];
        }
    }
    for (i = 0; i < BUFSIZE; i++) {
        for (j = 0; j < VEC_LEN / 2; j++) {
            check(__LINE__, i, j, output[i].uh[j], expect[i].uh[j]);
        }
    }
}

/* Test vector XOR */
static void test_vxor(void)
{
    void *p0 = buffer0;
    void *p1 = buffer1;
    void *pout = output;
    int i, j;

    for (i = 0; i < BUFSIZE; i++) {
        asm("v2 = vmem(%0 + #0)\n\t"
            "v3 = vmem(%1 + #0)\n\t"
            "v2 = vxor(v2, v3)\n\t"
            "vmem(%2 + #0) = v2\n\t"
            : : "r"(p0), "r"(p1), "r"(pout) : "v2", "v3", "memory");
        p0 += VEC_LEN;
        p1 += VEC_LEN;
        pout += VEC_LEN;
    }

    for (i = 0; i < BUFSIZE; i++) {
        for (j = 0; j < VEC_LEN / 8; j++) {
            expect[i].ud[j] = buffer0[i].ud[j] ^ buffer1[i].ud[j];
        }
    }
    for (i = 0; i < BUFSIZE; i++) {
        for (j = 0; j < VEC_LEN / 8; j++) {
            check(__LINE__, i, j, output[i].ud[j], expect[i].ud[j]);
        }
    }
}

/* Test vector splat */
static void test_vsplat(void)
{
    void *pout = output;
    uint32_t val = 0xdeadbeef;
    int j;

    asm("v2 = vsplat(%1)\n\t"
        "vmem(%0 + #0) = v2\n\t"
        : : "r"(pout), "r"(val) : "v2", "memory");

    for (j = 0; j < VEC_LEN / 4; j++) {
        check(__LINE__, 0, j, output[0].uw[j], val);
    }
}

/* Test aligned load: low bits of address should be masked to VEC_LEN */
static void test_load_aligned(void)
{
    void *p0 = buffer0;
    void *pout = output;
    int j;

    p0 += 13;    /* Create an unaligned address */
    asm("v2 = vmem(%0 + #0)\n\t"
        "vmem(%1 + #0) = v2\n\t"
        : : "r"(p0), "r"(pout) : "v2", "memory");

    /* Aligned load should snap to VEC_LEN-byte boundary */
    for (j = 0; j < VEC_LEN / 4; j++) {
        check(__LINE__, 0, j, output[0].uw[j], buffer0[0].uw[j]);
    }
}

/*
 * Test that vector pair operations work correctly.
 * vcombine creates a pair from two single vectors.
 */
static void test_vcombine(void)
{
    void *pout = output;
    int j;

    asm volatile("v2 = vsplat(%0)\n\t"
                 "v3 = vsplat(%1)\n\t"
                 "v3:2 = vcombine(v2, v3)\n\t"
                 "vmem(%2+#0) = v2\n\t"
                 "vmem(%2+#1) = v3\n\t"
                 :
                 : "r"(0xAAAAAAAA), "r"(0xBBBBBBBB), "r"(pout)
                 : "v2", "v3", "memory");

    /* vcombine(hi, lo): v2 = lo (0xBBBBBBBB), v3 = hi (0xAAAAAAAA) */
    for (j = 0; j < VEC_LEN / 4; j++) {
        check(__LINE__, 0, j, output[0].uw[j], 0xBBBBBBBB);
        check(__LINE__, 1, j, output[1].uw[j], 0xAAAAAAAA);
    }
}

int main(void)
{
    init_buffers();

    printf("Testing HVX with vector length = %d bytes\n", VEC_LEN);

    test_load_store();
    test_vadd_w();
    test_vsub_h();
    test_vxor();
    test_vsplat();
    test_load_aligned();
    test_vcombine();

    puts(err ? "FAIL" : "PASS");
    return err ? 1 : 0;
}
