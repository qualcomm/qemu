/*
 * Test that instructions from a newer revision than the running CPU
 * are rejected with SIGILL.
 *
 * Compiled with -mv66 so that e_flags selects CPU v66. The test embeds
 * a v68 instruction (L2_loadw_aq: "r0 = memw_aq(r0)") via .word
 * encoding. The revision-gated decoder must reject it, and linux-user
 * must deliver SIGILL.
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
 * Try to execute "r0 = memw_aq(r0)" (L2_loadw_aq, introduced in v68).
 * On a v66 CPU this must raise SIGILL.
 *
 * 0x9200c800 = { r0 = memw_aq(r0) } (single-word packet, parse bits = 3)
 */
static int try_v68_insn(void)
{
    int sig;

    asm volatile(
        "r0 = #0\n"
        "r1 = ##1f\n"
        "memw(%1) = r1\n"
        ".word 0x9200c800\n"   /* { r0 = memw_aq(r0) } */
        "1:\n"
        "%0 = r0\n"
        : "=r"(sig)
        : "r"(&resume_pc)
        : "r0", "r1", "memory");

    return sig;
}

int main(void)
{
    struct sigaction act;

    memset(&act, 0, sizeof(act));
    act.sa_sigaction = handle_sigill;
    act.sa_flags = SA_SIGINFO;
    assert(sigaction(SIGILL, &act, NULL) == 0);

    assert(try_v68_insn() == SIGILL);

    puts("PASS");
    return EXIT_SUCCESS;
}
