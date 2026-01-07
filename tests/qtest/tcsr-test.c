/*
 * QTest testcase for TCSR (Top Control and Status Register) device
 *
 * Copyright (c) 2025 Qualcomm Technologies, Inc. All Rights Reserved.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "libqtest.h"
#include "qemu/module.h"

#define TCSR_BASE     0x01fc0000

/* Register offsets */
#define REG_SOC_EMULATION_TYPE       0x8004
#define REG_SOC_HW_VERSION          0x8000
#define REG_SW_WONCE_SOC_HW_VERSION 0x8008
#define REG_TZ_WONCE_BASE           0x4000

/* SOC_HW_VERSION fields */
#define SOC_HW_VERSION_MINOR_VERSION_SHIFT 0
#define SOC_HW_VERSION_MINOR_VERSION_MASK  0xFF
#define SOC_HW_VERSION_MAJOR_VERSION_SHIFT 8
#define SOC_HW_VERSION_MAJOR_VERSION_MASK  0xFF
#define SOC_HW_VERSION_DEVICE_NUMBER_SHIFT 16
#define SOC_HW_VERSION_DEVICE_NUMBER_MASK  0xFFF
#define SOC_HW_VERSION_FAMILY_NUMBER_SHIFT 28
#define SOC_HW_VERSION_FAMILY_NUMBER_MASK  0xF

static void test_tcsr_soc_hw_version(void)
{
    QTestState *qts;
    uint32_t value;
    uint32_t expected;

    qts = qtest_init("-machine SA8775P_CDSP0");

    /* Read SOC_HW_VERSION register */
    value = qtest_readl(qts, TCSR_BASE + REG_SOC_HW_VERSION);

    /* Default values: family=0xE, device=0x875, major=0x1, minor=0x0 */
    expected = (0xE << SOC_HW_VERSION_FAMILY_NUMBER_SHIFT) |
               (0x875 << SOC_HW_VERSION_DEVICE_NUMBER_SHIFT) |
               (0x1 << SOC_HW_VERSION_MAJOR_VERSION_SHIFT) |
               (0x0 << SOC_HW_VERSION_MINOR_VERSION_SHIFT);

    g_assert_cmpuint(value, ==, expected);

    /* Verify the register is read-only by attempting to write */
    qtest_writel(qts, TCSR_BASE + REG_SOC_HW_VERSION, 0xDEADBEEF);
    value = qtest_readl(qts, TCSR_BASE + REG_SOC_HW_VERSION);
    g_assert_cmpuint(value, ==, expected);

    qtest_quit(qts);
}

static void test_tcsr_soc_emulation_type(void)
{
    QTestState *qts;
    uint32_t value;

    qts = qtest_init("-machine SA8775P_CDSP0");

    /* Read SOC_EMULATION_TYPE register */
    value = qtest_readl(qts, TCSR_BASE + REG_SOC_EMULATION_TYPE);

    /* Default emulation type is 0 */
    g_assert_cmpuint(value, ==, 0x0);

    /* Verify the register is read-only */
    qtest_writel(qts, TCSR_BASE + REG_SOC_EMULATION_TYPE, 0x12345678);
    value = qtest_readl(qts, TCSR_BASE + REG_SOC_EMULATION_TYPE);
    g_assert_cmpuint(value, ==, 0x0);

    qtest_quit(qts);
}

static void test_tcsr_sw_wonce_soc_hw_version(void)
{
    QTestState *qts;
    uint32_t value;
    uint32_t initial_value;

    qts = qtest_init("-machine SA8775P_CDSP0");

    /* Read initial value - should match SOC_HW_VERSION */
    initial_value = qtest_readl(qts, TCSR_BASE + REG_SW_WONCE_SOC_HW_VERSION);
    /* Initial value should match default SOC_HW_VERSION */
    g_assert_cmpuint(initial_value, ==, 0xE8750100);

    /* Write a new value - this should succeed once */
    qtest_writel(qts, TCSR_BASE + REG_SW_WONCE_SOC_HW_VERSION, 0x12345678);
    value = qtest_readl(qts, TCSR_BASE + REG_SW_WONCE_SOC_HW_VERSION);
    g_assert_cmpuint(value, ==, 0x12345678);

    /* Try to write again - this should fail (write-once) */
    qtest_writel(qts, TCSR_BASE + REG_SW_WONCE_SOC_HW_VERSION, 0x87654321);
    value = qtest_readl(qts, TCSR_BASE + REG_SW_WONCE_SOC_HW_VERSION);
    g_assert_cmpuint(value, ==, 0x12345678); /* Value should not change */

    qtest_quit(qts);
}

static void test_tcsr_tz_wonce(void)
{
    QTestState *qts;
    uint32_t value;
    int i;

    qts = qtest_init("-machine SA8775P_CDSP0");

    /* Test a few TZ_WONCE registers */
    for (i = 0; i < 4; i++) {
        uint32_t offset = REG_TZ_WONCE_BASE + (i * 4);

        /* Initial value - register 0 has special init value */
        value = qtest_readl(qts, TCSR_BASE + offset);
        if (i == 0) {
            g_assert_cmpuint(value, ==, 0x90aff320);
        } else {
            g_assert_cmpuint(value, ==, 0x0);
        }

        /* Write a value - should succeed once */
        uint32_t test_val = 0xCAFE0000 | i;
        qtest_writel(qts, TCSR_BASE + offset, test_val);
        value = qtest_readl(qts, TCSR_BASE + offset);
        g_assert_cmpuint(value, ==, test_val);

        /* Write again - should fail (write-once) */
        qtest_writel(qts, TCSR_BASE + offset, 0xDEADBEEF);
        value = qtest_readl(qts, TCSR_BASE + offset);
        g_assert_cmpuint(value, ==, test_val); /* Value should not change */
    }

    qtest_quit(qts);
}

static void test_tcsr_wonce_addr_init_value(void)
{
    QTestState *qts;
    uint32_t value;

    qts = qtest_init("-machine SA8775P_CDSP0");

    /* Read TZ_WONCE[0] - should have the initialized value 0x90aff320 */
    value = qtest_readl(qts, TCSR_BASE + REG_TZ_WONCE_BASE);
    g_assert_cmpuint(value, ==, 0x90aff320);

    qtest_quit(qts);
}

static void test_tcsr_multiple_wonce_init(void)
{
    QTestState *qts;
    uint32_t value;
    int i;

    qts = qtest_init("-machine SA8775P_CDSP0");

    /* Verify WONCE[0] has the initialized value */
    value = qtest_readl(qts, TCSR_BASE + REG_TZ_WONCE_BASE);
    g_assert_cmpuint(value, ==, 0x90aff320);

    /* Verify the rest are zero */
    for (i = 1; i < 8; i++) {
        value = qtest_readl(qts, TCSR_BASE + REG_TZ_WONCE_BASE + i * 4);
        g_assert_cmpuint(value, ==, 0);
    }

    /* Test that WONCE registers are write-once */
    qtest_writel(qts, TCSR_BASE + REG_TZ_WONCE_BASE, 0xdeadbeef);
    value = qtest_readl(qts, TCSR_BASE + REG_TZ_WONCE_BASE);
    g_assert_cmpuint(value, ==, 0xdeadbeef);

    /* Second write should fail */
    qtest_writel(qts, TCSR_BASE + REG_TZ_WONCE_BASE, 0xcafebabe);
    value = qtest_readl(qts, TCSR_BASE + REG_TZ_WONCE_BASE);
    g_assert_cmpuint(value, ==, 0xdeadbeef); /* Value shouldn't change */

    qtest_quit(qts);
}

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);

    qtest_add_func("/tcsr/soc_hw_version", test_tcsr_soc_hw_version);
    qtest_add_func("/tcsr/soc_emulation_type", test_tcsr_soc_emulation_type);
    qtest_add_func("/tcsr/sw_wonce_soc_hw_version",
                   test_tcsr_sw_wonce_soc_hw_version);
    qtest_add_func("/tcsr/tz_wonce", test_tcsr_tz_wonce);
    qtest_add_func("/tcsr/wonce_addr_init_value",
                   test_tcsr_wonce_addr_init_value);
    qtest_add_func("/tcsr/multiple_wonce_init",
                   test_tcsr_multiple_wonce_init);

    return g_test_run();
}
