/*
 * QTest testcase for DSPSS_PUB device
 *
 * Copyright (c) 2025 Qualcomm Technologies, Inc. All Rights Reserved.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "libqtest.h"
#include "qemu/module.h"

/* CSR base address from machine config */
#define CSR_BASE 0x26300000

/* Register offsets */
#define REG_VERSION                        0x00000
#define REG_DBG_CFG                        0x00018
#define REG_RET_CFG                        0x0001C
#define REG_NMI                            0x00040
#define REG_NMI_STATUS                     0x00044
#define REG_STRAP_TCM_BASE_STATUS          0x00100
#define REG_BOOT_CORE_START                0x00400
#define REG_BOOT_CMD                       0x00404
#define REG_BOOT_STATUS                    0x00408
#define REG_MEM_STATUS                     0x00438
#define REG_L2MEM_EFUSE_STATUS             0x00490
#define REG_CP_CLK_CTL                     0x00508
#define REG_LMH_STATUS                     0x0081C
#define REG_HMX_STATUS                     0x02024
#define REG_CLK_STATUS                     0x02468

/* Register field values */
#define NMI_SET_MASK          0x01
#define NMI_CLEAR_STATUS_MASK 0x02
#define BOOT_CORE_MASK        0x01
#define BOOT_CMD_MASK         0x01

static void test_dspss_pub_version(void)
{
    QTestState *qts;
    uint32_t value;

    qts = qtest_init("-machine SA8775P_CDSP0");

    /* Read VERSION register */
    value = qtest_readl(qts, CSR_BASE + REG_VERSION);
    g_assert_cmpuint(value, ==, 0x10020000);

    qtest_quit(qts);
}

static void test_dspss_pub_dbg_cfg(void)
{
    QTestState *qts;
    uint32_t value;

    qts = qtest_init("-machine SA8775P_CDSP0");

    /* Initial value should be 0 */
    value = qtest_readl(qts, CSR_BASE + REG_DBG_CFG);
    g_assert_cmpuint(value, ==, 0x0);

    /* Write test value */
    qtest_writel(qts, CSR_BASE + REG_DBG_CFG, 0x12345678);
    value = qtest_readl(qts, CSR_BASE + REG_DBG_CFG);
    g_assert_cmpuint(value, ==, 0x12345678);

    /* Write another value */
    qtest_writel(qts, CSR_BASE + REG_DBG_CFG, 0x87654321);
    value = qtest_readl(qts, CSR_BASE + REG_DBG_CFG);
    g_assert_cmpuint(value, ==, 0x87654321);

    qtest_quit(qts);
}

static void test_dspss_pub_nmi(void)
{
    QTestState *qts;
    uint32_t value;

    qts = qtest_init("-machine SA8775P_CDSP0");

    /* NMI status should be clear initially */
    value = qtest_readl(qts, CSR_BASE + REG_NMI_STATUS);
    g_assert_cmpuint(value, ==, 0x0);

    /* Trigger NMI - should see pulse on GPIO */
    qtest_writel(qts, CSR_BASE + REG_NMI, NMI_SET_MASK);
    value = qtest_readl(qts, CSR_BASE + REG_NMI_STATUS);
    g_assert_cmpuint(value, ==, 0x1);
    /* NMI signal pulses high then low atomically - can't catch it mid-pulse */

    /* Clear NMI status */
    qtest_writel(qts, CSR_BASE + REG_NMI, NMI_CLEAR_STATUS_MASK);
    value = qtest_readl(qts, CSR_BASE + REG_NMI_STATUS);
    g_assert_cmpuint(value, ==, 0x0);

    /* Trigger and clear in same write */
    qtest_writel(qts, CSR_BASE + REG_NMI, NMI_SET_MASK | NMI_CLEAR_STATUS_MASK);
    value = qtest_readl(qts, CSR_BASE + REG_NMI_STATUS);
    g_assert_cmpuint(value, ==, 0x0); /* Clear takes precedence */

    qtest_quit(qts);
}

static void test_dspss_pub_boot(void)
{
    QTestState *qts;
    uint32_t value;

    qts = qtest_init("-machine SA8775P_CDSP0");

    /* Boot status should be 0 initially */
    value = qtest_readl(qts, CSR_BASE + REG_BOOT_STATUS);
    g_assert_cmpuint(value, ==, 0x0);

    /* Set boot core start */
    qtest_writel(qts, CSR_BASE + REG_BOOT_CORE_START, BOOT_CORE_MASK);

    /* Set boot command */
    qtest_writel(qts, CSR_BASE + REG_BOOT_CMD, BOOT_CMD_MASK);
    value = qtest_readl(qts, CSR_BASE + REG_BOOT_STATUS);
    g_assert_cmpuint(value, ==, 0x1);

    /* Clear boot command */
    qtest_writel(qts, CSR_BASE + REG_BOOT_CMD, 0x0);
    value = qtest_readl(qts, CSR_BASE + REG_BOOT_STATUS);
    g_assert_cmpuint(value, ==, 0x0);

    qtest_quit(qts);
}

static void test_dspss_pub_readonly_regs(void)
{
    QTestState *qts;
    uint32_t value;

    qts = qtest_init("-machine SA8775P_CDSP0");

    /* Test fixed value read-only registers */
    value = qtest_readl(qts, CSR_BASE + REG_MEM_STATUS);
    g_assert_cmpuint(value, ==, 0x1F001F);

    value = qtest_readl(qts, CSR_BASE + REG_L2MEM_EFUSE_STATUS);
    g_assert_cmpuint(value, ==, 0xF);

    value = qtest_readl(qts, CSR_BASE + REG_LMH_STATUS);
    g_assert_cmpuint(value, ==, 0x1);

    value = qtest_readl(qts, CSR_BASE + REG_HMX_STATUS);
    g_assert_cmpuint(value, ==, 0x14);

    value = qtest_readl(qts, CSR_BASE + REG_CLK_STATUS);
    g_assert_cmpuint(value, ==, 0x1FFF);

    qtest_quit(qts);
}

static void test_dspss_pub_cp_clk_ctl(void)
{
    QTestState *qts;
    uint32_t value;

    qts = qtest_init("-machine SA8775P_CDSP0");

    /* Initial value should be 0 */
    value = qtest_readl(qts, CSR_BASE + REG_CP_CLK_CTL);
    g_assert_cmpuint(value, ==, 0x0);

    /* Enable coprocessor */
    qtest_writel(qts, CSR_BASE + REG_CP_CLK_CTL, 0x1);
    value = qtest_readl(qts, CSR_BASE + REG_CP_CLK_CTL);
    g_assert_cmpuint(value, ==, 0x1);

    /* Write full 32-bit value */
    qtest_writel(qts, CSR_BASE + REG_CP_CLK_CTL, 0xFFFFFFFF);
    value = qtest_readl(qts, CSR_BASE + REG_CP_CLK_CTL);
    g_assert_cmpuint(value, ==, 0xFFFFFFFF);

    qtest_quit(qts);
}

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);

    qtest_add_func("/dspss_pub/version", test_dspss_pub_version);
    qtest_add_func("/dspss_pub/dbg_cfg", test_dspss_pub_dbg_cfg);
    qtest_add_func("/dspss_pub/nmi", test_dspss_pub_nmi);
    qtest_add_func("/dspss_pub/boot", test_dspss_pub_boot);
    qtest_add_func("/dspss_pub/readonly_regs", test_dspss_pub_readonly_regs);
    qtest_add_func("/dspss_pub/cp_clk_ctl", test_dspss_pub_cp_clk_ctl);

    return g_test_run();
}
