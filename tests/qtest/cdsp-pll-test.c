/*
 * QTest testcase for CDSP PLL device
 *
 * Copyright (c) 2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "libqtest.h"

/* PLL register offsets */
#define CDSP_PLL_MODE                0x200
#define CDSP_PLL_L_VAL               0x204
#define CDSP_PLL_CAL_L_VAL           0x208
#define CDSP_PLL_USER_CTL            0x20c
#define CDSP_PLL_USER_CTL_U          0x210
#define CDSP_PLL_CONFIG_CTL          0x214
#define CDSP_PLL_CONFIG_CTL_U        0x218
#define CDSP_PLL_TEST_CTL            0x21c
#define CDSP_PLL_TEST_CTL_U          0x220
#define CDSP_PLL_STATUS              0x224
#define CDSP_PLL_FREQ_CTL            0x228
#define CDSP_PLL_OPMODE              0x238
#define CDSP_PLL_STATE               0x23c
#define CDSP_PLL_DROOP               0x234
#define CDSP_PLL_SPARE               0x23c
#define CDSP_PLL_SSC_DELTA_ALPHA     0x240
#define CDSP_PLL_SSC_UPDATE_RATE     0x244
#define CDSP_PLL_SSC_NUM_STEPS       0x248
#define CDSP_PLL_RCG_UPDATE_STATUS   0x250
#define CDSP_PLL_RCG_UPDATE_CFG      0x254
#define CDSP_PLL_RCG_UPDATE_DLYCTL   0x258
#define CDSP_PLL_RCG_UPDATE_CMD      0x25c
#define CDSP_PLL_CORE_CFG_RCGR       0x02c
#define CDSP_PLL_CORE_CMD_RCGR       0x028

/* PLL mode register bit definitions */
#define CDSP_PLL_MODE_OUTCTRL        (1 << 0)
#define CDSP_PLL_MODE_BYPASSNL       (1 << 1)
#define CDSP_PLL_MODE_RESET_N        (1 << 2)
#define CDSP_PLL_MODE_LOCK_DET       (1 << 31)
#define CDSP_PLL_MODE_UPDATE         (1 << 22)
#define CDSP_PLL_MODE_ACK_LATCH      (1 << 29)

/* PLL user control register bit definitions */
#define CDSP_PLL_USER_CTL_PLLOUT_MAIN    (1 << 0)

/* PLL operation modes */
#define CDSP_PLL_OPMODE_STANDBY      0
#define CDSP_PLL_OPMODE_RUN          1

/* PLL base address in virt machine */
#define PLL_BASE 0x26300000

static void test_pll_reset_state(void)
{
    QTestState *qts = qtest_init("-machine virt");
    uint32_t val;

    /* Test initial reset values */
    val = qtest_readl(qts, PLL_BASE + CDSP_PLL_MODE);
    g_assert_cmphex(val, ==, 0); /* PLL should be in reset initially */

    val = qtest_readl(qts, PLL_BASE + CDSP_PLL_OPMODE);
    g_assert_cmphex(val, ==, CDSP_PLL_OPMODE_STANDBY);

    val = qtest_readl(qts, PLL_BASE + CDSP_PLL_L_VAL);
    g_assert_cmphex(val, ==, 62); /* Default L value */

    val = qtest_readl(qts, PLL_BASE + CDSP_PLL_CAL_L_VAL);
    g_assert_cmphex(val, ==, 62); /* Should match L_VAL initially */

    qtest_quit(qts);
}

static void test_pll_lock_sequence(void)
{
    QTestState *qts = qtest_init("-machine virt");
    uint32_t val;

    /* Test PLL locking sequence */

    /* 1. Take PLL out of reset */
    val = qtest_readl(qts, PLL_BASE + CDSP_PLL_MODE);
    val |= CDSP_PLL_MODE_RESET_N;
    qtest_writel(qts, PLL_BASE + CDSP_PLL_MODE, val);

    /* 2. Set operation mode to RUN */
    qtest_writel(qts, PLL_BASE + CDSP_PLL_OPMODE, CDSP_PLL_OPMODE_RUN);

    /* 3. Check that PLL locks */
    val = qtest_readl(qts, PLL_BASE + CDSP_PLL_MODE);
    g_assert_true(val & CDSP_PLL_MODE_LOCK_DET);

    /* 4. Enable main output */
    val = qtest_readl(qts, PLL_BASE + CDSP_PLL_USER_CTL);
    val |= CDSP_PLL_USER_CTL_PLLOUT_MAIN;
    qtest_writel(qts, PLL_BASE + CDSP_PLL_USER_CTL, val);

    /* 5. Enable global outputs */
    val = qtest_readl(qts, PLL_BASE + CDSP_PLL_MODE);
    val |= CDSP_PLL_MODE_OUTCTRL;
    qtest_writel(qts, PLL_BASE + CDSP_PLL_MODE, val);

    qtest_quit(qts);
}

static void test_pll_lval_update(void)
{
    QTestState *qts = qtest_init("-machine virt");
    uint32_t val;

    /* Test L-value update mechanism */

    /* Set PLL to running state first */
    val = CDSP_PLL_MODE_RESET_N;
    qtest_writel(qts, PLL_BASE + CDSP_PLL_MODE, val);
    qtest_writel(qts, PLL_BASE + CDSP_PLL_OPMODE, CDSP_PLL_OPMODE_RUN);

    /* Write new L value */
    qtest_writel(qts, PLL_BASE + CDSP_PLL_L_VAL, 100);

    /* Trigger latch update */
    val = qtest_readl(qts, PLL_BASE + CDSP_PLL_MODE);
    val |= CDSP_PLL_MODE_UPDATE;
    qtest_writel(qts, PLL_BASE + CDSP_PLL_MODE, val);

    /* Check that CAL_L_VAL was updated and ack bit is set */
    val = qtest_readl(qts, PLL_BASE + CDSP_PLL_CAL_L_VAL);
    g_assert_cmphex(val, ==, 100);

    val = qtest_readl(qts, PLL_BASE + CDSP_PLL_MODE);
    g_assert_true(val & CDSP_PLL_MODE_ACK_LATCH);

    /* Clear update bit */
    val &= ~CDSP_PLL_MODE_UPDATE;
    qtest_writel(qts, PLL_BASE + CDSP_PLL_MODE, val);

    /* Check that ack bit is cleared */
    val = qtest_readl(qts, PLL_BASE + CDSP_PLL_MODE);
    g_assert_false(val & CDSP_PLL_MODE_ACK_LATCH);

    qtest_quit(qts);
}

static void test_pll_rcg_command(void)
{
    QTestState *qts = qtest_init("-machine virt");
    uint32_t val;

    /* Test RCG command auto-clear mechanism */

    /* Write configuration */
    qtest_writel(qts, PLL_BASE + CDSP_PLL_CORE_CFG_RCGR, 0x201);

    /* Issue RCG command */
    qtest_writel(qts, PLL_BASE + CDSP_PLL_CORE_CMD_RCGR, 0x1);

    /* Command bit should auto-clear */
    val = qtest_readl(qts, PLL_BASE + CDSP_PLL_CORE_CMD_RCGR);
    g_assert_cmphex(val, ==, 0);

    qtest_quit(qts);
}

static void test_pll_register_access(void)
{
    QTestState *qts = qtest_init("-machine virt");
    uint32_t test_value = 0x12345678;
    uint32_t val;

    /* Test read/write access to various registers */

    /* Test L_VAL register */
    qtest_writel(qts, PLL_BASE + CDSP_PLL_L_VAL, test_value);
    val = qtest_readl(qts, PLL_BASE + CDSP_PLL_L_VAL);
    g_assert_cmphex(val, ==, test_value);

    /* Test CONFIG_CTL register */
    qtest_writel(qts, PLL_BASE + CDSP_PLL_CONFIG_CTL, test_value);
    val = qtest_readl(qts, PLL_BASE + CDSP_PLL_CONFIG_CTL);
    g_assert_cmphex(val, ==, test_value);

    /* Test FREQ_CTL register */
    qtest_writel(qts, PLL_BASE + CDSP_PLL_FREQ_CTL, test_value);
    val = qtest_readl(qts, PLL_BASE + CDSP_PLL_FREQ_CTL);
    g_assert_cmphex(val, ==, test_value);

    qtest_quit(qts);
}

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);

    qtest_add_func("/cdsp-pll/reset-state", test_pll_reset_state);
    qtest_add_func("/cdsp-pll/lock-sequence", test_pll_lock_sequence);
    qtest_add_func("/cdsp-pll/lval-update", test_pll_lval_update);
    qtest_add_func("/cdsp-pll/rcg-command", test_pll_rcg_command);
    qtest_add_func("/cdsp-pll/register-access", test_pll_register_access);

    return g_test_run();
}
