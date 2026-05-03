/*
 * QTest testcase for QCOM IPCC device
 *
 * This test exercises the IPCC (Inter-Processor Communication Controller)
 * device based on real-world usage patterns from the Linux kernel driver.
 *
 * Copyright (c) 2025 Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "libqtest.h"

/* Test machine and device configuration */
#define IPCC_BASE_ADDR 0x00400000

/* IPCC register offsets */
#define IPCC_REG_VERSION                    0x000
#define IPCC_REG_ID                         0x004
#define IPCC_REG_CONFIG                     0x008
#define IPCC_REG_SEND_ID                    0x00C
#define IPCC_REG_RECV_ID                    0x010
#define IPCC_REG_RECV_SIGNAL_ENABLE         0x014
#define IPCC_REG_RECV_SIGNAL_DISABLE        0x018
#define IPCC_REG_RECV_SIGNAL_CLEAR          0x01C
#define IPCC_REG_CLIENT_CLEAR               0x038
#define IPCC_REG_RECV_CLIENT_PRIORITY       0x100

/* Address calculation macros */
#define IPCC_PROTOCOL_SIZE  0x40000   /* 256KB per protocol */
#define IPCC_CLIENT_SIZE    0x1000    /* 4KB per client */

#define IPCC_CLIENT_ADDR(protocol, client) \
    (IPCC_BASE_ADDR + (protocol) * IPCC_PROTOCOL_SIZE + \
     (client) * IPCC_CLIENT_SIZE)

/* Register field definitions */
#define IPCC_CONFIG_CLEAR_ON_RECV_RD        0x00000001
#define IPCC_CONFIG_DISABLE_MODE            0x80000000
#define IPCC_SIGNAL_ID_MASK                 0x0000FFFF
#define IPCC_CLIENT_ID_MASK                 0xFFFF0000
#define IPCC_CLIENT_ID_SHIFT                16
#define IPCC_SEND_BROADCAST_FLAG            0x80000000
#define IPCC_NO_PENDING_IRQ                 0xFFFFFFFF
#define IPCC_DEFAULT_VERSION                0x10200

/* Test client IDs based on typical Qualcomm SoC configurations */
#define CLIENT_APPS         0
#define CLIENT_MODEM        1
#define CLIENT_ADSP         2
#define CLIENT_CDSP         3
#define CLIENT_SLPI         4

/* Test signal IDs */
#define SIGNAL_TEST         0
#define SIGNAL_WAKEUP       1
#define SIGNAL_SHUTDOWN     2

/* Helper functions for register access */
static uint32_t read_reg(QTestState *qts, uint32_t protocol,
                         uint32_t client, uint32_t offset)
{
    return qtest_readl(qts, IPCC_CLIENT_ADDR(protocol, client) + offset);
}

static void write_reg(QTestState *qts, uint32_t protocol,
                      uint32_t client, uint32_t offset, uint32_t value)
{
    qtest_writel(qts, IPCC_CLIENT_ADDR(protocol, client) + offset, value);
}

/*
 * Note: IRQ state verification is complex with L2VIC connections
 * For now, we verify IRQ logic through register state and functional testing.
 * The CDSP IRQ is connected to L2VIC IRQ 128 in hexagon_dsp.c
 */

static void verify_irq_register_logic(QTestState *qts, uint32_t client,
                                       bool should_have_pending)
{
    uint32_t recv_id = read_reg(qts, 0, client, IPCC_REG_RECV_ID);
    if (should_have_pending) {
        g_assert_cmpuint(recv_id, !=, IPCC_NO_PENDING_IRQ);
    } else {
        g_assert_cmpuint(recv_id, ==, IPCC_NO_PENDING_IRQ);
    }
}

/* Test basic register access and reset values */
static void test_ipcc_reset_values(void)
{
    QTestState *qts = qtest_init("-M SA8775P_CDSP0");
    uint32_t version, id, config, recv_id;

    /* Test protocol 0, client 0 (typically APPS processor) */
    version = read_reg(qts, 0, CLIENT_APPS, IPCC_REG_VERSION);
    g_assert_cmpuint(version, ==, IPCC_DEFAULT_VERSION);

    id = read_reg(qts, 0, CLIENT_APPS, IPCC_REG_ID);
    g_assert_cmpuint(id, ==, (CLIENT_APPS << 16) | 0);

    config = read_reg(qts, 0, CLIENT_APPS, IPCC_REG_CONFIG);
    g_assert_cmpuint(config, ==, 0);

    recv_id = read_reg(qts, 0, CLIENT_APPS, IPCC_REG_RECV_ID);
    g_assert_cmpuint(recv_id, ==, IPCC_NO_PENDING_IRQ);

    /* Verify IRQ register logic shows no pending IRQ at reset */
    verify_irq_register_logic(qts, CLIENT_CDSP, false);

    /* Test different client IDs have correct ID register */
    id = read_reg(qts, 0, CLIENT_MODEM, IPCC_REG_ID);
    g_assert_cmpuint(id, ==, (CLIENT_MODEM << 16) | 0);

    /* Test different protocols have correct ID register */
    id = read_reg(qts, 1, CLIENT_APPS, IPCC_REG_ID);
    g_assert_cmpuint(id, ==, (CLIENT_APPS << 16) | 1);

    qtest_quit(qts);
}

/* Test signal enable/disable functionality */
static void test_ipcc_signal_enable_disable(void)
{
    QTestState *qts = qtest_init("-M SA8775P_CDSP0");
    uint32_t hwirq, recv_id;

    /* Note: Only CDSP client has IRQ connected in this machine */

    /* Enable signal from CLIENT_MODEM to CLIENT_APPS */
    hwirq = (CLIENT_MODEM << IPCC_CLIENT_ID_SHIFT) | SIGNAL_WAKEUP;
    write_reg(qts, 0, CLIENT_APPS, IPCC_REG_RECV_SIGNAL_ENABLE, hwirq);

    /* Verify no pending IRQ initially */
    recv_id = read_reg(qts, 0, CLIENT_APPS, IPCC_REG_RECV_ID);
    g_assert_cmpuint(recv_id, ==, IPCC_NO_PENDING_IRQ);

    /* Send signal from MODEM to APPS */
    hwirq = (CLIENT_APPS << IPCC_CLIENT_ID_SHIFT) | SIGNAL_WAKEUP;
    write_reg(qts, 0, CLIENT_MODEM, IPCC_REG_SEND_ID, hwirq);

    /* Check that signal is now pending and IRQ logic indicates pending */
    recv_id = read_reg(qts, 0, CLIENT_APPS, IPCC_REG_RECV_ID);
    g_assert_cmpuint(recv_id, ==, (CLIENT_MODEM << 16) | SIGNAL_WAKEUP);
    verify_irq_register_logic(qts, CLIENT_APPS, true);

    /* Disable the signal */
    hwirq = (CLIENT_MODEM << IPCC_CLIENT_ID_SHIFT) | SIGNAL_WAKEUP;
    write_reg(qts, 0, CLIENT_APPS, IPCC_REG_RECV_SIGNAL_DISABLE, hwirq);

    /* Verify no pending IRQ after disable and IRQ logic shows no pending */
    recv_id = read_reg(qts, 0, CLIENT_APPS, IPCC_REG_RECV_ID);
    g_assert_cmpuint(recv_id, ==, IPCC_NO_PENDING_IRQ);
    verify_irq_register_logic(qts, CLIENT_APPS, false);

    qtest_quit(qts);
}

/* Test signal clearing functionality */
static void test_ipcc_signal_clear(void)
{
    QTestState *qts = qtest_init("-M SA8775P_CDSP0");
    uint32_t hwirq, recv_id;

    /* Enable signal from CLIENT_CDSP to CLIENT_APPS */
    hwirq = (CLIENT_CDSP << IPCC_CLIENT_ID_SHIFT) | SIGNAL_TEST;
    write_reg(qts, 0, CLIENT_APPS, IPCC_REG_RECV_SIGNAL_ENABLE, hwirq);

    /* Send signal from CDSP to APPS */
    hwirq = (CLIENT_APPS << IPCC_CLIENT_ID_SHIFT) | SIGNAL_TEST;
    write_reg(qts, 0, CLIENT_CDSP, IPCC_REG_SEND_ID, hwirq);

    /* Verify signal is pending */
    recv_id = read_reg(qts, 0, CLIENT_APPS, IPCC_REG_RECV_ID);
    g_assert_cmpuint(recv_id, ==, (CLIENT_CDSP << 16) | SIGNAL_TEST);
    verify_irq_register_logic(qts, CLIENT_APPS, true);

    /* Clear the signal explicitly */
    hwirq = (CLIENT_CDSP << IPCC_CLIENT_ID_SHIFT) | SIGNAL_TEST;
    write_reg(qts, 0, CLIENT_APPS, IPCC_REG_RECV_SIGNAL_CLEAR, hwirq);

    /* Verify signal is cleared */
    recv_id = read_reg(qts, 0, CLIENT_APPS, IPCC_REG_RECV_ID);
    g_assert_cmpuint(recv_id, ==, IPCC_NO_PENDING_IRQ);
    verify_irq_register_logic(qts, CLIENT_APPS, false);

    qtest_quit(qts);
}

/* Test clear-on-read functionality */
static void test_ipcc_clear_on_read(void)
{
    QTestState *qts = qtest_init("-M SA8775P_CDSP0");
    uint32_t hwirq, recv_id, config;

    /* Enable clear-on-read mode */
    config = IPCC_CONFIG_CLEAR_ON_RECV_RD;
    write_reg(qts, 0, CLIENT_APPS, IPCC_REG_CONFIG, config);

    /* Enable signal from CLIENT_ADSP to CLIENT_APPS */
    hwirq = (CLIENT_ADSP << IPCC_CLIENT_ID_SHIFT) | SIGNAL_WAKEUP;
    write_reg(qts, 0, CLIENT_APPS, IPCC_REG_RECV_SIGNAL_ENABLE, hwirq);

    /* Send signal from ADSP to APPS */
    hwirq = (CLIENT_APPS << IPCC_CLIENT_ID_SHIFT) | SIGNAL_WAKEUP;
    write_reg(qts, 0, CLIENT_ADSP, IPCC_REG_SEND_ID, hwirq);

    /* Verify signal is pending - first read should show pending */
    recv_id = read_reg(qts, 0, CLIENT_APPS, IPCC_REG_RECV_ID);
    g_assert_cmpuint(recv_id, ==, (CLIENT_ADSP << 16) | SIGNAL_WAKEUP);

    /* Read again - should be cleared due to clear-on-read */
    recv_id = read_reg(qts, 0, CLIENT_APPS, IPCC_REG_RECV_ID);
    g_assert_cmpuint(recv_id, ==, IPCC_NO_PENDING_IRQ);

    qtest_quit(qts);
}

/* Test broadcast functionality */
static void test_ipcc_broadcast(void)
{
    QTestState *qts = qtest_init("-M SA8775P_CDSP0");
    uint32_t hwirq, recv_id;

    /* Enable signal from CLIENT_APPS on multiple clients */
    hwirq = (CLIENT_APPS << IPCC_CLIENT_ID_SHIFT) | SIGNAL_SHUTDOWN;
    write_reg(qts, 0, CLIENT_MODEM, IPCC_REG_RECV_SIGNAL_ENABLE, hwirq);
    write_reg(qts, 0, CLIENT_ADSP, IPCC_REG_RECV_SIGNAL_ENABLE, hwirq);
    write_reg(qts, 0, CLIENT_CDSP, IPCC_REG_RECV_SIGNAL_ENABLE, hwirq);

    /* Send broadcast signal from APPS */
    hwirq = IPCC_SEND_BROADCAST_FLAG | SIGNAL_SHUTDOWN;
    write_reg(qts, 0, CLIENT_APPS, IPCC_REG_SEND_ID, hwirq);

    /*
     * Verify all enabled clients received the signal and IRQ logic indicates
     * pending
     */
    recv_id = read_reg(qts, 0, CLIENT_MODEM, IPCC_REG_RECV_ID);
    g_assert_cmpuint(recv_id, ==, (CLIENT_APPS << 16) | SIGNAL_SHUTDOWN);
    verify_irq_register_logic(qts, CLIENT_MODEM, true);

    recv_id = read_reg(qts, 0, CLIENT_ADSP, IPCC_REG_RECV_ID);
    g_assert_cmpuint(recv_id, ==, (CLIENT_APPS << 16) | SIGNAL_SHUTDOWN);
    verify_irq_register_logic(qts, CLIENT_ADSP, true);

    recv_id = read_reg(qts, 0, CLIENT_CDSP, IPCC_REG_RECV_ID);
    g_assert_cmpuint(recv_id, ==, (CLIENT_APPS << 16) | SIGNAL_SHUTDOWN);
    verify_irq_register_logic(qts, CLIENT_CDSP, true);

    /* Verify CLIENT_SLPI (not enabled) did not receive signal */
    recv_id = read_reg(qts, 0, CLIENT_SLPI, IPCC_REG_RECV_ID);
    g_assert_cmpuint(recv_id, ==, IPCC_NO_PENDING_IRQ);
    verify_irq_register_logic(qts, CLIENT_SLPI, false);

    qtest_quit(qts);
}

/* Test CDSP IRQ functionality - this client has IRQ connected to L2VIC */
static void test_ipcc_cdsp_irq(void)
{
    QTestState *qts = qtest_init("-M SA8775P_CDSP0");
    uint32_t hwirq, recv_id;

    /* Verify CDSP IRQ register logic shows no pending IRQ initially */
    verify_irq_register_logic(qts, CLIENT_CDSP, false);

    /* Enable signal from CLIENT_APPS to CLIENT_CDSP */
    hwirq = (CLIENT_APPS << IPCC_CLIENT_ID_SHIFT) | SIGNAL_TEST;
    write_reg(qts, 0, CLIENT_CDSP, IPCC_REG_RECV_SIGNAL_ENABLE, hwirq);

    /* Verify no pending IRQ initially */
    recv_id = read_reg(qts, 0, CLIENT_CDSP, IPCC_REG_RECV_ID);
    g_assert_cmpuint(recv_id, ==, IPCC_NO_PENDING_IRQ);
    verify_irq_register_logic(qts, CLIENT_CDSP, false);

    /* Send signal from APPS to CDSP */
    hwirq = (CLIENT_CDSP << IPCC_CLIENT_ID_SHIFT) | SIGNAL_TEST;
    write_reg(qts, 0, CLIENT_APPS, IPCC_REG_SEND_ID, hwirq);

    /* Check that signal is now pending and IRQ should be raised */
    recv_id = read_reg(qts, 0, CLIENT_CDSP, IPCC_REG_RECV_ID);
    g_assert_cmpuint(recv_id, ==, (CLIENT_APPS << 16) | SIGNAL_TEST);
    verify_irq_register_logic(qts, CLIENT_CDSP, true);

    /* Clear the signal */
    hwirq = (CLIENT_APPS << IPCC_CLIENT_ID_SHIFT) | SIGNAL_TEST;
    write_reg(qts, 0, CLIENT_CDSP, IPCC_REG_RECV_SIGNAL_CLEAR, hwirq);

    /* Verify signal is cleared and IRQ should be lowered */
    recv_id = read_reg(qts, 0, CLIENT_CDSP, IPCC_REG_RECV_ID);
    g_assert_cmpuint(recv_id, ==, IPCC_NO_PENDING_IRQ);
    verify_irq_register_logic(qts, CLIENT_CDSP, false);

    qtest_quit(qts);
}

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);

    qtest_add_func("/qcom-ipcc/reset-values", test_ipcc_reset_values);
    qtest_add_func("/qcom-ipcc/signal-enable-disable",
                   test_ipcc_signal_enable_disable);
    qtest_add_func("/qcom-ipcc/signal-clear", test_ipcc_signal_clear);
    qtest_add_func("/qcom-ipcc/clear-on-read", test_ipcc_clear_on_read);
    qtest_add_func("/qcom-ipcc/broadcast", test_ipcc_broadcast);
    qtest_add_func("/qcom-ipcc/cdsp-irq", test_ipcc_cdsp_irq);

    return g_test_run();
}
