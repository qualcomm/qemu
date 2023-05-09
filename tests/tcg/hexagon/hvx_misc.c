/*
 *  Copyright(c) 2021-2022 Qualcomm Innovation Center, Inc. All Rights Reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <limits.h>
#include <assert.h>
#include "hvx_misc.h"

int err;

static void __check(int line, int i, int j, uint64_t result, uint64_t expect)
{
    if (result != expect) {
        printf("ERROR at line %d: [%d][%d] 0x%016llx != 0x%016llx\n",
               line, i, j, result, expect);
        err++;
    }
}

#define MAX_VEC_SIZE_BYTES         128

typedef union {
    uint64_t ud[MAX_VEC_SIZE_BYTES / 8];
    int64_t   d[MAX_VEC_SIZE_BYTES / 8];
    uint32_t uw[MAX_VEC_SIZE_BYTES / 4];
    int32_t   w[MAX_VEC_SIZE_BYTES / 4];
    uint16_t uh[MAX_VEC_SIZE_BYTES / 2];
    int16_t   h[MAX_VEC_SIZE_BYTES / 2];
    uint8_t  ub[MAX_VEC_SIZE_BYTES / 1];
    int8_t    b[MAX_VEC_SIZE_BYTES / 1];
    int32_t   sf[MAX_VEC_SIZE_BYTES / 4];
    int16_t   hf[MAX_VEC_SIZE_BYTES / 2];
} MMVector;

#define BUFSIZE      16
#define OUTSIZE      16
#define MASKMOD      3

MMVector buffer0[BUFSIZE] __attribute__((aligned(MAX_VEC_SIZE_BYTES)));
MMVector buffer1[BUFSIZE] __attribute__((aligned(MAX_VEC_SIZE_BYTES)));
MMVector sf_buffer0[BUFSIZE] __attribute__((aligned(MAX_VEC_SIZE_BYTES)));
MMVector sf_buffer1[BUFSIZE] __attribute__((aligned(MAX_VEC_SIZE_BYTES)));
MMVector hf_buffer0[BUFSIZE] __attribute__((aligned(MAX_VEC_SIZE_BYTES)));
MMVector hf_buffer1[BUFSIZE] __attribute__((aligned(MAX_VEC_SIZE_BYTES)));
MMVector mask[BUFSIZE] __attribute__((aligned(MAX_VEC_SIZE_BYTES)));
MMVector output[OUTSIZE] __attribute__((aligned(MAX_VEC_SIZE_BYTES)));
MMVector expect[OUTSIZE] __attribute__((aligned(MAX_VEC_SIZE_BYTES)));

int v6mpy_ref[BUFSIZE][MAX_VEC_SIZE_BYTES / 4] = {
#include "v6mpy_ref.h"
};

#define CHECK_OUTPUT_FUNC(FIELD, FIELDSZ) \
static void check_output_##FIELD(int line, size_t num_vectors) \
{ \
    for (int i = 0; i < num_vectors; i++) { \
        for (int j = 0; j < MAX_VEC_SIZE_BYTES / FIELDSZ; j++) { \
            __check(line, i, j, output[i].FIELD[j], expect[i].FIELD[j]); \
        } \
    } \
}

CHECK_OUTPUT_FUNC(d,  8)
CHECK_OUTPUT_FUNC(w,  4)
CHECK_OUTPUT_FUNC(h,  2)
CHECK_OUTPUT_FUNC(b,  1)

static void init_buffers(void)
{
    int counter0 = 0;
    int counter1 = 17;
    int sf_counter0 = 0;
    int sf_counter1 = 17;
    int hf_counter0 = 0;
    int hf_counter1 = 17;
    for (int i = 0; i < BUFSIZE; i++) {
        for (int j = 0; j < MAX_VEC_SIZE_BYTES; j++) {
            buffer0[i].b[j] = counter0++;
            buffer1[i].b[j] = counter1++;
        }
        for (int j = 0; j < MAX_VEC_SIZE_BYTES / 4; j++) {
            sf_buffer0[i].sf[j] = sf_counter0++;
            sf_buffer1[i].sf[j] = sf_counter1++;
        }
        for (int j = 0; j < MAX_VEC_SIZE_BYTES / 2; j++) {
            hf_buffer0[i].hf[j] = hf_counter0++;
            hf_buffer1[i].hf[j] = hf_counter1++;
        }
        for (int j = 0; j < MAX_VEC_SIZE_BYTES / 4; j++) {
            mask[i].w[j] = (i + j % MASKMOD == 0) ? 0 : 1;
        }
    }
}

static void test_load_tmp(void)
{
    void *p0 = buffer0;
    void *p1 = buffer1;
    void *pout = output;

    for (int i = 0; i < BUFSIZE; i++) {
        /*
         * Load into v12 as .tmp, then use it in the next packet
         * Should get the new value within the same packet and
         * the old value in the next packet
         */
        asm("v3 = vmem(%0 + #0)\n\t"
            "r1 = #1\n\t"
            "v12 = vsplat(r1)\n\t"
            "{\n\t"
            "    v12.tmp = vmem(%1 + #0)\n\t"
            "    v4.w = vadd(v12.w, v3.w)\n\t"
            "}\n\t"
            "v4.w = vadd(v4.w, v12.w)\n\t"
            "vmem(%2 + #0) = v4\n\t"
            : : "r"(p0), "r"(p1), "r"(pout)
            : "r1", "v12", "v3", "v4", "v6", "memory");
        p0 += sizeof(MMVector);
        p1 += sizeof(MMVector);
        pout += sizeof(MMVector);

        for (int j = 0; j < MAX_VEC_SIZE_BYTES / 4; j++) {
            expect[i].w[j] = buffer0[i].w[j] + buffer1[i].w[j] + 1;
        }
    }

    check_output_w(__LINE__, BUFSIZE);
}

static void test_load_tmp2(void)
{
    void *pout0 = &output[0];
    void *pout1 = &output[1];

    asm volatile(
        "r0 = #0x0\n\t"
        "v14 = vsplat(r0)\n\t"
        "v15 = vsplat(r0)\n\t"
        "v24 = vsplat(r0)\n\t"
        "v25 = vsplat(r0)\n\t"
        "r0 = #0x03030303\n\t"
        "v16 = vsplat(r0)\n\t"
        "r0 = #0x04040404\n\t"
        "v18 = vsplat(r0)\n\t"
        "r0 = #0x05050505\n\t"
        "v21 = vsplat(r0)\n\t"
        "{\n\t"
        "   v25:24 += vmpyo(v18.w, v14.h)\n\t"
        "   v15:14.tmp = vcombine(v21, v16)\n\t"
        "}\n\t"
        "vmem(%0 + #0) = v24\n\t"
        "vmem(%1 + #0) = v25\n\t"
        : : "r"(pout0), "r"(pout1)
        : "r0", "v14", "v15", "v16", "v18", "v21", "v24", "v25", "memory"
    );

    for (int i = 0; i < MAX_VEC_SIZE_BYTES / 4; ++i) {
        expect[0].w[i] = 0x180c0000;
        expect[1].w[i] = 0x000c1818;
    }

    check_output_w(__LINE__, 2);
}

static void test_load_cur(void)
{
    void *p0 = buffer0;
    void *pout = output;

    for (int i = 0; i < BUFSIZE; i++) {
        asm("{\n\t"
            "    v2.cur = vmem(%0 + #0)\n\t"
            "    vmem(%1 + #0) = v2\n\t"
            "}\n\t"
            : : "r"(p0), "r"(pout) : "v2", "memory");
        p0 += sizeof(MMVector);
        pout += sizeof(MMVector);

        for (int j = 0; j < MAX_VEC_SIZE_BYTES / 4; j++) {
            expect[i].uw[j] = buffer0[i].uw[j];
        }
    }

    check_output_w(__LINE__, BUFSIZE);
}

static void test_load_aligned(void)
{
    /* Aligned loads ignore the low bits of the address */
    void *p0 = buffer0;
    void *pout = output;
    const size_t offset = 13;

    p0 += offset;    /* Create an unaligned address */
    asm("v2 = vmem(%0 + #0)\n\t"
        "vmem(%1 + #0) = v2\n\t"
        : : "r"(p0), "r"(pout) : "v2", "memory");

    expect[0] = buffer0[0];

    check_output_w(__LINE__, 1);
}

static void test_load_unaligned(void)
{
    void *p0 = buffer0;
    void *pout = output;
    const size_t offset = 12;

    p0 += offset;    /* Create an unaligned address */
    asm("v2 = vmemu(%0 + #0)\n\t"
        "vmem(%1 + #0) = v2\n\t"
        : : "r"(p0), "r"(pout) : "v2", "memory");

    memcpy(expect, &buffer0[0].ub[offset], sizeof(MMVector));

    check_output_w(__LINE__, 1);
}

static void test_store_aligned(void)
{
    /* Aligned stores ignore the low bits of the address */
    void *p0 = buffer0;
    void *pout = output;
    const size_t offset = 13;

    pout += offset;    /* Create an unaligned address */
    asm("v2 = vmem(%0 + #0)\n\t"
        "vmem(%1 + #0) = v2\n\t"
        : : "r"(p0), "r"(pout) : "v2", "memory");

    expect[0] = buffer0[0];

    check_output_w(__LINE__, 1);
}

static void test_store_unaligned(void)
{
    void *p0 = buffer0;
    void *pout = output;
    const size_t offset = 12;

    pout += offset;    /* Create an unaligned address */
    asm("v2 = vmem(%0 + #0)\n\t"
        "vmemu(%1 + #0) = v2\n\t"
        : : "r"(p0), "r"(pout) : "v2", "memory");

    memcpy(expect, buffer0, 2 * sizeof(MMVector));
    memcpy(&expect[0].ub[offset], buffer0, sizeof(MMVector));

    check_output_w(__LINE__, 2);
}

static void test_masked_store(bool invert)
{
    void *p0 = buffer0;
    void *pmask = mask;
    void *pout = output;

    memset(expect, 0xff, sizeof(expect));
    memset(output, 0xff, sizeof(expect));

    for (int i = 0; i < BUFSIZE; i++) {
        if (invert) {
            asm("r4 = #0\n\t"
                "v4 = vsplat(r4)\n\t"
                "v5 = vmem(%0 + #0)\n\t"
                "q0 = vcmp.eq(v4.w, v5.w)\n\t"
                "v5 = vmem(%1)\n\t"
                "if (!q0) vmem(%2) = v5\n\t"             /* Inverted test */
                : : "r"(pmask), "r"(p0), "r"(pout)
                : "r4", "v4", "v5", "q0", "memory");
        } else {
            asm("r4 = #0\n\t"
                "v4 = vsplat(r4)\n\t"
                "v5 = vmem(%0 + #0)\n\t"
                "q0 = vcmp.eq(v4.w, v5.w)\n\t"
                "v5 = vmem(%1)\n\t"
                "if (q0) vmem(%2) = v5\n\t"             /* Non-inverted test */
                : : "r"(pmask), "r"(p0), "r"(pout)
                : "r4", "v4", "v5", "q0", "memory");
        }
        p0 += sizeof(MMVector);
        pmask += sizeof(MMVector);
        pout += sizeof(MMVector);

        for (int j = 0; j < MAX_VEC_SIZE_BYTES / 4; j++) {
            if (invert) {
                if (i + j % MASKMOD != 0) {
                    expect[i].w[j] = buffer0[i].w[j];
                }
            } else {
                if (i + j % MASKMOD == 0) {
                    expect[i].w[j] = buffer0[i].w[j];
                }
            }
        }
    }

    check_output_w(__LINE__, BUFSIZE);
}

static void test_new_value_store(void)
{
    void *p0 = buffer0;
    void *pout = output;

    asm("{\n\t"
        "    v2 = vmem(%0 + #0)\n\t"
        "    vmem(%1 + #0) = v2.new\n\t"
        "}\n\t"
        : : "r"(p0), "r"(pout) : "v2", "memory");

    expect[0] = buffer0[0];

    check_output_w(__LINE__, 1);
}

static void test_max_temps()
{
    void *p0 = buffer0;
    void *pout = output;

    asm("v0 = vmem(%0 + #0)\n\t"
        "v1 = vmem(%0 + #1)\n\t"
        "v2 = vmem(%0 + #2)\n\t"
        "v3 = vmem(%0 + #3)\n\t"
        "v4 = vmem(%0 + #4)\n\t"
        "{\n\t"
        "    v1:0.w = vadd(v3:2.w, v1:0.w)\n\t"
        "    v2.b = vshuffe(v3.b, v2.b)\n\t"
        "    v3.w = vadd(v1.w, v4.w)\n\t"
        "    v4.tmp = vmem(%0 + #5)\n\t"
        "}\n\t"
        "vmem(%1 + #0) = v0\n\t"
        "vmem(%1 + #1) = v1\n\t"
        "vmem(%1 + #2) = v2\n\t"
        "vmem(%1 + #3) = v3\n\t"
        "vmem(%1 + #4) = v4\n\t"
        : : "r"(p0), "r"(pout) : "memory");

        /* The first two vectors come from the vadd-pair instruction */
        for (int i = 0; i < MAX_VEC_SIZE_BYTES / 4; i++) {
            expect[0].w[i] = buffer0[0].w[i] + buffer0[2].w[i];
            expect[1].w[i] = buffer0[1].w[i] + buffer0[3].w[i];
        }
        /* The third vector comes from the vshuffe instruction */
        for (int i = 0; i < MAX_VEC_SIZE_BYTES / 2; i++) {
            expect[2].uh[i] = (buffer0[2].uh[i] & 0xff) |
                              (buffer0[3].uh[i] & 0xff) << 8;
        }
        /* The fourth vector comes from the vadd-single instruction */
        for (int i = 0; i < MAX_VEC_SIZE_BYTES / 4; i++) {
            expect[3].w[i] = buffer0[1].w[i] + buffer0[5].w[i];
        }
        /*
         * The fifth vector comes from the load to v4
         * make sure the .tmp is dropped
         */
        expect[4] = buffer0[4];

        check_output_b(__LINE__, 5);
}

#define VEC_OP1(ASM, EL, IN, OUT) \
    asm("v2 = vmem(%0 + #0)\n\t" \
        "v2" #EL " = " #ASM "(v2" #EL ")\n\t" \
        "vmem(%1 + #0) = v2\n\t" \
        : : "r"(IN), "r"(OUT) : "v2", "memory")

#define VEC_OP2(ASM, EL, IN0, IN1, OUT) \
    asm("v2 = vmem(%0 + #0)\n\t" \
        "v3 = vmem(%1 + #0)\n\t" \
        "v2" #EL " = " #ASM "(v2" #EL ", v3" #EL ")\n\t" \
        "vmem(%2 + #0) = v2\n\t" \
        : : "r"(IN0), "r"(IN1), "r"(OUT) : "v2", "v3", "memory")

#define TEST_VEC_OP1(NAME, ASM, EL, FIELD, FIELDSZ, OP) \
static void test_##NAME(void) \
{ \
    void *pin = buffer0; \
    void *pout = output; \
    for (int i = 0; i < BUFSIZE; i++) { \
        VEC_OP1(ASM, EL, pin, pout); \
        pin += sizeof(MMVector); \
        pout += sizeof(MMVector); \
    } \
    for (int i = 0; i < BUFSIZE; i++) { \
        for (int j = 0; j < MAX_VEC_SIZE_BYTES / FIELDSZ; j++) { \
            expect[i].FIELD[j] = OP buffer0[i].FIELD[j]; \
        } \
    } \
    check_output_##FIELD(__LINE__, BUFSIZE); \
}

#define TEST_VEC_OP2(NAME, ASM, EL, FIELD, FIELDSZ, OP) \
static void test_##NAME(void) \
{ \
    void *p0 = buffer0; \
    void *p1 = buffer1; \
    void *pout = output; \
    for (int i = 0; i < BUFSIZE; i++) { \
        VEC_OP2(ASM, EL, p0, p1, pout); \
        p0 += sizeof(MMVector); \
        p1 += sizeof(MMVector); \
        pout += sizeof(MMVector); \
    } \
    for (int i = 0; i < BUFSIZE; i++) { \
        for (int j = 0; j < MAX_VEC_SIZE_BYTES / FIELDSZ; j++) { \
            expect[i].FIELD[j] = buffer0[i].FIELD[j] OP buffer1[i].FIELD[j]; \
        } \
    } \
    check_output_##FIELD(__LINE__, BUFSIZE); \
}

#define THRESHOLD        31

#define PRED_OP2(ASM, IN0, IN1, OUT, INV) \
    asm("r4 = #%3\n\t" \
        "v1.b = vsplat(r4)\n\t" \
        "v2 = vmem(%0 + #0)\n\t" \
        "q0 = vcmp.gt(v2.b, v1.b)\n\t" \
        "v3 = vmem(%1 + #0)\n\t" \
        "q1 = vcmp.gt(v3.b, v1.b)\n\t" \
        "q2 = " #ASM "(q0, " INV "q1)\n\t" \
        "r4 = #0xff\n\t" \
        "v1.b = vsplat(r4)\n\t" \
        "if (q2) vmem(%2 + #0) = v1\n\t" \
        : : "r"(IN0), "r"(IN1), "r"(OUT), "i"(THRESHOLD) \
        : "r4", "v1", "v2", "v3", "q0", "q1", "q2", "memory")

#define TEST_PRED_OP2(NAME, ASM, OP, INV) \
static void test_##NAME(bool invert) \
{ \
    void *p0 = buffer0; \
    void *p1 = buffer1; \
    void *pout = output; \
    memset(output, 0, sizeof(expect)); \
    for (int i = 0; i < BUFSIZE; i++) { \
        PRED_OP2(ASM, p0, p1, pout, INV); \
        p0 += sizeof(MMVector); \
        p1 += sizeof(MMVector); \
        pout += sizeof(MMVector); \
    } \
    for (int i = 0; i < BUFSIZE; i++) { \
        for (int j = 0; j < MAX_VEC_SIZE_BYTES; j++) { \
            bool p0 = (buffer0[i].b[j] > THRESHOLD); \
            bool p1 = (buffer1[i].b[j] > THRESHOLD); \
            if (invert) { \
                expect[i].b[j] = (p0 OP !p1) ? 0xff : 0x00; \
            } else { \
                expect[i].b[j] = (p0 OP p1) ? 0xff : 0x00; \
            } \
        } \
    } \
    check_output_b(__LINE__, BUFSIZE); \
}

TEST_VEC_OP2(vadd_w, vadd, .w, w, 4, +)
TEST_VEC_OP2(vadd_h, vadd, .h, h, 2, +)
TEST_VEC_OP2(vadd_b, vadd, .b, b, 1, +)
TEST_VEC_OP2(vsub_w, vsub, .w, w, 4, -)
TEST_VEC_OP2(vsub_h, vsub, .h, h, 2, -)
TEST_VEC_OP2(vsub_b, vsub, .b, b, 1, -)
TEST_VEC_OP2(vxor, vxor, , d, 8, ^)
TEST_VEC_OP2(vand, vand, , d, 8, &)
TEST_VEC_OP2(vor, vor, , d, 8, |)
TEST_VEC_OP1(vnot, vnot, , d, 8, ~)

TEST_PRED_OP2(pred_or, or, |, "")
TEST_PRED_OP2(pred_or_n, or, |, "!")
TEST_PRED_OP2(pred_and, and, &, "")
TEST_PRED_OP2(pred_and_n, and, &, "!")
TEST_PRED_OP2(pred_xor, xor, ^, "")

#define TOFLOAT32(X)         (*(float *)(&(X)))
#define HF2SF(X)             (__extendhfsf2(X))

#ifndef TOOLCHAIN_WORKAROUND
#define TOOLCHAIN_WORKAROUND          1
#endif

static void test_vgtsf(void)
{
    void *p0 = sf_buffer0;
    void *p1 = sf_buffer1;
    void *pout = output;

    memset(expect, 0xff, sizeof(expect));
    memset(output, 0xff, sizeof(expect));

    for (int i = 0; i < BUFSIZE; i++) {
        asm("v4 = vmem(%0 + #0)\n\t"
            "v5 = vmem(%1 + #0)\n\t"
#if TOOLCHAIN_WORKAROUND
            ".word 0x1c85e470\n\t" /* q0 = vcmp.gt(v4.sf, v5.sf) */
#else
            "q0 = vcmp.gt(v4.sf, v5.sf)\n\t"
#endif
            "r4 = #0\n\t"
            "v5 = vsplat(r4)\n\t"
            "if (q0) vmem(%2) = v5\n\t"
            : : "r"(p0), "r"(p1), "r"(pout)
            : "r4", "v4", "v5", "q0", "memory");
        p0 += sizeof(MMVector);
        p1 += sizeof(MMVector);
        pout += sizeof(MMVector);

        for (int j = 0; j < MAX_VEC_SIZE_BYTES / 4; j++) {
            if (TOFLOAT32(sf_buffer0[i].sf[j]) >
                TOFLOAT32(sf_buffer1[i].sf[j])) {
                expect[i].w[j] = 0;
            }
        }
    }

    check_output_w(__LINE__, BUFSIZE);
}

static void test_vgthf(void)
{
    void *p0 = hf_buffer0;
    void *p1 = hf_buffer1;
    void *pout = output;

    memset(expect, 0xff, sizeof(expect));
    memset(output, 0xff, sizeof(expect));

    for (int i = 0; i < BUFSIZE; i++) {
        asm("v4 = vmem(%0 + #0)\n\t"
            "v5 = vmem(%1 + #0)\n\t"
#if TOOLCHAIN_WORKAROUND
            ".word 0x1c85e474\n\t"  /* q0 = vcmp.gt(v4.hf, v5.hf) */
#else
            "q0 = vcmp.gt(v4.hf, v5.hf)\n\t"
#endif
            "r4 = #0\n\t"
            "v5 = vsplat(r4)\n\t"
            "if (q0) vmem(%2) = v5\n\t"
            : : "r"(p0), "r"(p1), "r"(pout)
            : "r4", "v4", "v5", "q0", "memory");
        p0 += sizeof(MMVector);
        p1 += sizeof(MMVector);
        pout += sizeof(MMVector);

        for (int j = 0; j < MAX_VEC_SIZE_BYTES / 2; j++) {
            if (HF2SF(hf_buffer0[i].hf[j]) >
                HF2SF(hf_buffer1[i].hf[j])) {
                expect[i].h[j] = 0;
            }
        }
    }

    check_output_w(__LINE__, BUFSIZE);
}

static void test_vmax_sf(void)
{
    void *p0 = sf_buffer0;
    void *p1 = sf_buffer1;
    void *pout = output;

    memset(expect, 0xff, sizeof(expect));
    memset(output, 0xff, sizeof(expect));

    for (int i = 0; i < BUFSIZE; i++) {
        asm("v4 = vmem(%0 + #0)\n\t"
            "v5 = vmem(%1 + #0)\n\t"
#if TOOLCHAIN_WORKAROUND
            ".word 0x1fc5e425\n\t"   /* v5.sf = vmax(v4.sf, v5.sf) */
#else
            "v5.sf = vmax(v4.sf, v5.sf)\n\t"
#endif
            "vmem(%2 + #0) = v5\n\t"
            : : "r"(p0), "r"(p1), "r"(pout)
            : "v4", "v5", "memory");
        p0 += sizeof(MMVector);
        p1 += sizeof(MMVector);
        pout += sizeof(MMVector);

        for (int j = 0; j < MAX_VEC_SIZE_BYTES / 4; j++) {
            if (TOFLOAT32(sf_buffer0[i].sf[j]) >
                TOFLOAT32(sf_buffer1[i].sf[j])) {
                expect[i].sf[j] = sf_buffer0[i].sf[j];
            } else {
                expect[i].sf[j] = sf_buffer1[i].sf[j];
            }
        }
    }

    check_output_w(__LINE__, BUFSIZE);
}

static void test_vmin_sf(void)
{
    void *p0 = sf_buffer0;
    void *p1 = sf_buffer1;
    void *pout = output;

    memset(expect, 0xff, sizeof(expect));
    memset(output, 0xff, sizeof(expect));

    for (int i = 0; i < BUFSIZE; i++) {
        asm("v4 = vmem(%0 + #0)\n\t"
            "v5 = vmem(%1 + #0)\n\t"
#if TOOLCHAIN_WORKAROUND
            ".word 0x1fc5e445\n\t"  /* v5.sf = vmin(v4.sf, v5.sf) */
#else
            "v5.sf = vmin(v4.sf, v5.sf)\n\t"
#endif
            "vmem(%2 + #0) = v5\n\t"
            : : "r"(p0), "r"(p1), "r"(pout)
            : "v4", "v5", "memory");
        p0 += sizeof(MMVector);
        p1 += sizeof(MMVector);
        pout += sizeof(MMVector);

        for (int j = 0; j < MAX_VEC_SIZE_BYTES / 4; j++) {
            if (TOFLOAT32(sf_buffer0[i].sf[j]) >
                TOFLOAT32(sf_buffer1[i].sf[j])) {
                expect[i].sf[j] = sf_buffer1[i].sf[j];
            } else {
                expect[i].sf[j] = sf_buffer0[i].sf[j];
            }
        }
    }

    check_output_w(__LINE__, BUFSIZE);
}

static void test_vmax_hf(void)
{
    void *p0 = hf_buffer0;
    void *p1 = hf_buffer1;
    void *pout = output;

    memset(expect, 0xff, sizeof(expect));
    memset(output, 0xff, sizeof(expect));

    for (int i = 0; i < BUFSIZE; i++) {
        asm("v4 = vmem(%0 + #0)\n\t"
            "v5 = vmem(%1 + #0)\n\t"
#if TOOLCHAIN_WORKAROUND
            ".word 0x1fc5e465\n\t"   /* v5.hf = vmax(v4.hf, v5.hf) */
#else
            "v5.hf = vmax(v4.hf, v5.hf)\n\t"
#endif
            "vmem(%2 + #0) = v5\n\t"
            : : "r"(p0), "r"(p1), "r"(pout)
            : "v4", "v5", "memory");
        p0 += sizeof(MMVector);
        p1 += sizeof(MMVector);
        pout += sizeof(MMVector);

        for (int j = 0; j < MAX_VEC_SIZE_BYTES / 2; j++) {
            if (HF2SF(hf_buffer0[i].hf[j]) >
                HF2SF(hf_buffer1[i].hf[j])) {
                expect[i].hf[j] = hf_buffer0[i].hf[j];
            } else {
                expect[i].hf[j] = hf_buffer1[i].hf[j];
            }
        }
    }

    check_output_h(__LINE__, BUFSIZE);
}

static void test_vmin_hf(void)
{
    void *p0 = hf_buffer0;
    void *p1 = hf_buffer1;
    void *pout = output;

    memset(expect, 0xff, sizeof(expect));
    memset(output, 0xff, sizeof(expect));

    for (int i = 0; i < BUFSIZE; i++) {
        asm("v4 = vmem(%0 + #0)\n\t"
            "v5 = vmem(%1 + #0)\n\t"
#if TOOLCHAIN_WORKAROUND
            ".word 0x1fc5e485\n\t"   /* v5.hf = vmin(v4.hf, v5.hf) */
#else
            "v5.hf = vmin(v4.hf, v5.hf)\n\t"
#endif
            "vmem(%2 + #0) = v5\n\t"
            : : "r"(p0), "r"(p1), "r"(pout)
            : "v4", "v5", "memory");
        p0 += sizeof(MMVector);
        p1 += sizeof(MMVector);
        pout += sizeof(MMVector);

        for (int j = 0; j < MAX_VEC_SIZE_BYTES / 2; j++) {
            if (HF2SF(hf_buffer0[i].hf[j]) >
                HF2SF(hf_buffer1[i].hf[j])) {
                expect[i].hf[j] = hf_buffer1[i].hf[j];
            } else {
                expect[i].hf[j] = hf_buffer0[i].hf[j];
            }
        }
    }

    check_output_h(__LINE__, BUFSIZE);
}

static void test_v6mpy(void)
{
    void *p00 = buffer0;
    void *p01 = sf_buffer0;
    void *p10 = buffer1;
    void *p11 = sf_buffer1;
    void *pout = output;

    memset(expect, 0xff, sizeof(expect));
    memset(output, 0xff, sizeof(expect));

    for (int i = 0; i < BUFSIZE; i++) {
        asm("v2 = vmem(%0 + #0)\n\t"
            "v3 = vmem(%1 + #0)\n\t"
            "v4 = vmem(%2 + #0)\n\t"
            "v5 = vmem(%3 + #0)\n\t"
            "v5:4.w = v6mpy(v5:4.ub, v3:2.b, #1):v\n\t"
            "vmem(%4 + #0) = v4\n\t"
            : : "r"(p00), "r"(p01), "r"(p10), "r"(p11), "r"(pout)
            : "v2", "v3", "v4", "v5", "memory");
        p00 += sizeof(MMVector);
        p01 += sizeof(MMVector);
        p10 += sizeof(MMVector);
        p11 += sizeof(MMVector);
        pout += sizeof(MMVector);

        for (int j = 0; j < MAX_VEC_SIZE_BYTES / 4; j++) {
            expect[i].w[j] = v6mpy_ref[i][j];
        }
    }

    check_output_w(__LINE__, BUFSIZE);
}

static void test_vadduwsat(void)
{
    /*
     * Test for saturation by adding two numbers that add to more than UINT_MAX
     * and make sure the result saturates to UINT_MAX
     */
    const uint32_t x = 0xffff0000;
    const uint32_t y = 0x000fffff;

    memset(expect, 0x12, sizeof(MMVector));
    memset(output, 0x34, sizeof(MMVector));

    asm volatile ("v10 = vsplat(%0)\n\t"
                  "v11 = vsplat(%1)\n\t"
                  "v21.uw = vadd(v11.uw, v10.uw):sat\n\t"
                  "vmem(%2+#0) = v21\n\t"
                  : /* no outputs */
                  : "r"(x), "r"(y), "r"(output)
                  : "v10", "v11", "v21", "memory");

    for (int j = 0; j < MAX_VEC_SIZE_BYTES / 4; j++) {
        expect[0].uw[j] = UINT_MAX;
    }

    check_output_w(__LINE__, 1);
}

static void test_vsubuwsat_dv(void)
{
    /*
     * Test for saturation by subtracting two numbers where the result is
     * negative and make sure the result saturates to zero
     *
     * vsubuwsat_dv operates on an HVX register pair, so we'll have a
     * pair of subtractions
     *     w - x < 0
     *     y - z < 0
     */
    const uint32_t w = 0x000000b7;
    const uint32_t x = 0xffffff4e;
    const uint32_t y = 0x31fe88e7;
    const uint32_t z = 0x7fffff79;

    memset(expect, 0x12, sizeof(MMVector) * 2);
    memset(output, 0x34, sizeof(MMVector) * 2);

    asm volatile ("v16 = vsplat(%0)\n\t"
                  "v17 = vsplat(%1)\n\t"
                  "v26 = vsplat(%2)\n\t"
                  "v27 = vsplat(%3)\n\t"
                  "v25:24.uw = vsub(v17:16.uw, v27:26.uw):sat\n\t"
                  "vmem(%4+#0) = v24\n\t"
                  "vmem(%4+#1) = v25\n\t"
                  : /* no outputs */
                  : "r"(w), "r"(y), "r"(x), "r"(z), "r"(output)
                  : "v16", "v17", "v24", "v25", "v26", "v27", "memory");

    for (int j = 0; j < MAX_VEC_SIZE_BYTES / 4; j++) {
        expect[0].uw[j] = 0x00000000;
        expect[1].uw[j] = 0x00000000;
    }

    check_output_w(__LINE__, 2);
}

#define TEST_V6_VCOMBINE_TMP(reg1, reg2) \
{ \
    assert(reg1 > 3 && reg2 > 3); \
    memset(output, 0, sizeof(MMVector) * 2); \
    expect[0] = buffer0[0]; \
    expect[1] = buffer0[1]; \
    asm volatile("v0 = vmem(%0)\n\t" \
                 "v1 = vmem(%1)\n\t" \
                 "{\n\t" \
                 "    v" #reg1 ":" #reg2 ".tmp=vcombine(v0,v1)\n\t" \
                 "    v3:2 = v" #reg1 ":" #reg2 "\n\t" \
                 "}\n\t" \
                 "vmemu(%2) = v3\n\t" \
                 "vmemu(%3) = v2\n\t" \
                 : /* no outputs */ \
                 : "r"(&buffer0[0]), "r"(&buffer0[1]), "r"(&output[0]), "r"(&output[1]) \
                 : "v0", "v1", "v2", "v3", "memory"); \
    check_output_b(__LINE__, 2); \
} while (0) \

static void test_v6_vcombine_tmp_alig(void)
{
    TEST_V6_VCOMBINE_TMP(9, 8);
}

static void test_v6_vcombine_tmp_unalig(void)
{
    TEST_V6_VCOMBINE_TMP(8, 9);
}

static void test_load_tmp_predicated(void)
{
    void *p0 = buffer0;
    void *p1 = buffer1;
    void *pout = output;
    bool pred = true;

    for (int i = 0; i < BUFSIZE; i++) {
        /*
         * Load into v12 as .tmp with a predicate
         * When the predicate is true, we get the vector from buffer1[i]
         * When the predicate is false, we get a vector of all 1's
         * Regardless of the predicate, the next packet should have
         * a vector of all 1's
         */
        asm("v3 = vmem(%0 + #0)\n\t"
            "r1 = #1\n\t"
            "v12 = vsplat(r1)\n\t"
            "p1 = !cmp.eq(%3, #0)\n\t"
            "{\n\t"
            "    if (p1) v12.tmp = vmem(%1 + #0)\n\t"
            "    v4.w = vadd(v12.w, v3.w)\n\t"
            "}\n\t"
            "v4.w = vadd(v4.w, v12.w)\n\t"
            "vmem(%2 + #0) = v4\n\t"
            : : "r"(p0), "r"(p1), "r"(pout), "r"(pred)
            : "r1", "p1", "v12", "v3", "v4", "v6", "memory");
        p0 += sizeof(MMVector);
        p1 += sizeof(MMVector);
        pout += sizeof(MMVector);

        for (int j = 0; j < MAX_VEC_SIZE_BYTES / 4; j++) {
            expect[i].w[j] =
                pred ? buffer0[i].w[j] + buffer1[i].w[j] + 1
                     : buffer0[i].w[j] + 2;
        }
        pred = !pred;
    }

    check_output_w(__LINE__, BUFSIZE);
}

static void test_load_cur_predicated(void)
{
    bool pred = true;
    for (int i = 0; i < BUFSIZE; i++) {
        asm volatile("p0 = !cmp.eq(%3, #0)\n\t"
                     "v3 = vmem(%0+#0)\n\t"
                     /*
                      * Preload v4 to make sure that the assignment from the
                      * packet below is not being ignored when pred is false.
                      */
                     "r0 = #0x01237654\n\t"
                     "v4 = vsplat(r0)\n\t"
                     "{\n\t"
                     "    if (p0) v3.cur = vmem(%1+#0)\n\t"
                     "    v4 = v3\n\t"
                     "}\n\t"
                     "vmem(%2+#0) = v4\n\t"
                     :
                     : "r"(&buffer0[i]), "r"(&buffer1[i]),
                       "r"(&output[i]), "r"(pred)
                     : "r0", "p0", "v3", "v4", "memory");
        expect[i] = pred ? buffer1[i] : buffer0[i];
        pred = !pred;
    }
    check_output_w(__LINE__, BUFSIZE);
}

int main()
{
    init_buffers();

    test_load_tmp();
    test_load_tmp2();
    test_load_cur();
    test_load_aligned();
    test_load_unaligned();
    test_store_aligned();
    test_store_unaligned();
    test_masked_store(false);
    test_masked_store(true);
    test_new_value_store();
    test_max_temps();

    test_vadd_w();
    test_vadd_h();
    test_vadd_b();
    test_vsub_w();
    test_vsub_h();
    test_vsub_b();
    test_vxor();
    test_vand();
    test_vor();
    test_vnot();

    test_pred_or(false);
    test_pred_or_n(true);
    test_pred_and(false);
    test_pred_and_n(true);
    test_pred_xor(false);

    test_vgtsf();
    test_vgthf();
    test_vmax_sf();
    test_vmin_sf();
    test_vmax_hf();
    test_vmin_hf();

    test_v6mpy();

    test_vadduwsat();
    test_vsubuwsat_dv();

    test_v6_vcombine_tmp_alig();
    test_v6_vcombine_tmp_unalig();

    test_load_tmp_predicated();
    test_load_cur_predicated();

    puts(err ? "FAIL" : "PASS");
    return err ? 1 : 0;
}
