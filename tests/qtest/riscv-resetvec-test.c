/*
 * QTest testcase for the RISC-V CPU "resetvec" property
 *
 * Verifies that "resetvec" can be set after the CPU is realized
 * (realized_set_allowed), which a reset then applies to env.resetvec.
 *
 * Copyright (c) 2026 Qualcomm Innovation Center, Inc. All Rights Reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "libqtest.h"
#include "qobject/qdict.h"

/* First hart of socket 0 on the "virt" machine. */
#define HART0_PATH "/machine/soc0/harts[0]"

static uint64_t qom_get_uint(QTestState *qts, const char *path, const char *prop)
{
    QDict *response;
    uint64_t value;

    response = qtest_qmp(qts,
                         "{ 'execute': 'qom-get',"
                         " 'arguments': { 'path': %s, 'property': %s } }",
                         path, prop);
    g_assert(qdict_haskey(response, "return"));
    value = qdict_get_int(response, "return");
    qobject_unref(response);
    return value;
}

static void run_test_resetvec(void)
{
    const uint64_t new_vec = 0x1db00000;
    QTestState *qts = qtest_init("-machine virt -cpu veyron-v1");
    QDict *response;

    /* The CPU is realized; setting "resetvec" must be allowed at runtime. */
    response = qtest_qmp(qts,
                         "{ 'execute': 'qom-set',"
                         " 'arguments': { 'path': %s,"
                         " 'property': 'resetvec',"
                         " 'value': %llu } }",
                         HART0_PATH, (unsigned long long)new_vec);
    g_assert(qdict_haskey(response, "return"));
    qobject_unref(response);

    /* Read it back to confirm the write landed. */
    g_assert_cmphex(qom_get_uint(qts, HART0_PATH, "resetvec"), ==, new_vec);

    qtest_quit(qts);
}

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);

    if (qtest_has_machine("virt")) {
        qtest_add_func("/cpu/resetvec", run_test_resetvec);
    }

    return g_test_run();
}
