/*
 *  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 * HLX memory operation tests (predicated load/store)
 *
 * Tests: LX_L16b_p_tmp, LX_S16b_p_new
 *
 * Predicated load/store have no intrinsic equivalents, so the test
 * operations use inline assembly.  Buffers are BSS-allocated (not VTCM)
 * because mixing HLX intrinsics with HLX inline asm in the same
 * compilation unit causes the compiler to generate invalid packets.
 *
 * NOTE: This test only runs on QEMU.  On hexagon-sim the HLX mem()
 * instructions require VTCM addresses, but VTCM initialisation via
 * HLX intrinsics conflicts with the inline asm HLX instructions.
 */

#include "hlx_test.h"

/* Committed-value seed for the false-predicate load test */
#define SEED_VAL  0x1234

/* Single-register buffers */
static uint16_t buf_a[HLX_HALF_ELEMS] HLX_ALIGN;
static uint16_t buf_b[HLX_HALF_ELEMS] HLX_ALIGN;
static uint16_t buf_out[HLX_HALF_ELEMS] HLX_ALIGN;
static uint16_t expected[HLX_HALF_ELEMS];

static void init_data(void)
{
    for (int i = 0; i < HLX_HALF_ELEMS; i++) {
        buf_a[i] = (i * 37 + 100) & 0xFFFF;
        buf_b[i] = (i * 23 + 200) & 0xFFFF;
        buf_out[i] = 0xDEAD;
    }
}

/* ===== Predicated load ===== */

static void test_pred_load_true(void)
{
    init_data();
    /*
     * if (Pv) Ld.tmp = mem(Rs) -- with true predicate.
     *
     * A .tmp load only forwards its data within the same packet, so the
     * consuming assign (L1.h = L0.h) and the .new store must live in the
     * same packet as the load.
     */
    asm volatile(
        "p0 = cmp.eq(r0, r0)\n"     /* p0 = true */
        "{ if (p0) L0.tmp = mem(%0)\n"
        "  L1.h = L0.h\n"
        "  mem(%1) = L1.new }\n"
        : : "r"(buf_a), "r"(buf_out) : "memory"
    );
    check_h("LX_L16b_p_tmp (true)", buf_out, buf_a, HLX_HALF_ELEMS);
}

static void test_pred_load_false(void)
{
    init_data();
    /*
     * if (Pv) Ld.tmp = mem(Rs) -- with false predicate.
     *
     * A .tmp load never writes the register file; it only forwards within
     * its packet.  With a false predicate the load is cancelled, so the
     * same-packet consumer (L1.h = L0.h) reads whatever committed value L0
     * already holds.  Seed L0 with a regular (non-tmp) HLX write first, then
     * confirm the cancelled tmp load does not override it.
     */
    uint32_t seed = SEED_VAL;
    asm volatile(
        "{ L0.h = %0.h }\n"          /* seed committed L0 with SEED_VAL */
        "p0 = !cmp.eq(r0, r0)\n"     /* p0 = false (always) */
        "{ if (p0) L0.tmp = mem(%1)\n"
        "  L1.h = L0.h\n"
        "  mem(%2) = L1.new }\n"
        : : "r"(seed), "r"(buf_a), "r"(buf_out) : "memory"
    );
    for (int i = 0; i < HLX_HALF_ELEMS; i++) {
        expected[i] = SEED_VAL;
    }
    check_h("LX_L16b_p_tmp (false)", buf_out, expected, HLX_HALF_ELEMS);
}

/* ===== Predicated store ===== */

static void test_pred_store_true(void)
{
    init_data();
    /*
     * if (Pv) mem(Rs) = Ku.new -- with true predicate.
     *
     * The .tmp load, its consumer, and the predicated .new store all live
     * in one packet so the loaded data is forwarded correctly.
     */
    asm volatile(
        "p0 = cmp.eq(r0, r0)\n"     /* p0 = true */
        "{ L0.tmp = mem(%0)\n"
        "  L1.h = L0.h\n"
        "  if (p0) mem(%1) = L1.new }\n"
        : : "r"(buf_a), "r"(buf_out) : "memory"
    );
    check_h("LX_S16b_p_new (true)", buf_out, buf_a, HLX_HALF_ELEMS);
}

static void test_pred_store_false(void)
{
    init_data();
    /*
     * if (Pv) mem(Rs) = Ku.new -- with false predicate.
     * Same same-packet producer pattern, but the false predicate cancels
     * the store; buf_out retains 0xDEAD.
     */
    asm volatile(
        "p0 = !cmp.eq(r0, r0)\n"    /* p0 = false (always) */
        "{ L0.tmp = mem(%0)\n"
        "  L1.h = L0.h\n"
        "  if (p0) mem(%1) = L1.new }\n"
        : : "r"(buf_a), "r"(buf_out) : "memory"
    );
    for (int i = 0; i < HLX_HALF_ELEMS; i++) {
        expected[i] = 0xDEAD;
    }
    check_h("LX_S16b_p_new (false)", buf_out, expected, HLX_HALF_ELEMS);
}

int main(void)
{
    enable_hlx();

    /* Predicated load */
    test_pred_load_true();
    test_pred_load_false();

    /* Predicated store */
    test_pred_store_true();
    test_pred_store_false();

    puts(err ? "FAIL" : "PASS");
    return err;
}
