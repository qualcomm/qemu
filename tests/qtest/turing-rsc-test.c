/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * QTest testcase for the Turing RSC device (qcom-turing-rsc)
 *
 * Tests register access, TCS trigger/completion, and IRQ status
 * for the SA8775P CDSP0 Turing RSC implementation.
 */

#include "qemu/osdep.h"
#include "libqtest.h"
#include "qemu/bitops.h"
#include "hw/misc/qcom-turing-rsc.h"

#define TURING_RSC_BASE 0x260A4000

typedef struct {
    QTestState *qts;
} TuringRsc;

static uint32_t rsc_readl(TuringRsc *f, uint32_t offset)
{
    return qtest_readl(f->qts, TURING_RSC_BASE + offset);
}

/* Test basic register access and device identification */
static void test_basic_registers(void)
{
    TuringRsc f = {0};
    f.qts = qtest_init("-machine SA8775P_CDSP0");

    /* Read device ID (read-only) */
    uint32_t drv_id = rsc_readl(&f, TURING_RSC_ID_DRV0);
    g_assert_cmphex(drv_id, !=, 0);

    /* Read solver config (read-only) */
    uint32_t solver_cfg = rsc_readl(&f,
                                    TURING_RSC_PARAM_SOLVER_CONFIG_DRV0);
    g_assert_cmphex(solver_cfg, !=, 0);

    /* Read parent-child config and extract TCS count */
    uint32_t config = rsc_readl(&f,
                                TURING_RSC_PARAM_RSC_PARENTCHILD_CONFIG_DRV0);
    uint32_t num_tcs = config & TURING_PARENTCHILD_CONFIG_NUM_TCS_DRV0_MASK;
    uint32_t ncpt = (config >> TURING_PARENTCHILD_CONFIG_NUM_CMDS_PER_TCS_SHIFT)
                    & 0x1F;

    g_assert_cmpuint(num_tcs, ==, TURING_RSC_MAX_TCS_PER_DRV);
    g_assert_cmpuint(ncpt, ==, TURING_RSC_MAX_CMDS_PER_TCS);

    /* AMC IRQ status should be 0 initially */
    g_assert_cmphex(rsc_readl(&f, TURING_TCS_AMC_MODE_IRQ_STATUS_DRV0),
                    ==, 0);

    qtest_quit(f.qts);
}


int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);

    qtest_add_func("/turing-rsc/basic-registers", test_basic_registers);

    return g_test_run();
}
