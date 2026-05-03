/*
 * QTest testcase for Watchdog (WDOG) device
 *
 * Copyright (c) 2025 Qualcomm Technologies, Inc. All Rights Reserved.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "libqtest.h"

#define WDOG_BASE 0x26384000  /* AHBS base (0x26300000) + 0x84000 */
#define REG_MAGIC_OFFSET 0xc
#define REG_MAGIC_VALUE  0xdeadbeef

static void test_wdog_magic_register(void)
{
    QTestState *qts;
    uint32_t value;

    qts = qtest_init("-machine SA8775P_CDSP0");

    /* Read magic register at offset 0xc - should be 0xdeadbeef */
    value = qtest_readl(qts, WDOG_BASE + REG_MAGIC_OFFSET);
    g_assert_cmpuint(value, ==, REG_MAGIC_VALUE);

    qtest_quit(qts);
}

static void test_wdog_read_write(void)
{
    QTestState *qts;
    uint32_t value;
    int i;

    qts = qtest_init("-machine SA8775P_CDSP0");

    /* Test writing and reading various registers */
    for (i = 0; i < 0x10; i++) {
        uint32_t offset = i * 4;
        uint32_t test_val = 0x12340000 + i;

        /* Skip the magic register */
        if (offset == REG_MAGIC_OFFSET) {
            continue;
        }

        /* Write test value */
        qtest_writel(qts, WDOG_BASE + offset, test_val);

        /* Read back and verify */
        value = qtest_readl(qts, WDOG_BASE + offset);
        g_assert_cmpuint(value, ==, test_val);
    }

    qtest_quit(qts);
}

static void test_wdog_magic_register_writable(void)
{
    QTestState *qts;
    uint32_t value;

    qts = qtest_init("-machine SA8775P_CDSP0");

    /* Verify magic register can be overwritten */
    qtest_writel(qts, WDOG_BASE + REG_MAGIC_OFFSET, 0xbeefdead);
    value = qtest_readl(qts, WDOG_BASE + REG_MAGIC_OFFSET);
    g_assert_cmpuint(value, ==, 0xbeefdead);

    qtest_quit(qts);
}

static void test_wdog_boundary(void)
{
    QTestState *qts;
    uint32_t value;

    qts = qtest_init("-machine SA8775P_CDSP0");

    /* Test last valid register */
    qtest_writel(qts, WDOG_BASE + 0x3fc, 0x99999999);
    value = qtest_readl(qts, WDOG_BASE + 0x3fc);
    g_assert_cmpuint(value, ==, 0x99999999);

    /* Test that reads from outside range return 0 */
    value = qtest_readl(qts, WDOG_BASE + 0x400);
    g_assert_cmpuint(value, ==, 0);

    qtest_quit(qts);
}

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);

    qtest_add_func("/wdog/magic_register", test_wdog_magic_register);
    qtest_add_func("/wdog/read_write", test_wdog_read_write);
    qtest_add_func("/wdog/magic_register_writable",
                   test_wdog_magic_register_writable);
    qtest_add_func("/wdog/boundary", test_wdog_boundary);

    return g_test_run();
}
