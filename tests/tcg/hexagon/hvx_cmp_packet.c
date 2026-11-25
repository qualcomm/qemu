/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 * Test the HVX vector compares that write a predicate: every element size
 * and condition, the forms that combine with the predicate, and a packet
 * with four of them, which is as large as a packet of compares gets.
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include <hexagon_types.h>
#include <hvx_hexagon_protos.h>

#define VBYTES 128

static int err;

static uint8_t in_a[VBYTES] __attribute__((aligned(VBYTES)));
static uint8_t in_b[VBYTES] __attribute__((aligned(VBYTES)));
static uint8_t out[4][VBYTES] __attribute__((aligned(VBYTES)));

static uint32_t rng = 12345;

static uint32_t next(void)
{
    rng = rng * 1664525u + 1013904223u;
    return rng >> 8;
}

/*
 * The operands agree in most lanes, at every width, and otherwise differ in
 * the sign, in the high bits or in the low bits, so that every compare is
 * true for some lanes and false for others.
 */
static void fill(void)
{
    for (int i = 0; i < VBYTES; i++) {
        in_a[i] = next();
        in_b[i] = (next() & 3) ? in_a[i] : next();
    }
    for (int i = 0; i < VBYTES; i += 8) {
        /* Make the lanes of some groups equal as whole words */
        if (next() & 1) {
            memcpy(in_b + i, in_a + i, 8);
        }
    }
}

static void swap_ab(void)
{
    uint8_t tmp[VBYTES];

    memcpy(tmp, in_a, VBYTES);
    memcpy(in_a, in_b, VBYTES);
    memcpy(in_b, tmp, VBYTES);
}

/* A predicate bit per byte, as a vector of 1s and 0s. */
static void store_q(int slot, HVX_VectorPred q)
{
    HVX_Vector v = Q6_V_vand_QR(q, 0x01010101);

    memcpy(out[slot], &v, VBYTES);
}

typedef enum { GT, GTU, EQ } cond_t;

static int lane_true(cond_t c, int size, int lane)
{
    int64_t sa = 0, sb = 0;
    uint64_t ua = 0, ub = 0;

    for (int k = 0; k < size; k++) {
        ua |= (uint64_t)in_a[lane * size + k] << (8 * k);
        ub |= (uint64_t)in_b[lane * size + k] << (8 * k);
    }
    sa = (int64_t)(ua << (64 - 8 * size)) >> (64 - 8 * size);
    sb = (int64_t)(ub << (64 - 8 * size)) >> (64 - 8 * size);
    switch (c) {
    case GT:
        return sa > sb;
    case GTU:
        return ua > ub;
    default:
        return ua == ub;
    }
}

/* The predicate for a compare, one byte per predicate bit. */
static void expect_q(uint8_t *want, cond_t c, int size)
{
    for (int lane = 0; lane < VBYTES / size; lane++) {
        for (int k = 0; k < size; k++) {
            want[lane * size + k] = lane_true(c, size, lane);
        }
    }
}

static void check_slot(const char *what, int slot, const uint8_t *want)
{
    if (memcmp(out[slot], want, VBYTES) != 0) {
        printf("ERROR %s\n", what);
        for (int i = 0; i < VBYTES; i++) {
            if (out[slot][i] != want[i]) {
                printf("  byte %d: %d != %d\n", i, out[slot][i], want[i]);
                break;
            }
        }
        err++;
    }
}

#define TEST_CMP(NAME, INTRIN, COND, SIZE) \
    do { \
        uint8_t want[VBYTES]; \
        HVX_Vector a = *(HVX_Vector *)in_a, b = *(HVX_Vector *)in_b; \
        \
        store_q(0, INTRIN(a, b)); \
        expect_q(want, COND, SIZE); \
        check_slot(NAME, 0, want); \
    } while (0)

/* Combine the compare with a predicate that has every other byte set. */
#define TEST_CMP_OP(NAME, INTRIN, COND, SIZE, OP) \
    do { \
        uint8_t want[VBYTES]; \
        HVX_Vector a = *(HVX_Vector *)in_a, b = *(HVX_Vector *)in_b; \
        HVX_VectorPred q = Q6_Q_vand_VR(Q6_V_vsplat_R(0x00ff00ff), \
                                        0x01010101); \
        \
        q = INTRIN(q, a, b); \
        store_q(0, q); \
        expect_q(want, COND, SIZE); \
        for (int i = 0; i < VBYTES; i++) { \
            int old = (i & 1) == 0; \
            want[i] = OP; \
        } \
        check_slot(NAME, 0, want); \
    } while (0)

static void test_compares(void)
{
    TEST_CMP("gtb", Q6_Q_vcmp_gt_VbVb, GT, 1);
    TEST_CMP("gth", Q6_Q_vcmp_gt_VhVh, GT, 2);
    TEST_CMP("gtw", Q6_Q_vcmp_gt_VwVw, GT, 4);
    TEST_CMP("gtub", Q6_Q_vcmp_gt_VubVub, GTU, 1);
    TEST_CMP("gtuh", Q6_Q_vcmp_gt_VuhVuh, GTU, 2);
    TEST_CMP("gtuw", Q6_Q_vcmp_gt_VuwVuw, GTU, 4);
    TEST_CMP("eqb", Q6_Q_vcmp_eq_VbVb, EQ, 1);
    TEST_CMP("eqh", Q6_Q_vcmp_eq_VhVh, EQ, 2);
    TEST_CMP("eqw", Q6_Q_vcmp_eq_VwVw, EQ, 4);
}

static void test_compare_ops(void)
{
    TEST_CMP_OP("gtb and", Q6_Q_vcmp_gtand_QVbVb, GT, 1, old & want[i]);
    TEST_CMP_OP("gtb or", Q6_Q_vcmp_gtor_QVbVb, GT, 1, old | want[i]);
    TEST_CMP_OP("gtb xor", Q6_Q_vcmp_gtxacc_QVbVb, GT, 1, old ^ want[i]);
    TEST_CMP_OP("eqh and", Q6_Q_vcmp_eqand_QVhVh, EQ, 2, old & want[i]);
    TEST_CMP_OP("eqw or", Q6_Q_vcmp_eqor_QVwVw, EQ, 4, old | want[i]);
    TEST_CMP_OP("gtuw xor", Q6_Q_vcmp_gtxacc_QVuwVuw, GTU, 4,
                old ^ want[i]);
}

/*
 * Four byte compares in one packet, the largest that a packet of compares
 * gets.  It is translated as one unit, and must not be too large to
 * translate.
 */
static void test_compare_packet(void)
{
    uint8_t want[VBYTES];

    asm volatile("v0 = vmem(%0 + #0)\n\t"
                 "v1 = vmem(%1 + #0)\n\t"
                 "r0 = ##0x01010101\n\t"
                 "{ q0 = vcmp.gt(v0.b, v1.b)\n\t"
                 "  q1 = vcmp.gt(v1.b, v0.b)\n\t"
                 "  q2 = vcmp.eq(v0.b, v1.b)\n\t"
                 "  q3 = vcmp.gt(v0.ub, v1.ub) }\n\t"
                 "v2 = vand(q0, r0)\n\t"
                 "v3 = vand(q1, r0)\n\t"
                 "v4 = vand(q2, r0)\n\t"
                 "v5 = vand(q3, r0)\n\t"
                 "vmem(%2 + #0) = v2\n\t"
                 "vmem(%2 + #1) = v3\n\t"
                 "vmem(%2 + #2) = v4\n\t"
                 "vmem(%2 + #3) = v5\n\t"
                 : : "r"(in_a), "r"(in_b), "r"(out)
                 : "r0", "v0", "v1", "v2", "v3", "v4", "v5",
                   "q0", "q1", "q2", "q3", "memory");

    expect_q(want, GT, 1);
    check_slot("packet gtb", 0, want);
    swap_ab();
    expect_q(want, GT, 1);
    swap_ab();
    check_slot("packet gtb, swapped", 1, want);
    expect_q(want, EQ, 1);
    check_slot("packet eqb", 2, want);
    expect_q(want, GTU, 1);
    check_slot("packet gtub", 3, want);
}

int main(void)
{
    for (int round = 0; round < 8; round++) {
        fill();
        test_compares();
        test_compare_ops();
        test_compare_packet();
    }

    puts(err ? "FAIL" : "PASS");
    return err ? 1 : 0;
}
