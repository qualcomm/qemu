/*
 * Test that the HMX F8 (FP8) instructions, introduced at v81, are
 * rejected with SIGILL on a v79 CPU, while the rest of HMX (present
 * since v75) still executes there.
 *
 * The assembler doesn't know the HMX mnemonics, so the instructions
 * are introduced with .word (see check_rev_gating.c).
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
static int signals_handled;
static int expected_signals;

static void handle_sigill(int sig, siginfo_t *info, void *puc)
{
    ucontext_t *uc = (ucontext_t *)puc;

    if (sig != SIGILL) {
        _exit(EXIT_FAILURE);
    }

    uc->uc_mcontext.r0 = SIGILL;
    uc->uc_mcontext.pc = (unsigned long)resume_pc;
    signals_handled++;
}

/*
 * Execute one instruction with r0 = 0 and return r0 afterwards: SIGILL
 * if the instruction was rejected, otherwise whatever it left in r0.
 */
#define TRY_FUNC(NAME, WORD) \
static int try_##NAME(void) \
{ \
    int sig; \
    asm volatile( \
        "r0 = #0\n" \
        "r1 = ##1f\n" \
        "memw(%[resume]) = r1\n" \
        WORD \
        "1:\n" \
        "%[sig] = r0\n" \
        : [sig] "=r"(sig) \
        : [resume] "r"(&resume_pc) \
        : "r0", "r1", "memory"); \
    return sig; \
}

/* F8 instructions, introduced at v81. */
TRY_FUNC(v81hmx_cvt_rs_f8,
         ".word 0xa6e0db10    /* cvt.f8=acc(r0) */\n")
TRY_FUNC(v81hmx_mxmem_f8,
         ".word 0xa6e0c11d    /* mxmem(r0,r1).f8=cvt */\n")
TRY_FUNC(v81hmx_mxmem_deep_f8,
         ".word 0xa6e0c11e    /* mxmem(r0,r1):deep.f8=cvt */\n")
TRY_FUNC(v81hmx_mxmem_sm_act_f8,
         ".word 0x9200c1e3    /* activation.f8=mxmem(r0,r1) */\n")
TRY_FUNC(v81hmx_mxmem_wei_f8,
         ".word 0x9200e147    /* weight.f8=mxmem(r0,r1) */\n")

/* Non-F8 HMX instructions, introduced at v75. */
TRY_FUNC(v75hmx_mxclracc_hf,
         ".word 0xa6e0c013    /* mxclracc.hf */\n")
TRY_FUNC(v75hmx_cvt_rs_hf,
         ".word 0xa6e0da10    /* cvt.hf=acc(r0) */\n")

int main(void)
{
    struct sigaction act;

    memset(&act, 0, sizeof(act));
    act.sa_sigaction = handle_sigill;
    act.sa_flags = SA_SIGINFO;
    assert(sigaction(SIGILL, &act, NULL) == 0);

    assert(try_v81hmx_cvt_rs_f8() == SIGILL);
    expected_signals++;
    assert(try_v81hmx_mxmem_f8() == SIGILL);
    expected_signals++;
    assert(try_v81hmx_mxmem_deep_f8() == SIGILL);
    expected_signals++;
    assert(try_v81hmx_mxmem_sm_act_f8() == SIGILL);
    expected_signals++;
    assert(try_v81hmx_mxmem_wei_f8() == SIGILL);
    expected_signals++;

    assert(try_v75hmx_mxclracc_hf() != SIGILL);
    assert(try_v75hmx_cvt_rs_hf() != SIGILL);

    assert(signals_handled == expected_signals);

    puts("PASS");
    return EXIT_SUCCESS;
}
