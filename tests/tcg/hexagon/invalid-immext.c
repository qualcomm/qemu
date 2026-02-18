/*
 * Test that invalid immext are properly rejected.
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void *resume_pc;

static void handle_sigill(int sig, siginfo_t *info, void *puc)
{
    ucontext_t *uc = (ucontext_t *)puc;

    if (sig != SIGILL) {
        _exit(EXIT_FAILURE);
    }

    uc->uc_mcontext.r0 = SIGILL;
    uc->uc_mcontext.pc = (unsigned long)resume_pc;
}

/*
 * This should raise SIGILL due to the invalid immext.
 */
static int test_invalid_immext(void)
{
    int sig;

    asm volatile(
        "r0 = #0\n"
        "r1 = ##1f\n"
        "memw(%1) = r1\n"
        "{\n"
        "    immext(#0xccb35480)\n"
        "    r2 = r3\n"
        "}\n"
        "1:\n"
        "%0 = r0\n"
        : "=r"(sig)
        : "r"(&resume_pc)
        : "r0", "r1", "r2", "r3", "memory");

    return sig;
}

int main()
{
    struct sigaction act;

    memset(&act, 0, sizeof(act));
    act.sa_sigaction = handle_sigill;
    act.sa_flags = SA_SIGINFO;
    assert(sigaction(SIGILL, &act, NULL) == 0);

    assert(test_invalid_immext() == SIGILL);

    puts("PASS");
    return EXIT_SUCCESS;
}
