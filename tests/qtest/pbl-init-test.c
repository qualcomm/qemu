/*
 * QTest for the pbl_init device
 *
 * Copyright (c) 2026 Pierrick Bouvier
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "libqtest-single.h"

/* Inside RAM on virt: the device must take precedence over it. */
#define BASE        0x40800000ULL
#define CHUNK       0x1000ULL
#define NR_CHUNKS   4
#define SIZE        (CHUNK * NR_CHUNKS)

static QTestState *start(void)
{
    return qtest_initf("-M virt -device pbl_init,addr=0x%llx,size=0x%llx,"
                       "chunk-size=0x%llx", BASE, SIZE, CHUNK);
}

static uint64_t chunk(int i)
{
    return BASE + i * CHUNK;
}

/* Full sequence: initial, first write, deferred writes, commit, normal. */
static void test_sequence(void)
{
    QTestState *qts = start();
    int i;

    /* initial: everything reads as 0 */
    for (i = 0; i < NR_CHUNKS; i++) {
        g_assert_cmphex(qtest_readq(qts, chunk(i)), ==, 0);
        g_assert_cmphex(qtest_readq(qts, chunk(i) + CHUNK - 8), ==, 0);
    }

    /* first write to chunk 0 is visible */
    qtest_writeq(qts, chunk(0), 0x1111111111111111ULL);
    g_assert_cmphex(qtest_readq(qts, chunk(0)), ==, 0x1111111111111111ULL);

    /* next writes to chunk 0 are not visible, anywhere in the chunk */
    qtest_writeq(qts, chunk(0), 0x2222222222222222ULL);
    qtest_writeq(qts, chunk(0) + 0x800, 0x3333333333333333ULL);
    g_assert_cmphex(qtest_readq(qts, chunk(0)), ==, 0x1111111111111111ULL);
    g_assert_cmphex(qtest_readq(qts, chunk(0) + 0x800), ==, 0);

    /* first writes to all but the last chunk: still not committed */
    for (i = 1; i < NR_CHUNKS - 1; i++) {
        qtest_writel(qts, chunk(i) + 4, 0xa0000000 + i);
        qtest_writel(qts, chunk(i) + 8, 0xb0000000 + i);
        g_assert_cmphex(qtest_readl(qts, chunk(i) + 4), ==, 0xa0000000 + i);
        g_assert_cmphex(qtest_readl(qts, chunk(i) + 8), ==, 0);
    }
    g_assert_cmphex(qtest_readq(qts, chunk(0)), ==, 0x1111111111111111ULL);

    /* first write to the last chunk commits all deferred writes */
    qtest_writeb(qts, chunk(NR_CHUNKS - 1), 0x42);
    g_assert_cmphex(qtest_readq(qts, chunk(0)), ==, 0x2222222222222222ULL);
    g_assert_cmphex(qtest_readq(qts, chunk(0) + 0x800), ==,
                    0x3333333333333333ULL);
    for (i = 1; i < NR_CHUNKS - 1; i++) {
        g_assert_cmphex(qtest_readl(qts, chunk(i) + 4), ==, 0xa0000000 + i);
        g_assert_cmphex(qtest_readl(qts, chunk(i) + 8), ==, 0xb0000000 + i);
    }
    g_assert_cmphex(qtest_readb(qts, chunk(NR_CHUNKS - 1)), ==, 0x42);

    /* normal memory from now on */
    qtest_writeq(qts, chunk(0), 0x4444444444444444ULL);
    g_assert_cmphex(qtest_readq(qts, chunk(0)), ==, 0x4444444444444444ULL);
    qtest_writew(qts, chunk(2) + 0x10, 0xbeef);
    g_assert_cmphex(qtest_readw(qts, chunk(2) + 0x10), ==, 0xbeef);

    qtest_quit(qts);
}

/* Only the bytes of the first write access become visible. */
static void test_partial_first_write(void)
{
    QTestState *qts = start();

    qtest_writeb(qts, chunk(1) + 3, 0x5a);
    qtest_writeb(qts, chunk(1) + 4, 0xa5);
    g_assert_cmphex(qtest_readq(qts, chunk(1)), ==, 0x000000005a000000ULL);

    qtest_quit(qts);
}

/* An unaligned access straddling two chunks is a first write to both. */
static void test_straddle(void)
{
    QTestState *qts = start();

    qtest_writel(qts, chunk(1) - 2, 0xddccbbaa);
    g_assert_cmphex(qtest_readw(qts, chunk(1) - 2), ==, 0xbbaa);
    g_assert_cmphex(qtest_readw(qts, chunk(1)), ==, 0xddcc);

    /* both chunks are now first-written: next writes are deferred */
    qtest_writew(qts, chunk(0), 0x1234);
    qtest_writew(qts, chunk(1) + 2, 0x5678);
    g_assert_cmphex(qtest_readw(qts, chunk(0)), ==, 0);
    g_assert_cmphex(qtest_readw(qts, chunk(1) + 2), ==, 0);

    qtest_quit(qts);
}

/* A system reset brings the device back to its initial state. */
static void test_reset(void)
{
    QTestState *qts = start();
    int i;

    for (i = 0; i < NR_CHUNKS; i++) {
        qtest_writeq(qts, chunk(i), 0x1000 + i);
    }
    qtest_writeq(qts, chunk(0), 0x9999);
    g_assert_cmphex(qtest_readq(qts, chunk(0)), ==, 0x9999);

    qtest_system_reset(qts);

    g_assert_cmphex(qtest_readq(qts, chunk(0)), ==, 0);
    qtest_writeq(qts, chunk(0), 0x7777);
    qtest_writeq(qts, chunk(0), 0x8888);
    g_assert_cmphex(qtest_readq(qts, chunk(0)), ==, 0x7777);

    qtest_quit(qts);
}

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);

    qtest_add_func("/pbl_init/sequence", test_sequence);
    qtest_add_func("/pbl_init/partial_first_write", test_partial_first_write);
    qtest_add_func("/pbl_init/straddle", test_straddle);
    qtest_add_func("/pbl_init/reset", test_reset);

    return g_test_run();
}
