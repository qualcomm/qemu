/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * QTest testcase for the Turing RSC device (qcom-turing-rsc)
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *
 * Tests register reset values, TCS trigger/completion, IRQ behavior,
 * command register access, and property-driven layout.
 */

#include "qemu/osdep.h"
#include "libqtest.h"
#include "qemu/bitops.h"

#define TURING_RSC_BASE 0x260A4000

/* Register offsets (from header) */
#define RSC_ID_DRV0                         0x0000
#define RSC_PARAM_SOLVER_CONFIG_DRV0        0x0004
#define RSC_PARAM_RSC_CONFIG_DRV0           0x0008
#define RSC_PARAM_RSC_PARENTCHILD_CONFIG_DRV0 0x000C

#define RSC_STATUS0_DRV0                    0x0010

#define HIDDEN_TCS_CTRL_DRV0                0x001C
#define PDC_SEQ_START_ADDR_REG_OFFSET_DRV0  0x0020
#define PDC_MATCH_VALUE_LO_REG_OFFSET_DRV0  0x0024
#define PDC_MATCH_VALUE_HI_REG_OFFSET_DRV0  0x0028
#define PDC_SLAVE_ID_DRV0                   0x002C
#define HIDDEN_TCS_STATUS_DRV0              0x0030
#define HIDDEN_TCS_CMD0_ADDR_DRV0           0x0034
#define HIDDEN_TCS_CMD0_DATA_DRV0           0x0038
#define HIDDEN_TCS_CMD1_ADDR_DRV0           0x003C
#define HIDDEN_TCS_CMD1_DATA_DRV0           0x0040
#define HIDDEN_TCS_CMD2_ADDR_DRV0           0x0044
#define HIDDEN_TCS_CMD2_DATA_DRV0           0x0048

#define RSC_ERROR_IRQ_STATUS_DRV0           0x00D0
#define RSC_ERROR_IRQ_CLEAR_DRV0            0x00D4
#define RSC_ERROR_IRQ_ENABLE_DRV0           0x00D8

#define RSC_SECURE_OVERRIDE_DRV0            0x0104

#define TCS_AMC_MODE_IRQ_ENABLE_DRV0        0x0D00
#define TCS_AMC_MODE_IRQ_STATUS_DRV0        0x0D04
#define TCS_AMC_MODE_IRQ_CLEAR_DRV0         0x0D08

/* Default property values */
#define DEFAULT_RSC_ID_RESET                0x00020400
#define DEFAULT_SOLVER_CONFIG_RESET         0x00010100
#define DEFAULT_RSC_CONFIG                  0x01300214
#define DEFAULT_PARENTCHILD_CONFIG_RESET    0x8000000A
#define DEFAULT_TCS_BASE_OFFSET             0x10
#define DEFAULT_CMD_SPACING                 0x14
#define DEFAULT_TCS_TIMEOUT_BASE            0x3D44

/* Fixed reset values */
#define RSC_RESET_PDC_SEQ_START_ADDR        0x00004520
#define RSC_RESET_PDC_MATCH_VALUE_LO        0x00004510
#define RSC_RESET_PDC_MATCH_VALUE_HI        0x00004514
#define RSC_RESET_PDC_SLAVE_ID              0x00000001
#define RSC_RESET_HIDDEN_TCS_CMD0_ADDR      0x82204514
#define RSC_RESET_HIDDEN_TCS_CMD1_ADDR      0x82204510
#define RSC_RESET_HIDDEN_TCS_CMD2_ADDR      0x82204520
#define RSC_RESET_SECURE_OVERRIDE           0x00000001
#define RSC_RESET_TCS_TIMEOUT_VAL           0x0000FFFF

/* TCS layout constants */
#define TCS_SPACING                         0x2A0
#define TCS_AMC_MODE_TRIGGER                BIT(24)
#define TCS_CONTROLLER_IDLE                 BIT(0)

/* TCS block base = TCS_AMC_MODE_IRQ_ENABLE_DRV0 + tcs_base_offset */
#define TCS_BLOCK_BASE  (TCS_AMC_MODE_IRQ_ENABLE_DRV0 + DEFAULT_TCS_BASE_OFFSET)

/* Per-TCS register offsets within a TCS slot */
#define TCS_CMD_WAIT_FOR_CMPL_OFF   0x00
#define TCS_CONTROL_OFF             0x04
#define TCS_STATUS_OFF              0x08
#define TCS_CMD_ENABLE_OFF          0x0C

/* Command area offset within each TCS (matches "cmd-base-in-tcs" property) */
#define CMD_BASE_IN_TCS 0x14

/* Compute the absolute register offset for a TCS register */
#define TCS_REG(tcs, off) (TCS_BLOCK_BASE + (tcs) * TCS_SPACING + (off))

/* Compute the absolute register offset for a command register */
#define CMD_REG(tcs, cmd, off) \
    (TCS_BLOCK_BASE + (tcs) * TCS_SPACING + CMD_BASE_IN_TCS + \
     (cmd) * DEFAULT_CMD_SPACING + (off))

/* Command register offsets within a command slot */
#define CMD_MSGID_OFF   0x0
#define CMD_ADDR_OFF    0x4
#define CMD_DATA_OFF    0x8
#define CMD_STATUS_OFF  0xC

/* Parentchild config field extraction */
#define PARENTCHILD_NUM_TCS_MASK    0x3F
#define PARENTCHILD_NUM_TCS_SHIFT   0
#define PARENTCHILD_CMDS_PER_TCS_MASK   0xF8000000
#define PARENTCHILD_CMDS_PER_TCS_SHIFT  27

static uint32_t rsc_readl(QTestState *qts, uint32_t offset)
{
    return qtest_readl(qts, TURING_RSC_BASE + offset);
}

static void rsc_writel(QTestState *qts, uint32_t offset, uint32_t value)
{
    qtest_writel(qts, TURING_RSC_BASE + offset, value);
}

/* Test: Verify ID and parameter registers match property defaults */
static void test_reset_id_registers(void)
{
    QTestState *qts = qtest_init("-machine SA8775P_CDSP0");

    g_assert_cmphex(rsc_readl(qts, RSC_ID_DRV0), ==, DEFAULT_RSC_ID_RESET);
    g_assert_cmphex(rsc_readl(qts, RSC_PARAM_SOLVER_CONFIG_DRV0), ==,
                    DEFAULT_SOLVER_CONFIG_RESET);
    g_assert_cmphex(rsc_readl(qts, RSC_PARAM_RSC_CONFIG_DRV0), ==,
                    DEFAULT_RSC_CONFIG);
    g_assert_cmphex(rsc_readl(qts, RSC_PARAM_RSC_PARENTCHILD_CONFIG_DRV0), ==,
                    DEFAULT_PARENTCHILD_CONFIG_RESET);

    /* Verify extracted TCS count and commands-per-TCS */
    uint32_t pc = DEFAULT_PARENTCHILD_CONFIG_RESET;
    uint32_t num_tcs = (pc >> PARENTCHILD_NUM_TCS_SHIFT) &
                       PARENTCHILD_NUM_TCS_MASK;
    uint32_t cmds_per_tcs = (pc >> PARENTCHILD_CMDS_PER_TCS_SHIFT) & 0x1F;
    g_assert_cmpuint(num_tcs, ==, 10);
    g_assert_cmpuint(cmds_per_tcs, ==, 16);

    qtest_quit(qts);
}

/* Test: Verify hidden TCS register reset values */
static void test_reset_hidden_tcs(void)
{
    QTestState *qts = qtest_init("-machine SA8775P_CDSP0");

    g_assert_cmphex(rsc_readl(qts, HIDDEN_TCS_CTRL_DRV0), ==, 0);
    g_assert_cmphex(rsc_readl(qts, PDC_SEQ_START_ADDR_REG_OFFSET_DRV0), ==,
                    RSC_RESET_PDC_SEQ_START_ADDR);
    g_assert_cmphex(rsc_readl(qts, PDC_MATCH_VALUE_LO_REG_OFFSET_DRV0), ==,
                    RSC_RESET_PDC_MATCH_VALUE_LO);
    g_assert_cmphex(rsc_readl(qts, PDC_MATCH_VALUE_HI_REG_OFFSET_DRV0), ==,
                    RSC_RESET_PDC_MATCH_VALUE_HI);
    g_assert_cmphex(rsc_readl(qts, PDC_SLAVE_ID_DRV0), ==,
                    RSC_RESET_PDC_SLAVE_ID);
    g_assert_cmphex(rsc_readl(qts, HIDDEN_TCS_STATUS_DRV0), ==, 0);

    g_assert_cmphex(rsc_readl(qts, HIDDEN_TCS_CMD0_ADDR_DRV0), ==,
                    RSC_RESET_HIDDEN_TCS_CMD0_ADDR);
    g_assert_cmphex(rsc_readl(qts, HIDDEN_TCS_CMD0_DATA_DRV0), ==, 0);
    g_assert_cmphex(rsc_readl(qts, HIDDEN_TCS_CMD1_ADDR_DRV0), ==,
                    RSC_RESET_HIDDEN_TCS_CMD1_ADDR);
    g_assert_cmphex(rsc_readl(qts, HIDDEN_TCS_CMD1_DATA_DRV0), ==, 0);
    g_assert_cmphex(rsc_readl(qts, HIDDEN_TCS_CMD2_ADDR_DRV0), ==,
                    RSC_RESET_HIDDEN_TCS_CMD2_ADDR);
    g_assert_cmphex(rsc_readl(qts, HIDDEN_TCS_CMD2_DATA_DRV0), ==, 0);

    qtest_quit(qts);
}

/* Test: Verify control register reset values */
static void test_reset_control_registers(void)
{
    QTestState *qts = qtest_init("-machine SA8775P_CDSP0");

    g_assert_cmphex(rsc_readl(qts, RSC_SECURE_OVERRIDE_DRV0), ==,
                    RSC_RESET_SECURE_OVERRIDE);
    g_assert_cmphex(rsc_readl(qts, TCS_AMC_MODE_IRQ_ENABLE_DRV0), ==, 0);
    g_assert_cmphex(rsc_readl(qts, TCS_AMC_MODE_IRQ_STATUS_DRV0), ==, 0);
    g_assert_cmphex(rsc_readl(qts, RSC_ERROR_IRQ_ENABLE_DRV0), ==, 0);

    qtest_quit(qts);
}

/* Test: Verify timeout register reset values */
static void test_reset_timeout_registers(void)
{
    QTestState *qts = qtest_init("-machine SA8775P_CDSP0");

    /* Timeout enable should be 0 */
    g_assert_cmphex(rsc_readl(qts, DEFAULT_TCS_TIMEOUT_BASE), ==, 0);
    /* Timeout val should be 0xFFFF */
    g_assert_cmphex(rsc_readl(qts, DEFAULT_TCS_TIMEOUT_BASE + 12), ==,
                    RSC_RESET_TCS_TIMEOUT_VAL);

    qtest_quit(qts);
}

/* Test: TCS status registers are idle at reset */
static void test_tcs_initial_status(void)
{
    QTestState *qts = qtest_init("-machine SA8775P_CDSP0");

    for (int i = 0; i < 10; i++) {
        uint32_t status = rsc_readl(qts, TCS_REG(i, TCS_STATUS_OFF));
        g_assert_cmphex(status, ==, TCS_CONTROLLER_IDLE);
    }

    qtest_quit(qts);
}

/* Test: TCS trigger sets IRQ status bit */
static void test_tcs_trigger(void)
{
    QTestState *qts = qtest_init("-machine SA8775P_CDSP0");

    /* IRQ status should start at 0 */
    g_assert_cmphex(rsc_readl(qts, TCS_AMC_MODE_IRQ_STATUS_DRV0), ==, 0);

    /* Trigger TCS5 (firmware typically picks highest available) */
    rsc_writel(qts, TCS_REG(5, TCS_CONTROL_OFF), TCS_AMC_MODE_TRIGGER);

    /* Verify IRQ status bit is set for TCS5 */
    g_assert_cmphex(rsc_readl(qts, TCS_AMC_MODE_IRQ_STATUS_DRV0), ==,
                    BIT(5));

    /* Trigger TCS9 */
    rsc_writel(qts, TCS_REG(9, TCS_CONTROL_OFF), TCS_AMC_MODE_TRIGGER);

    /* Both bits should be set */
    g_assert_cmphex(rsc_readl(qts, TCS_AMC_MODE_IRQ_STATUS_DRV0), ==,
                    BIT(5) | BIT(9));

    qtest_quit(qts);
}

/* Test: IRQ clear register clears status bits */
static void test_irq_clear(void)
{
    QTestState *qts = qtest_init("-machine SA8775P_CDSP0");

    /* Trigger TCS0 and TCS3 */
    rsc_writel(qts, TCS_REG(0, TCS_CONTROL_OFF), TCS_AMC_MODE_TRIGGER);
    rsc_writel(qts, TCS_REG(3, TCS_CONTROL_OFF), TCS_AMC_MODE_TRIGGER);
    g_assert_cmphex(rsc_readl(qts, TCS_AMC_MODE_IRQ_STATUS_DRV0), ==,
                    BIT(0) | BIT(3));

    /* Clear TCS0 bit only */
    rsc_writel(qts, TCS_AMC_MODE_IRQ_CLEAR_DRV0, BIT(0));
    g_assert_cmphex(rsc_readl(qts, TCS_AMC_MODE_IRQ_STATUS_DRV0), ==,
                    BIT(3));

    /* Clear TCS3 bit */
    rsc_writel(qts, TCS_AMC_MODE_IRQ_CLEAR_DRV0, BIT(3));
    g_assert_cmphex(rsc_readl(qts, TCS_AMC_MODE_IRQ_STATUS_DRV0), ==, 0);

    qtest_quit(qts);
}

/* Test: IRQ enable register gating */
static void test_irq_enable(void)
{
    QTestState *qts = qtest_init("-machine SA8775P_CDSP0");

    /* Enable IRQ for TCS2 */
    rsc_writel(qts, TCS_AMC_MODE_IRQ_ENABLE_DRV0, BIT(2));
    g_assert_cmphex(rsc_readl(qts, TCS_AMC_MODE_IRQ_ENABLE_DRV0), ==,
                    BIT(2));

    /* Trigger TCS2 - should set status */
    rsc_writel(qts, TCS_REG(2, TCS_CONTROL_OFF), TCS_AMC_MODE_TRIGGER);
    g_assert_cmphex(rsc_readl(qts, TCS_AMC_MODE_IRQ_STATUS_DRV0), ==,
                    BIT(2));

    /* Clear and verify */
    rsc_writel(qts, TCS_AMC_MODE_IRQ_CLEAR_DRV0, BIT(2));
    g_assert_cmphex(rsc_readl(qts, TCS_AMC_MODE_IRQ_STATUS_DRV0), ==, 0);

    qtest_quit(qts);
}

/* Test: Error IRQ enable and clear */
static void test_error_irq(void)
{
    QTestState *qts = qtest_init("-machine SA8775P_CDSP0");

    /* Error IRQ enable starts at 0 */
    g_assert_cmphex(rsc_readl(qts, RSC_ERROR_IRQ_ENABLE_DRV0), ==, 0);

    /* Write enable value */
    rsc_writel(qts, RSC_ERROR_IRQ_ENABLE_DRV0, 0x1);
    g_assert_cmphex(rsc_readl(qts, RSC_ERROR_IRQ_ENABLE_DRV0), ==, 0x1);

    /* Clear enable */
    rsc_writel(qts, RSC_ERROR_IRQ_ENABLE_DRV0, 0x0);
    g_assert_cmphex(rsc_readl(qts, RSC_ERROR_IRQ_ENABLE_DRV0), ==, 0x0);

    qtest_quit(qts);
}

/* Test: TCS command register read/write */
static void test_tcs_command_registers(void)
{
    QTestState *qts = qtest_init("-machine SA8775P_CDSP0");

    /* Write MSGID, ADDR, DATA to TCS0 CMD0 */
    rsc_writel(qts, CMD_REG(0, 0, CMD_MSGID_OFF), 0x00010101);
    rsc_writel(qts, CMD_REG(0, 0, CMD_ADDR_OFF), 0x00030004);
    rsc_writel(qts, CMD_REG(0, 0, CMD_DATA_OFF), 0xDEADBEEF);

    g_assert_cmphex(rsc_readl(qts, CMD_REG(0, 0, CMD_MSGID_OFF)), ==,
                    0x00010101);
    g_assert_cmphex(rsc_readl(qts, CMD_REG(0, 0, CMD_ADDR_OFF)), ==,
                    0x00030004);
    g_assert_cmphex(rsc_readl(qts, CMD_REG(0, 0, CMD_DATA_OFF)), ==,
                    0xDEADBEEF);

    /* Write to TCS5 CMD15 (last command in a high TCS) */
    rsc_writel(qts, CMD_REG(5, 15, CMD_MSGID_OFF), 0xAAAA);
    rsc_writel(qts, CMD_REG(5, 15, CMD_ADDR_OFF), 0xBBBB);
    rsc_writel(qts, CMD_REG(5, 15, CMD_DATA_OFF), 0xCCCC);

    g_assert_cmphex(rsc_readl(qts, CMD_REG(5, 15, CMD_MSGID_OFF)), ==,
                    0xAAAA);
    g_assert_cmphex(rsc_readl(qts, CMD_REG(5, 15, CMD_ADDR_OFF)), ==,
                    0xBBBB);
    g_assert_cmphex(rsc_readl(qts, CMD_REG(5, 15, CMD_DATA_OFF)), ==,
                    0xCCCC);

    /* CMD STATUS is read-only and returns a fixed value */
    g_assert_cmphex(rsc_readl(qts, CMD_REG(0, 0, CMD_STATUS_OFF)), ==,
                    0x10101);

    qtest_quit(qts);
}

/* Test: TCS CMD_WAIT_FOR_CMPL and CMD_ENABLE registers */
static void test_tcs_control_registers(void)
{
    QTestState *qts = qtest_init("-machine SA8775P_CDSP0");

    /* CMD_WAIT_FOR_CMPL should be 0 at reset */
    g_assert_cmphex(rsc_readl(qts, TCS_REG(0, TCS_CMD_WAIT_FOR_CMPL_OFF)),
                    ==, 0);

    /* Write and read back */
    rsc_writel(qts, TCS_REG(0, TCS_CMD_WAIT_FOR_CMPL_OFF), 0xFFFF);
    g_assert_cmphex(rsc_readl(qts, TCS_REG(0, TCS_CMD_WAIT_FOR_CMPL_OFF)),
                    ==, 0xFFFF);

    /* CMD_ENABLE should be 0 at reset */
    g_assert_cmphex(rsc_readl(qts, TCS_REG(0, TCS_CMD_ENABLE_OFF)), ==, 0);

    rsc_writel(qts, TCS_REG(0, TCS_CMD_ENABLE_OFF), 0xABCD);
    g_assert_cmphex(rsc_readl(qts, TCS_REG(0, TCS_CMD_ENABLE_OFF)), ==,
                    0xABCD);

    qtest_quit(qts);
}

/* Test: Hidden TCS data registers are writable */
static void test_hidden_tcs_write(void)
{
    QTestState *qts = qtest_init("-machine SA8775P_CDSP0");

    /* Write hidden TCS command data registers */
    rsc_writel(qts, HIDDEN_TCS_CMD0_DATA_DRV0, 0x11111111);
    rsc_writel(qts, HIDDEN_TCS_CMD1_DATA_DRV0, 0x22222222);
    rsc_writel(qts, HIDDEN_TCS_CMD2_DATA_DRV0, 0x33333333);

    g_assert_cmphex(rsc_readl(qts, HIDDEN_TCS_CMD0_DATA_DRV0), ==,
                    0x11111111);
    g_assert_cmphex(rsc_readl(qts, HIDDEN_TCS_CMD1_DATA_DRV0), ==,
                    0x22222222);
    g_assert_cmphex(rsc_readl(qts, HIDDEN_TCS_CMD2_DATA_DRV0), ==,
                    0x33333333);

    /* PDC registers are writable */
    rsc_writel(qts, PDC_SEQ_START_ADDR_REG_OFFSET_DRV0, 0x5000);
    g_assert_cmphex(rsc_readl(qts, PDC_SEQ_START_ADDR_REG_OFFSET_DRV0), ==,
                    0x5000);

    rsc_writel(qts, PDC_SLAVE_ID_DRV0, 0x3);
    g_assert_cmphex(rsc_readl(qts, PDC_SLAVE_ID_DRV0), ==, 0x3);

    qtest_quit(qts);
}

/* Test: Timeout register read/write */
static void test_timeout_registers(void)
{
    QTestState *qts = qtest_init("-machine SA8775P_CDSP0");

    /* Write timeout enable */
    rsc_writel(qts, DEFAULT_TCS_TIMEOUT_BASE, 0x1);
    g_assert_cmphex(rsc_readl(qts, DEFAULT_TCS_TIMEOUT_BASE), ==, 0x1);

    /* Write timeout value */
    rsc_writel(qts, DEFAULT_TCS_TIMEOUT_BASE + 12, 0x1234);
    g_assert_cmphex(rsc_readl(qts, DEFAULT_TCS_TIMEOUT_BASE + 12), ==,
                    0x1234);

    qtest_quit(qts);
}

/* Test: Secure override register */
static void test_secure_override(void)
{
    QTestState *qts = qtest_init("-machine SA8775P_CDSP0");

    /* Should have reset value */
    g_assert_cmphex(rsc_readl(qts, RSC_SECURE_OVERRIDE_DRV0), ==,
                    RSC_RESET_SECURE_OVERRIDE);

    /* Write new value */
    rsc_writel(qts, RSC_SECURE_OVERRIDE_DRV0, 0x0);
    g_assert_cmphex(rsc_readl(qts, RSC_SECURE_OVERRIDE_DRV0), ==, 0x0);

    rsc_writel(qts, RSC_SECURE_OVERRIDE_DRV0, 0x1);
    g_assert_cmphex(rsc_readl(qts, RSC_SECURE_OVERRIDE_DRV0), ==, 0x1);

    qtest_quit(qts);
}

/* Test: Full TCS trigger/clear sequence mimicking firmware flow */
static void test_tcs_firmware_sequence(void)
{
    QTestState *qts = qtest_init("-machine SA8775P_CDSP0");

    /* Enable IRQ for all 10 TCS */
    rsc_writel(qts, TCS_AMC_MODE_IRQ_ENABLE_DRV0, 0x3FF);

    /* Program TCS9 commands (firmware picks highest idle TCS) */
    rsc_writel(qts, TCS_REG(9, TCS_CMD_ENABLE_OFF), 0x1);
    rsc_writel(qts, CMD_REG(9, 0, CMD_MSGID_OFF), 0x00010001);
    rsc_writel(qts, CMD_REG(9, 0, CMD_ADDR_OFF), 0x00040000);
    rsc_writel(qts, CMD_REG(9, 0, CMD_DATA_OFF), 0x00000001);

    /* Trigger: write CONTROL without trigger first, then with trigger */
    rsc_writel(qts, TCS_REG(9, TCS_CONTROL_OFF), 0);
    g_assert_cmphex(rsc_readl(qts, TCS_AMC_MODE_IRQ_STATUS_DRV0), ==, 0);

    rsc_writel(qts, TCS_REG(9, TCS_CONTROL_OFF), TCS_AMC_MODE_TRIGGER);
    g_assert_cmphex(rsc_readl(qts, TCS_AMC_MODE_IRQ_STATUS_DRV0), ==,
                    BIT(9));

    /* ISR clears the IRQ */
    rsc_writel(qts, TCS_AMC_MODE_IRQ_CLEAR_DRV0, BIT(9));
    g_assert_cmphex(rsc_readl(qts, TCS_AMC_MODE_IRQ_STATUS_DRV0), ==, 0);

    qtest_quit(qts);
}

/* Test: Verify property-driven parentchild config extraction */
static void test_properties_parentchild(void)
{
    QTestState *qts = qtest_init("-machine SA8775P_CDSP0");

    uint32_t pc = rsc_readl(qts, RSC_PARAM_RSC_PARENTCHILD_CONFIG_DRV0);
    uint32_t num_tcs = (pc >> PARENTCHILD_NUM_TCS_SHIFT) &
                       PARENTCHILD_NUM_TCS_MASK;
    uint32_t cmds_per_tcs = (pc >> PARENTCHILD_CMDS_PER_TCS_SHIFT) & 0x1F;

    /* Default: 10 TCS, 16 commands per TCS */
    g_assert_cmpuint(num_tcs, ==, 10);
    g_assert_cmpuint(cmds_per_tcs, ==, 16);

    /* Verify all 10 TCS are accessible (status reads as idle) */
    for (uint32_t i = 0; i < num_tcs; i++) {
        g_assert_cmphex(rsc_readl(qts, TCS_REG(i, TCS_STATUS_OFF)), ==,
                        TCS_CONTROLLER_IDLE);
    }

    qtest_quit(qts);
}

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);

    qtest_add_func("/turing-rsc/reset/id-registers",
                   test_reset_id_registers);
    qtest_add_func("/turing-rsc/reset/hidden-tcs",
                   test_reset_hidden_tcs);
    qtest_add_func("/turing-rsc/reset/control-registers",
                   test_reset_control_registers);
    qtest_add_func("/turing-rsc/reset/timeout-registers",
                   test_reset_timeout_registers);
    qtest_add_func("/turing-rsc/tcs/initial-status",
                   test_tcs_initial_status);
    qtest_add_func("/turing-rsc/tcs/trigger",
                   test_tcs_trigger);
    qtest_add_func("/turing-rsc/tcs/control-registers",
                   test_tcs_control_registers);
    qtest_add_func("/turing-rsc/tcs/command-registers",
                   test_tcs_command_registers);
    qtest_add_func("/turing-rsc/tcs/firmware-sequence",
                   test_tcs_firmware_sequence);
    qtest_add_func("/turing-rsc/irq/clear",
                   test_irq_clear);
    qtest_add_func("/turing-rsc/irq/enable",
                   test_irq_enable);
    qtest_add_func("/turing-rsc/irq/error",
                   test_error_irq);
    qtest_add_func("/turing-rsc/hidden-tcs/write",
                   test_hidden_tcs_write);
    qtest_add_func("/turing-rsc/timeout/registers",
                   test_timeout_registers);
    qtest_add_func("/turing-rsc/control/secure-override",
                   test_secure_override);
    qtest_add_func("/turing-rsc/properties/parentchild",
                   test_properties_parentchild);

    return g_test_run();
}
