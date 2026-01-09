/*
 * QTest testcase for RPMH-RSC on SA8775P
 *
 * Copyright (c) 2024 Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Tests the synthesized RPMH-RSC implementation on SA8775P-CDSP0 machine
 */

#include "qemu/osdep.h"
#include "libqtest.h"
#include "qemu/bitops.h"

#define RPMH_RSC_BASE       0x260A4000 /* SA8775P TURING0 RSC base address */

/* TCS Types */
#define SLEEP_TCS           0
#define WAKE_TCS            1
#define ACTIVE_TCS          2
#define CONTROL_TCS         3

/* Register offsets (v2.7) */
#define DRV_ID                     0x00
#define DRV_SOLVER_CONFIG          0x04
#define DRV_PRNT_CHLD_CONFIG       0x0C
#define RSC_DRV_IRQ_ENABLE         0x20
#define RSC_DRV_IRQ_STATUS         0x04
#define RSC_DRV_IRQ_CLEAR          0x24
#define RSC_DRV_CMD_WAIT_FOR_CMPL  0x10
#define RSC_DRV_CONTROL            0x14
#define RSC_DRV_STATUS             0x18
#define RSC_DRV_CMD_ENABLE         0x1C
#define RSC_DRV_CMD_MSGID          0x30
#define RSC_DRV_CMD_ADDR           0x34
#define RSC_DRV_CMD_DATA           0x38
#define RSC_DRV_CMD_STATUS         0x3C
#define RSC_DRV_CMD_RESP_DATA      0x40

#define RSC_DRV_TCS_OFFSET         672
#define RSC_DRV_CMD_OFFSET         20
#define TCS_BASE_OFFSET            0x00000D00

/* Configuration bits */
#define DRV_NUM_TCS_MASK           0x3F
#define DRV_NUM_TCS_SHIFT          6
#define DRV_NCPT_MASK              0x1F
#define DRV_NCPT_SHIFT             27

/* TCS control bits */
#define TCS_AMC_MODE_ENABLE        BIT(16)
#define TCS_AMC_MODE_TRIGGER       BIT(24)

/* Command bits */
#define CMD_MSGID_RESP_REQ         BIT(8)
#define CMD_MSGID_WRITE            BIT(16)
#define CMD_STATUS_ISSUED          BIT(8)
#define CMD_STATUS_COMPL           BIT(16)

/* Test fixture */
typedef struct {
    QTestState *qts;
    uint64_t base;
    uint32_t tcs_offset;
    uint32_t num_tcs;
    uint32_t ncpt;
} RpmhRscTestFixture;

static uint32_t rpmh_rsc_readl(RpmhRscTestFixture *f, uint32_t offset)
{
    return qtest_readl(f->qts, f->base + offset);
}

static void rpmh_rsc_writel(RpmhRscTestFixture *f, uint32_t offset,
                            uint32_t value)
{
    qtest_writel(f->qts, f->base + offset, value);
}

static uint32_t tcs_reg_addr(RpmhRscTestFixture *f, int tcs_id, uint32_t reg)
{
    return f->tcs_offset + RSC_DRV_TCS_OFFSET * tcs_id + reg;
}

static uint32_t tcs_cmd_addr(RpmhRscTestFixture *f, int tcs_id, int cmd_id,
                             uint32_t reg)
{
    return tcs_reg_addr(f, tcs_id, reg) + RSC_DRV_CMD_OFFSET * cmd_id;
}

static void rpmh_rsc_setup(RpmhRscTestFixture *f)
{
    f->qts = qtest_init("-machine SA8775P_CDSP0");
    f->base = RPMH_RSC_BASE;
    f->tcs_offset = TCS_BASE_OFFSET;

    /* Read hardware configuration from synthesized device */
    uint32_t config = rpmh_rsc_readl(f, DRV_PRNT_CHLD_CONFIG);

    /* Extract number of TCS and commands per TCS */
    f->num_tcs = (config >> DRV_NUM_TCS_SHIFT) & DRV_NUM_TCS_MASK;
    f->ncpt = (config >> DRV_NCPT_SHIFT) & DRV_NCPT_MASK;

    g_assert_cmpuint(f->num_tcs, >, 0);
    /* Max supported by our implementation */
    g_assert_cmpuint(f->num_tcs, <=, 32);
    g_assert_cmpuint(f->ncpt, >, 0);
    g_assert_cmpuint(f->ncpt, <=, 16);
}

static void rpmh_rsc_teardown(RpmhRscTestFixture *f)
{
    qtest_quit(f->qts);
}

/* Test basic register access and device identification */
static void test_rpmh_rsc_basic_registers(void)
{
    RpmhRscTestFixture f = {0};
    rpmh_rsc_setup(&f);

    /* Test driver ID register (read-only) */
    uint32_t drv_id = rpmh_rsc_readl(&f, DRV_ID);
    g_assert_cmphex(drv_id, ==, 0x00040300); /* Expected APPS RSC ID */

    /* Test solver config (read-only) */
    uint32_t solver_cfg = rpmh_rsc_readl(&f, DRV_SOLVER_CONFIG);
    g_assert_cmphex(solver_cfg, ==, 0x04010100);

    /* Test IRQ enable/clear registers */
    rpmh_rsc_writel(&f, RSC_DRV_IRQ_ENABLE, 0xFF);
    g_assert_cmphex(rpmh_rsc_readl(&f, RSC_DRV_IRQ_ENABLE), ==, 0xFF);

    /* IRQ clear is write-only, status should be 0 initially */
    g_assert_cmphex(rpmh_rsc_readl(&f, TCS_BASE_OFFSET + RSC_DRV_IRQ_STATUS),
                    ==, 0);

    /* Test configuration registers are read-only */
    uint32_t orig_prnt_chld_cfg = rpmh_rsc_readl(&f, DRV_PRNT_CHLD_CONFIG);

    /* Try to write - should not change */
    rpmh_rsc_writel(&f, DRV_SOLVER_CONFIG, 0xDEADBEEF);
    rpmh_rsc_writel(&f, DRV_PRNT_CHLD_CONFIG, 0xDEADBEEF);

    g_assert_cmphex(rpmh_rsc_readl(&f, DRV_SOLVER_CONFIG), ==, solver_cfg);
    g_assert_cmphex(rpmh_rsc_readl(&f, DRV_PRNT_CHLD_CONFIG),
                    ==, orig_prnt_chld_cfg);

    rpmh_rsc_teardown(&f);
}

/* Test TCS command programming */
static void test_rpmh_rsc_tcs_commands(void)
{
    RpmhRscTestFixture f = {0};
    rpmh_rsc_setup(&f);

    int tcs_id = 0; /* Use first TCS */
    int cmd_id = 0; /* First command */

    /* Program a command to test register access */
    uint32_t test_addr = 0x12345678;
    uint32_t test_data = 0xABCDEF01;
    uint32_t msgid = CMD_MSGID_WRITE | CMD_MSGID_RESP_REQ;

    /* Write command registers */
    rpmh_rsc_writel(&f, tcs_cmd_addr(&f, tcs_id, cmd_id, RSC_DRV_CMD_MSGID),
                    msgid);
    rpmh_rsc_writel(&f, tcs_cmd_addr(&f, tcs_id, cmd_id, RSC_DRV_CMD_ADDR),
                    test_addr);
    rpmh_rsc_writel(&f, tcs_cmd_addr(&f, tcs_id, cmd_id, RSC_DRV_CMD_DATA),
                    test_data);

    /* Verify writes */
    g_assert_cmphex(rpmh_rsc_readl(&f, tcs_cmd_addr(&f, tcs_id, cmd_id,
                                                    RSC_DRV_CMD_MSGID)),
                    ==, msgid);
    g_assert_cmphex(rpmh_rsc_readl(&f, tcs_cmd_addr(&f, tcs_id, cmd_id,
                                                    RSC_DRV_CMD_ADDR)),
                    ==, test_addr);
    g_assert_cmphex(rpmh_rsc_readl(&f, tcs_cmd_addr(&f, tcs_id, cmd_id,
                                                    RSC_DRV_CMD_DATA)),
                    ==, test_data);

    /* Command status should be 0 before trigger */
    g_assert_cmphex(rpmh_rsc_readl(&f, tcs_cmd_addr(&f, tcs_id, cmd_id,
                                                    RSC_DRV_CMD_STATUS)),
                    ==, 0);

    rpmh_rsc_teardown(&f);
}

/* Test TCS trigger and completion with IRQ handling */
static void test_rpmh_rsc_tcs_trigger_and_irq(void)
{
    RpmhRscTestFixture f = {0};
    rpmh_rsc_setup(&f);

    int tcs_id = 0;
    int cmd_id = 0;

    /* Enable IRQ for TCS 0 */
    rpmh_rsc_writel(&f, RSC_DRV_IRQ_ENABLE, BIT(tcs_id));

    /* Setup a command */
    uint32_t msgid = CMD_MSGID_WRITE;
    rpmh_rsc_writel(&f, tcs_cmd_addr(&f, tcs_id, cmd_id, RSC_DRV_CMD_MSGID),
                    msgid);
    rpmh_rsc_writel(&f, tcs_cmd_addr(&f, tcs_id, cmd_id, RSC_DRV_CMD_ADDR),
                    0x11111111);
    rpmh_rsc_writel(&f, tcs_cmd_addr(&f, tcs_id, cmd_id, RSC_DRV_CMD_DATA),
                    0x22222222);

    /* Enable command 0 */
    rpmh_rsc_writel(&f, tcs_reg_addr(&f, tcs_id, RSC_DRV_CMD_ENABLE),
                    BIT(cmd_id));

    /* Verify command enable was set */
    g_assert_cmphex(rpmh_rsc_readl(&f, tcs_reg_addr(&f, tcs_id,
                                                    RSC_DRV_CMD_ENABLE)),
                    ==, BIT(cmd_id));

    /* Enable TCS AMC mode */
    rpmh_rsc_writel(&f, tcs_reg_addr(&f, tcs_id, RSC_DRV_CONTROL),
                    TCS_AMC_MODE_ENABLE);

    /* Trigger the TCS */
    rpmh_rsc_writel(&f, tcs_reg_addr(&f, tcs_id, RSC_DRV_CONTROL),
                    TCS_AMC_MODE_ENABLE | TCS_AMC_MODE_TRIGGER);

    /* Check command status - should show issued and completed */
    uint32_t status = rpmh_rsc_readl(&f, tcs_cmd_addr(&f, tcs_id, cmd_id,
                                                      RSC_DRV_CMD_STATUS));
    g_assert_cmphex(status & (CMD_STATUS_ISSUED | CMD_STATUS_COMPL), ==,
                    CMD_STATUS_ISSUED | CMD_STATUS_COMPL);

    /* Check IRQ status - should have TCS0 bit set */
    uint32_t irq_status = rpmh_rsc_readl(&f, f.tcs_offset + RSC_DRV_IRQ_STATUS);
    g_assert_cmphex(irq_status & BIT(tcs_id), ==, BIT(tcs_id));

    /* Clear IRQ */
    rpmh_rsc_writel(&f, RSC_DRV_IRQ_CLEAR, BIT(tcs_id));
    g_assert_cmphex(rpmh_rsc_readl(&f, f.tcs_offset + RSC_DRV_IRQ_STATUS) &
                    BIT(tcs_id), ==, 0);

    rpmh_rsc_teardown(&f);
}

/* Test multiple commands in a TCS (typical RPMH usage) */
static void test_rpmh_rsc_multi_command_tcs(void)
{
    RpmhRscTestFixture f = {0};
    rpmh_rsc_setup(&f);

    int tcs_id = 1; /* Use second TCS */
    int num_cmds = 3;
    uint32_t cmd_enable_mask = 0;

    /* Program multiple commands (typical RPMH power sequence) */
    for (int i = 0; i < num_cmds; i++) {
        uint32_t msgid = CMD_MSGID_WRITE | (i == 0 ? CMD_MSGID_RESP_REQ : 0);
        /* Simulated power rail addresses */
        uint32_t addr = 0x10000000 + (i * 0x1000);
        uint32_t data = 0x100 + i;  /* Simulated voltage levels */

        rpmh_rsc_writel(&f, tcs_cmd_addr(&f, tcs_id, i, RSC_DRV_CMD_MSGID),
                        msgid);
        rpmh_rsc_writel(&f, tcs_cmd_addr(&f, tcs_id, i, RSC_DRV_CMD_ADDR),
                        addr);
        rpmh_rsc_writel(&f, tcs_cmd_addr(&f, tcs_id, i, RSC_DRV_CMD_DATA),
                        data);

        cmd_enable_mask |= BIT(i);
    }

    /* Enable all commands */
    rpmh_rsc_writel(&f, tcs_reg_addr(&f, tcs_id, RSC_DRV_CMD_ENABLE),
                    cmd_enable_mask);

    /* Trigger TCS */
    rpmh_rsc_writel(&f, tcs_reg_addr(&f, tcs_id, RSC_DRV_CONTROL),
                    TCS_AMC_MODE_ENABLE | TCS_AMC_MODE_TRIGGER);

    /* Verify all commands completed */
    for (int i = 0; i < num_cmds; i++) {
        uint32_t status = rpmh_rsc_readl(&f, tcs_cmd_addr(&f, tcs_id, i,
                                                          RSC_DRV_CMD_STATUS));
        g_assert_cmphex(status & CMD_STATUS_COMPL, ==, CMD_STATUS_COMPL);
    }

    rpmh_rsc_teardown(&f);
}

/* Test TCS invalidation pattern used by Linux */
static void test_rpmh_rsc_tcs_invalidate(void)
{
    RpmhRscTestFixture f = {0};
    rpmh_rsc_setup(&f);

    int tcs_id = 2;

    /* Setup and enable a command */
    rpmh_rsc_writel(&f, tcs_cmd_addr(&f, tcs_id, 0, RSC_DRV_CMD_MSGID),
                    CMD_MSGID_WRITE);
    rpmh_rsc_writel(&f, tcs_cmd_addr(&f, tcs_id, 0, RSC_DRV_CMD_ADDR),
                    0x33333333);
    rpmh_rsc_writel(&f, tcs_cmd_addr(&f, tcs_id, 0, RSC_DRV_CMD_DATA),
                    0x44444444);
    rpmh_rsc_writel(&f, tcs_reg_addr(&f, tcs_id, RSC_DRV_CMD_ENABLE), BIT(0));

    /* Verify command is enabled */
    g_assert_cmphex(rpmh_rsc_readl(&f, tcs_reg_addr(&f, tcs_id,
                                                    RSC_DRV_CMD_ENABLE)),
                    ==, BIT(0));

    /* Invalidate TCS by writing 0 to CMD_ENABLE (Linux invalidation pattern) */
    rpmh_rsc_writel(&f, tcs_reg_addr(&f, tcs_id, RSC_DRV_CMD_ENABLE), 0);

    /* Verify invalidation */
    g_assert_cmphex(rpmh_rsc_readl(&f, tcs_reg_addr(&f, tcs_id,
                                                    RSC_DRV_CMD_ENABLE)),
                    ==, 0);

    rpmh_rsc_teardown(&f);
}

/* Test device configuration and limits */
static void test_rpmh_rsc_configuration(void)
{
    RpmhRscTestFixture f = {0};
    rpmh_rsc_setup(&f);

    /* Verify expected configuration for SA8775P */
    uint32_t config = rpmh_rsc_readl(&f, DRV_PRNT_CHLD_CONFIG);

    /*
     * Check that we have reasonable TCS count for SA8775P
     * (should be at least 4)
     */
    g_assert_cmpuint(f.num_tcs, >=, 4);
    g_assert_cmpuint(f.num_tcs, <=, 32);

    /* Check commands per TCS (should be at least 8 for SA8775P) */
    g_assert_cmpuint(f.ncpt, >=, 8);
    g_assert_cmpuint(f.ncpt, <=, 16);

    /* Verify the configuration matches what we read */
    uint32_t extracted_tcs = (config >> DRV_NUM_TCS_SHIFT) & DRV_NUM_TCS_MASK;
    uint32_t extracted_ncpt = (config >> DRV_NCPT_SHIFT) & DRV_NCPT_MASK;

    g_assert_cmpuint(extracted_tcs, ==, f.num_tcs);
    g_assert_cmpuint(extracted_ncpt, ==, f.ncpt);

    rpmh_rsc_teardown(&f);
}

/* Test kernel-like initialization sequence (probe simulation) */
static void test_rpmh_rsc_kernel_init_sequence(void)
{
    RpmhRscTestFixture f = {0};
    rpmh_rsc_setup(&f);

    /* Simulate kernel probe sequence */

    /* 1. Read hardware version (like kernel does during probe) */
    uint32_t version = rpmh_rsc_readl(&f, DRV_ID);
    g_assert_cmphex(version, ==, 0x00040300); /* Version 2.7 */

    /* 2. Read hardware configuration */
    uint32_t config = rpmh_rsc_readl(&f, DRV_PRNT_CHLD_CONFIG);
    g_assert_cmpuint(config, !=, 0);

    /* 3. Read solver config (autonomous mode detection) */
    uint32_t solver_cfg = rpmh_rsc_readl(&f, DRV_SOLVER_CONFIG);
    g_assert_cmphex(solver_cfg, ==, 0x04010100);

    /* 4. Enable interrupts for active TCSes (like kernel probe does) */
    uint32_t active_tcs_mask = 0x0F; /* Assume first 4 TCSes are ACTIVE */
    rpmh_rsc_writel(&f, RSC_DRV_IRQ_ENABLE, active_tcs_mask);
    g_assert_cmphex(rpmh_rsc_readl(&f, RSC_DRV_IRQ_ENABLE), ==,
                    active_tcs_mask);

    /* 5. Verify IRQ status is initially clear */
    g_assert_cmphex(rpmh_rsc_readl(&f, f.tcs_offset + RSC_DRV_IRQ_STATUS),
                    ==, 0);

    rpmh_rsc_teardown(&f);
}

/* Test active request flow (immediate command execution) */
static void test_rpmh_rsc_active_request_flow(void)
{
    RpmhRscTestFixture f = {0};
    rpmh_rsc_setup(&f);

    int tcs_id = 0; /* Use ACTIVE TCS */
    int cmd_id = 0;

    /* Enable IRQ for this TCS */
    rpmh_rsc_writel(&f, RSC_DRV_IRQ_ENABLE, BIT(tcs_id));

    /* Simulate active power request to PMIC */
    uint32_t pmic_addr = 0x40000324; /* Typical PMIC register */
    uint32_t voltage_level = 0x006F0000; /* 1.11V encoded */
    uint32_t msgid = CMD_MSGID_WRITE | CMD_MSGID_RESP_REQ;

    /* Program command (like __tcs_buffer_write in kernel) */
    rpmh_rsc_writel(&f, tcs_cmd_addr(&f, tcs_id, cmd_id, RSC_DRV_CMD_MSGID),
                    msgid);
    rpmh_rsc_writel(&f, tcs_cmd_addr(&f, tcs_id, cmd_id, RSC_DRV_CMD_ADDR),
                    pmic_addr);
    rpmh_rsc_writel(&f, tcs_cmd_addr(&f, tcs_id, cmd_id, RSC_DRV_CMD_DATA),
                    voltage_level);

    /* Enable command (like kernel does) */
    rpmh_rsc_writel(&f, tcs_reg_addr(&f, tcs_id, RSC_DRV_CMD_ENABLE),
                    BIT(cmd_id));

    /* Trigger TCS (like __tcs_set_trigger in kernel) */
    rpmh_rsc_writel(&f, tcs_reg_addr(&f, tcs_id, RSC_DRV_CONTROL),
                    TCS_AMC_MODE_ENABLE | TCS_AMC_MODE_TRIGGER);

    /* Verify command completion */
    uint32_t cmd_status = rpmh_rsc_readl(&f, tcs_cmd_addr(&f, tcs_id, cmd_id,
                                                          RSC_DRV_CMD_STATUS));
    g_assert_cmphex(cmd_status & CMD_STATUS_COMPL, ==, CMD_STATUS_COMPL);

    /* Check IRQ was triggered */
    uint32_t irq_status = rpmh_rsc_readl(&f, f.tcs_offset + RSC_DRV_IRQ_STATUS);
    g_assert_cmphex(irq_status & BIT(tcs_id), ==, BIT(tcs_id));

    /* Simulate TX done handler cleanup */
    /* Clear TCS trigger (like tcs_tx_done) */
    rpmh_rsc_writel(&f, tcs_reg_addr(&f, tcs_id, RSC_DRV_CONTROL),
                    0); /* Disable AMC mode */

    /* Clear command enable (like tcs_tx_done) */
    rpmh_rsc_writel(&f, tcs_reg_addr(&f, tcs_id, RSC_DRV_CMD_ENABLE), 0);

    /* Clear IRQ (like tcs_tx_done) */
    rpmh_rsc_writel(&f, RSC_DRV_IRQ_CLEAR, BIT(tcs_id));
    g_assert_cmphex(rpmh_rsc_readl(&f, f.tcs_offset + RSC_DRV_IRQ_STATUS) &
                    BIT(tcs_id), ==, 0);

    rpmh_rsc_teardown(&f);
}

/* Test sleep/wake request programming (cached requests) */
static void test_rpmh_rsc_sleep_wake_programming(void)
{
    RpmhRscTestFixture f = {0};
    rpmh_rsc_setup(&f);

    /* Use valid TCS indices within our implementation */
    int sleep_tcs = 2; /* Use TCS 2 for sleep */
    int wake_tcs = 3; /* Use TCS 3 for wake */
    int num_cmds = 3;

    /* Program SLEEP TCS with power down sequence */
    for (int i = 0; i < num_cmds; i++) {
        uint32_t sleep_addr = 0x40000324 + (i * 4); /* Sequential PMIC regs */
        uint32_t sleep_data = 0x00000000; /* Power down voltage */
        uint32_t msgid = CMD_MSGID_WRITE; /* No response for SLEEP */

        rpmh_rsc_writel(&f, tcs_cmd_addr(&f, sleep_tcs, i, RSC_DRV_CMD_MSGID),
                        msgid);
        rpmh_rsc_writel(&f, tcs_cmd_addr(&f, sleep_tcs, i, RSC_DRV_CMD_ADDR),
                        sleep_addr);
        rpmh_rsc_writel(&f, tcs_cmd_addr(&f, sleep_tcs, i, RSC_DRV_CMD_DATA),
                        sleep_data);
    }

    /* Program WAKE TCS with power up sequence */
    for (int i = 0; i < num_cmds; i++) {
        uint32_t wake_addr = 0x40000324 + (i * 4);
        uint32_t wake_data = 0x006F0000; /* Power up voltage */
        uint32_t msgid = CMD_MSGID_WRITE;

        rpmh_rsc_writel(&f, tcs_cmd_addr(&f, wake_tcs, i, RSC_DRV_CMD_MSGID),
                        msgid);
        rpmh_rsc_writel(&f, tcs_cmd_addr(&f, wake_tcs, i, RSC_DRV_CMD_ADDR),
                        wake_addr);
        rpmh_rsc_writel(&f, tcs_cmd_addr(&f, wake_tcs, i, RSC_DRV_CMD_DATA),
                        wake_data);
    }

    /* Enable commands in both TCSes */
    uint32_t cmd_mask = (1 << num_cmds) - 1;
    rpmh_rsc_writel(&f, tcs_reg_addr(&f, sleep_tcs, RSC_DRV_CMD_ENABLE),
                    cmd_mask);
    rpmh_rsc_writel(&f, tcs_reg_addr(&f, wake_tcs, RSC_DRV_CMD_ENABLE),
                    cmd_mask);

    /* Verify commands were programmed correctly */
    for (int i = 0; i < num_cmds; i++) {
        /* Check SLEEP TCS */
        g_assert_cmphex(rpmh_rsc_readl(&f, tcs_cmd_addr(&f, sleep_tcs, i,
                                                        RSC_DRV_CMD_DATA)),
                        ==, 0x00000000);

        /* Check WAKE TCS */
        g_assert_cmphex(rpmh_rsc_readl(&f, tcs_cmd_addr(&f, wake_tcs, i,
                                                        RSC_DRV_CMD_DATA)),
                        ==, 0x006F0000);
    }

    rpmh_rsc_teardown(&f);
}

/* Test TCS conflict detection (same address in multiple TCSes) */
static void test_rpmh_rsc_conflict_detection(void)
{
    RpmhRscTestFixture f = {0};
    rpmh_rsc_setup(&f);

    int tcs1 = 0;
    int tcs2 = 1;
    uint32_t same_addr = 0x40000324;

    /* Program same address in two different TCSes */
    rpmh_rsc_writel(&f, tcs_cmd_addr(&f, tcs1, 0, RSC_DRV_CMD_ADDR), same_addr);
    rpmh_rsc_writel(&f, tcs_cmd_addr(&f, tcs1, 0, RSC_DRV_CMD_DATA),
                    0x11111111);
    rpmh_rsc_writel(&f, tcs_cmd_addr(&f, tcs1, 0, RSC_DRV_CMD_MSGID),
                    CMD_MSGID_WRITE);

    rpmh_rsc_writel(&f, tcs_cmd_addr(&f, tcs2, 0, RSC_DRV_CMD_ADDR), same_addr);
    rpmh_rsc_writel(&f, tcs_cmd_addr(&f, tcs2, 0, RSC_DRV_CMD_DATA),
                    0x22222222);
    rpmh_rsc_writel(&f, tcs_cmd_addr(&f, tcs2, 0, RSC_DRV_CMD_MSGID),
                    CMD_MSGID_WRITE);

    /* Enable first command */
    rpmh_rsc_writel(&f, tcs_reg_addr(&f, tcs1, RSC_DRV_CMD_ENABLE), BIT(0));
    rpmh_rsc_writel(&f, RSC_DRV_IRQ_ENABLE, BIT(tcs1));

    /* Trigger first TCS */
    rpmh_rsc_writel(&f, tcs_reg_addr(&f, tcs1, RSC_DRV_CONTROL),
                    TCS_AMC_MODE_ENABLE | TCS_AMC_MODE_TRIGGER);

    /* Verify first command completed */
    uint32_t status1 = rpmh_rsc_readl(&f, tcs_cmd_addr(&f, tcs1, 0,
                                                        RSC_DRV_CMD_STATUS));
    g_assert_cmphex(status1 & CMD_STATUS_COMPL, ==, CMD_STATUS_COMPL);

    /* Clean up first TCS */
    rpmh_rsc_writel(&f, tcs_reg_addr(&f, tcs1, RSC_DRV_CONTROL), 0);
    rpmh_rsc_writel(&f, tcs_reg_addr(&f, tcs1, RSC_DRV_CMD_ENABLE), 0);
    rpmh_rsc_writel(&f, RSC_DRV_IRQ_CLEAR, BIT(tcs1));

    /* Now trigger second TCS with conflicting address */
    rpmh_rsc_writel(&f, tcs_reg_addr(&f, tcs2, RSC_DRV_CMD_ENABLE), BIT(0));
    rpmh_rsc_writel(&f, RSC_DRV_IRQ_ENABLE, BIT(tcs2));
    rpmh_rsc_writel(&f, tcs_reg_addr(&f, tcs2, RSC_DRV_CONTROL),
                    TCS_AMC_MODE_ENABLE | TCS_AMC_MODE_TRIGGER);

    /* Verify second command also completed */
    uint32_t status2 = rpmh_rsc_readl(&f, tcs_cmd_addr(&f, tcs2, 0,
                                                        RSC_DRV_CMD_STATUS));
    g_assert_cmphex(status2 & CMD_STATUS_COMPL, ==, CMD_STATUS_COMPL);

    rpmh_rsc_teardown(&f);
}

/* Test TCS borrowing (WAKE TCS used for ACTIVE when ACTIVE unavailable) */
static void test_rpmh_rsc_tcs_borrowing(void)
{
    RpmhRscTestFixture f = {0};
    rpmh_rsc_setup(&f);

    /* Use valid TCS indices */
    int active_tcs = 0;
    int wake_tcs = 3; /* This will be "borrowed" for active use */

    /* Enable IRQs for both */
    rpmh_rsc_writel(&f, RSC_DRV_IRQ_ENABLE, BIT(active_tcs) | BIT(wake_tcs));

    /* Fill up ACTIVE TCS with a long-running command */
    rpmh_rsc_writel(&f, tcs_cmd_addr(&f, active_tcs, 0, RSC_DRV_CMD_ADDR),
                    0x40000324);
    rpmh_rsc_writel(&f, tcs_cmd_addr(&f, active_tcs, 0, RSC_DRV_CMD_DATA),
                    0x11111111);
    rpmh_rsc_writel(&f, tcs_cmd_addr(&f, active_tcs, 0, RSC_DRV_CMD_MSGID),
                    CMD_MSGID_WRITE | CMD_MSGID_RESP_REQ);
    rpmh_rsc_writel(&f, tcs_reg_addr(&f, active_tcs, RSC_DRV_CMD_ENABLE),
                    BIT(0));
    rpmh_rsc_writel(&f, tcs_reg_addr(&f, active_tcs, RSC_DRV_CONTROL),
                    TCS_AMC_MODE_ENABLE | TCS_AMC_MODE_TRIGGER);

    /* Now "borrow" WAKE TCS for another active request */
    rpmh_rsc_writel(&f, tcs_cmd_addr(&f, wake_tcs, 0, RSC_DRV_CMD_ADDR),
                    0x40000328);
    rpmh_rsc_writel(&f, tcs_cmd_addr(&f, wake_tcs, 0, RSC_DRV_CMD_DATA),
                    0x22222222);
    rpmh_rsc_writel(&f, tcs_cmd_addr(&f, wake_tcs, 0, RSC_DRV_CMD_MSGID),
                    CMD_MSGID_WRITE | CMD_MSGID_RESP_REQ);
    rpmh_rsc_writel(&f, tcs_reg_addr(&f, wake_tcs, RSC_DRV_CMD_ENABLE),
                    BIT(0));
    rpmh_rsc_writel(&f, tcs_reg_addr(&f, wake_tcs, RSC_DRV_CONTROL),
                    TCS_AMC_MODE_ENABLE | TCS_AMC_MODE_TRIGGER);

    /* Verify both commands completed */
    uint32_t status1 = rpmh_rsc_readl(&f, tcs_cmd_addr(&f, active_tcs, 0,
                                                        RSC_DRV_CMD_STATUS));
    uint32_t status2 = rpmh_rsc_readl(&f, tcs_cmd_addr(&f, wake_tcs, 0,
                                                        RSC_DRV_CMD_STATUS));

    g_assert_cmphex(status1 & CMD_STATUS_COMPL, ==, CMD_STATUS_COMPL);
    g_assert_cmphex(status2 & CMD_STATUS_COMPL, ==, CMD_STATUS_COMPL);

    /* Check both TCSes triggered IRQs */
    uint32_t irq_status = rpmh_rsc_readl(&f, f.tcs_offset + RSC_DRV_IRQ_STATUS);
    g_assert_cmphex(irq_status & (BIT(active_tcs) | BIT(wake_tcs)), ==,
                    BIT(active_tcs) | BIT(wake_tcs));

    rpmh_rsc_teardown(&f);
}

/* Test resource address validation (typical PMIC addresses) */
static void test_rpmh_rsc_resource_addresses(void)
{
    RpmhRscTestFixture f = {0};
    rpmh_rsc_setup(&f);

    int tcs_id = 0;

    /* Test typical RPMH resource addresses used by kernel */
    uint32_t test_addresses[] = {
        0x40000324, /* PMIC voltage regulator */
        0x40000400, /* PMIC LDO */
        0x30000100, /* Clock controller */
        0x50000200, /* Bus bandwidth */
        0x60000300  /* Interconnect */
    };

    for (int i = 0; i < 5; i++) {
        uint32_t addr = test_addresses[i];
        uint32_t data = 0x12340000 + i;

        /* Program command */
        rpmh_rsc_writel(&f, tcs_cmd_addr(&f, tcs_id, i, RSC_DRV_CMD_ADDR),
                        addr);
        rpmh_rsc_writel(&f, tcs_cmd_addr(&f, tcs_id, i, RSC_DRV_CMD_DATA),
                        data);
        rpmh_rsc_writel(&f, tcs_cmd_addr(&f, tcs_id, i, RSC_DRV_CMD_MSGID),
                        CMD_MSGID_WRITE);

        /* Verify readback */
        g_assert_cmphex(rpmh_rsc_readl(&f, tcs_cmd_addr(&f, tcs_id, i,
                                                        RSC_DRV_CMD_ADDR)),
                        ==, addr);
        g_assert_cmphex(rpmh_rsc_readl(&f, tcs_cmd_addr(&f, tcs_id, i,
                                                        RSC_DRV_CMD_DATA)),
                        ==, data);
    }

    /* Enable and trigger all commands */
    rpmh_rsc_writel(&f, tcs_reg_addr(&f, tcs_id, RSC_DRV_CMD_ENABLE), 0x1F);
    rpmh_rsc_writel(&f, RSC_DRV_IRQ_ENABLE, BIT(tcs_id));
    rpmh_rsc_writel(&f, tcs_reg_addr(&f, tcs_id, RSC_DRV_CONTROL),
                    TCS_AMC_MODE_ENABLE | TCS_AMC_MODE_TRIGGER);

    /* Verify all commands completed */
    for (int i = 0; i < 5; i++) {
        uint32_t status = rpmh_rsc_readl(&f, tcs_cmd_addr(&f, tcs_id, i,
                                                          RSC_DRV_CMD_STATUS));
        g_assert_cmphex(status & CMD_STATUS_COMPL, ==, CMD_STATUS_COMPL);
    }

    rpmh_rsc_teardown(&f);
}

/* Test power management state transitions (like kernel suspend/resume) */
static void test_rpmh_rsc_power_transitions(void)
{
    RpmhRscTestFixture f = {0};
    rpmh_rsc_setup(&f);

    /* Simulate cache flush during suspend (like rpmh_flush) */
    int sleep_tcs = 2;
    int wake_tcs = 3;

    /* Program sleep sequence (power down) */
    rpmh_rsc_writel(&f, tcs_cmd_addr(&f, sleep_tcs, 0, RSC_DRV_CMD_ADDR),
                    0x40000324);
    rpmh_rsc_writel(&f, tcs_cmd_addr(&f, sleep_tcs, 0, RSC_DRV_CMD_DATA),
                    0x00000000); /* Power down */
    rpmh_rsc_writel(&f, tcs_cmd_addr(&f, sleep_tcs, 0, RSC_DRV_CMD_MSGID),
                    CMD_MSGID_WRITE);

    rpmh_rsc_writel(&f, tcs_cmd_addr(&f, sleep_tcs, 1, RSC_DRV_CMD_ADDR),
                    0x30000100);
    rpmh_rsc_writel(&f, tcs_cmd_addr(&f, sleep_tcs, 1, RSC_DRV_CMD_DATA),
                    0x00000000); /* Clock off */
    rpmh_rsc_writel(&f, tcs_cmd_addr(&f, sleep_tcs, 1, RSC_DRV_CMD_MSGID),
                    CMD_MSGID_WRITE);

    /* Program wake sequence (power up) */
    rpmh_rsc_writel(&f, tcs_cmd_addr(&f, wake_tcs, 0, RSC_DRV_CMD_ADDR),
                    0x40000324);
    rpmh_rsc_writel(&f, tcs_cmd_addr(&f, wake_tcs, 0, RSC_DRV_CMD_DATA),
                    0x006F0000); /* Nominal voltage */
    rpmh_rsc_writel(&f, tcs_cmd_addr(&f, wake_tcs, 0, RSC_DRV_CMD_MSGID),
                    CMD_MSGID_WRITE);

    rpmh_rsc_writel(&f, tcs_cmd_addr(&f, wake_tcs, 1, RSC_DRV_CMD_ADDR),
                    0x30000100);
    rpmh_rsc_writel(&f, tcs_cmd_addr(&f, wake_tcs, 1, RSC_DRV_CMD_DATA),
                    0x00000001); /* Clock on */
    rpmh_rsc_writel(&f, tcs_cmd_addr(&f, wake_tcs, 1, RSC_DRV_CMD_MSGID),
                    CMD_MSGID_WRITE);

    /* Enable commands */
    rpmh_rsc_writel(&f, tcs_reg_addr(&f, sleep_tcs, RSC_DRV_CMD_ENABLE), 0x03);
    rpmh_rsc_writel(&f, tcs_reg_addr(&f, wake_tcs, RSC_DRV_CMD_ENABLE), 0x03);

    /* Verify programming successful */
    g_assert_cmphex(rpmh_rsc_readl(&f, tcs_reg_addr(&f, sleep_tcs,
                                                    RSC_DRV_CMD_ENABLE)),
                    ==, 0x03);
    g_assert_cmphex(rpmh_rsc_readl(&f, tcs_reg_addr(&f, wake_tcs,
                                                    RSC_DRV_CMD_ENABLE)),
                    ==, 0x03);

    /* Sleep and wake TCSes are programmed but not triggered */
    /* They will be executed automatically by hardware during transitions */

    rpmh_rsc_teardown(&f);
}

/* Test kernel driver synchronization patterns (IRQ-based completion) */
static void test_rpmh_rsc_kernel_synchronization_patterns(void)
{
    RpmhRscTestFixture f = {0};
    rpmh_rsc_setup(&f);

    /* Test the exact sequence kernel driver uses for blocking operations */
    int tcs_id = 0;
    int cmd_id = 0;

    /* 1. Setup command (like __tcs_buffer_write in kernel) */
    uint32_t addr = 0x40000324;  /* Real PMIC voltage register */
    uint32_t data = 0x006F0000;  /* 1.11V encoded voltage */
    uint32_t msgid = CMD_MSGID_WRITE | CMD_MSGID_RESP_REQ;

    rpmh_rsc_writel(&f, tcs_cmd_addr(&f, tcs_id, cmd_id, RSC_DRV_CMD_MSGID),
                    msgid);
    rpmh_rsc_writel(&f, tcs_cmd_addr(&f, tcs_id, cmd_id, RSC_DRV_CMD_ADDR),
                    addr);
    rpmh_rsc_writel(&f, tcs_cmd_addr(&f, tcs_id, cmd_id, RSC_DRV_CMD_DATA),
                    data);

    /* 2. Enable command */
    rpmh_rsc_writel(&f, tcs_reg_addr(&f, tcs_id, RSC_DRV_CMD_ENABLE),
                    BIT(cmd_id));

    /* 3. Enable IRQ (like kernel probe does) */
    rpmh_rsc_writel(&f, RSC_DRV_IRQ_ENABLE, BIT(tcs_id));

    /* 4. Verify initial state before trigger */
    g_assert_cmphex(rpmh_rsc_readl(&f, f.tcs_offset + RSC_DRV_IRQ_STATUS),
                    ==, 0);
    g_assert_cmphex(rpmh_rsc_readl(&f, tcs_cmd_addr(&f, tcs_id, cmd_id,
                                                    RSC_DRV_CMD_STATUS)),
                    ==, 0);

    /* 5. Trigger TCS (like __tcs_set_trigger in kernel) */
    rpmh_rsc_writel(&f, tcs_reg_addr(&f, tcs_id, RSC_DRV_CONTROL),
                    TCS_AMC_MODE_ENABLE | TCS_AMC_MODE_TRIGGER);

    /*
     * 6. Hardware should immediately:
     *    a) Set command status to ISSUED|COMPL
     *    b) Set IRQ status bit for this TCS
     *    c) Generate IRQ (would wake kernel waiters)
     */
    uint32_t cmd_status = rpmh_rsc_readl(&f, tcs_cmd_addr(&f, tcs_id, cmd_id,
                                                          RSC_DRV_CMD_STATUS));
    g_assert_cmphex(cmd_status & CMD_STATUS_ISSUED, ==, CMD_STATUS_ISSUED);
    g_assert_cmphex(cmd_status & CMD_STATUS_COMPL, ==, CMD_STATUS_COMPL);

    uint32_t irq_status = rpmh_rsc_readl(&f, f.tcs_offset + RSC_DRV_IRQ_STATUS);
    g_assert_cmphex(irq_status & BIT(tcs_id), ==, BIT(tcs_id));

    /*
     * 7. Kernel IRQ handler (tcs_tx_done) would:
     *    a) Read IRQ status
     *    b) Clear TCS control
     *    c) Clear command enable
     *    d) Clear IRQ status
     *    e) Wake up waiters
     */

    /* Clear TCS control (disable AMC mode) */
    rpmh_rsc_writel(&f, tcs_reg_addr(&f, tcs_id, RSC_DRV_CONTROL), 0);

    /* Clear command enable */
    rpmh_rsc_writel(&f, tcs_reg_addr(&f, tcs_id, RSC_DRV_CMD_ENABLE), 0);

    /* Clear IRQ status */
    rpmh_rsc_writel(&f, RSC_DRV_IRQ_CLEAR, BIT(tcs_id));

    /* 8. Verify IRQ properly cleared */
    g_assert_cmphex(rpmh_rsc_readl(&f, f.tcs_offset + RSC_DRV_IRQ_STATUS),
                    ==, 0);

    rpmh_rsc_teardown(&f);
}

/* Test multiple TCS IRQ handling (like kernel TCS resource management) */
static void test_rpmh_rsc_multiple_tcs_irq_handling(void)
{
    RpmhRscTestFixture f = {0};
    rpmh_rsc_setup(&f);

    /* Enable IRQs for multiple TCSes */
    uint32_t multi_tcs_mask = 0x07; /* TCS 0, 1, 2 */
    rpmh_rsc_writel(&f, RSC_DRV_IRQ_ENABLE, multi_tcs_mask);

    /* Trigger multiple TCSes simultaneously (like kernel under load) */
    for (int tcs = 0; tcs < 3; tcs++) {
        /* Setup command */
        rpmh_rsc_writel(&f, tcs_cmd_addr(&f, tcs, 0, RSC_DRV_CMD_ADDR),
                        0x40000324 + tcs * 4);
        rpmh_rsc_writel(&f, tcs_cmd_addr(&f, tcs, 0, RSC_DRV_CMD_DATA),
                        0x12340000 + tcs);
        rpmh_rsc_writel(&f, tcs_cmd_addr(&f, tcs, 0, RSC_DRV_CMD_MSGID),
                        CMD_MSGID_WRITE);

        /* Enable and trigger */
        rpmh_rsc_writel(&f, tcs_reg_addr(&f, tcs, RSC_DRV_CMD_ENABLE),
                        BIT(0));
        rpmh_rsc_writel(&f, tcs_reg_addr(&f, tcs, RSC_DRV_CONTROL),
                        TCS_AMC_MODE_ENABLE | TCS_AMC_MODE_TRIGGER);
    }

    /*
     * Verify all TCSes completed and generated IRQs
     * (Kernel IRQ handler processes multiple bits in IRQ status)
     */
    uint32_t irq_status = rpmh_rsc_readl(&f, f.tcs_offset + RSC_DRV_IRQ_STATUS);
    g_assert_cmphex(irq_status & multi_tcs_mask, ==, multi_tcs_mask);

    /* Verify each TCS command completed */
    for (int tcs = 0; tcs < 3; tcs++) {
        uint32_t status = rpmh_rsc_readl(&f, tcs_cmd_addr(&f, tcs, 0,
                                                          RSC_DRV_CMD_STATUS));
        g_assert_cmphex(status & CMD_STATUS_COMPL, ==, CMD_STATUS_COMPL);
    }

    /* Clear IRQs individually (like kernel handler) */
    for (int tcs = 0; tcs < 3; tcs++) {
        rpmh_rsc_writel(&f, RSC_DRV_IRQ_CLEAR, BIT(tcs));

        /* Verify only this TCS IRQ cleared */
        uint32_t remaining = rpmh_rsc_readl(&f, f.tcs_offset +
                                            RSC_DRV_IRQ_STATUS);
        uint32_t expected = multi_tcs_mask & ~((1 << (tcs + 1)) - 1);
        g_assert_cmphex(remaining, ==, expected);
    }

    /* Verify all IRQs cleared */
    g_assert_cmphex(rpmh_rsc_readl(&f, f.tcs_offset + RSC_DRV_IRQ_STATUS),
                    ==, 0);

    rpmh_rsc_teardown(&f);
}

/* Test register write synchronization (like write_tcs_reg_sync) */
static void test_rpmh_rsc_register_write_synchronization(void)
{
    RpmhRscTestFixture f = {0};
    rpmh_rsc_setup(&f);

    int tcs_id = 0;
    uint32_t test_values[] = { 0x12345678, 0xABCDEF01, 0x55AA55AA, 0x0 };

    /*
     * Kernel requires register writes to be immediately visible
     * on readback (write_tcs_reg_sync function)
     */
    for (int i = 0; i < 4; i++) {
        uint32_t test_val = test_values[i];

        /* Write to control register */
        rpmh_rsc_writel(&f, tcs_reg_addr(&f, tcs_id, RSC_DRV_CONTROL),
                        test_val);

        /* Immediate readback must match (no polling needed in our model) */
        uint32_t readback = rpmh_rsc_readl(&f, tcs_reg_addr(&f, tcs_id,
                                                            RSC_DRV_CONTROL));
        g_assert_cmphex(readback, ==, test_val);

        /* Test command enable register */
        rpmh_rsc_writel(&f, tcs_reg_addr(&f, tcs_id, RSC_DRV_CMD_ENABLE),
                        test_val);
        readback = rpmh_rsc_readl(&f, tcs_reg_addr(&f, tcs_id,
                                                   RSC_DRV_CMD_ENABLE));
        g_assert_cmphex(readback, ==, test_val);
    }

    rpmh_rsc_teardown(&f);
}

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);

    qtest_add_func("/sa8775p/rpmh-rsc/basic-registers",
                   test_rpmh_rsc_basic_registers);
    qtest_add_func("/sa8775p/rpmh-rsc/tcs-commands",
                   test_rpmh_rsc_tcs_commands);
    qtest_add_func("/sa8775p/rpmh-rsc/tcs-trigger-irq",
                   test_rpmh_rsc_tcs_trigger_and_irq);
    qtest_add_func("/sa8775p/rpmh-rsc/multi-command-tcs",
                   test_rpmh_rsc_multi_command_tcs);
    qtest_add_func("/sa8775p/rpmh-rsc/tcs-invalidate",
                   test_rpmh_rsc_tcs_invalidate);
    qtest_add_func("/sa8775p/rpmh-rsc/configuration",
                   test_rpmh_rsc_configuration);

    /* Kernel-like usage pattern tests */
    qtest_add_func("/sa8775p/rpmh-rsc/kernel-init-sequence",
                   test_rpmh_rsc_kernel_init_sequence);
    qtest_add_func("/sa8775p/rpmh-rsc/active-request-flow",
                   test_rpmh_rsc_active_request_flow);
    qtest_add_func("/sa8775p/rpmh-rsc/sleep-wake-programming",
                   test_rpmh_rsc_sleep_wake_programming);
    qtest_add_func("/sa8775p/rpmh-rsc/conflict-detection",
                   test_rpmh_rsc_conflict_detection);
    qtest_add_func("/sa8775p/rpmh-rsc/tcs-borrowing",
                   test_rpmh_rsc_tcs_borrowing);
    qtest_add_func("/sa8775p/rpmh-rsc/resource-addresses",
                   test_rpmh_rsc_resource_addresses);
    qtest_add_func("/sa8775p/rpmh-rsc/power-transitions",
                   test_rpmh_rsc_power_transitions);
    qtest_add_func("/sa8775p/rpmh-rsc/kernel-synchronization-patterns",
                   test_rpmh_rsc_kernel_synchronization_patterns);
    qtest_add_func("/sa8775p/rpmh-rsc/multiple-tcs-irq-handling",
                   test_rpmh_rsc_multiple_tcs_irq_handling);
    qtest_add_func("/sa8775p/rpmh-rsc/register-write-synchronization",
                   test_rpmh_rsc_register_write_synchronization);

    return g_test_run();
}

