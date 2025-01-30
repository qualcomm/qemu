/*
 *  Copyright(c) 2023-2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <hexagon_standalone.h>
#include <stdio.h>

int called;
int ssr_cc1;
int ssr_cc2;
int diag;

/*
 * Execute my_isr twice:
 *  -- first pass will handle the invalid insn
 *     -- the first pass will trigger another exception while ssr:ex==1 thus
 * double exception.
 *  -- second pass is the double exception.
 *     -- the second pass will verify that diag is set to the original ssr:cc
 */

/*
 * naked is requried for ISR routines.  This prevents the compiler from adding
 * frame save/restore
 */
__attribute__((naked)) void my_isr(void);

void my_isr()
{
    asm volatile(
        /* Keep track of the number of calls, should only get called twice. */
        "r11 = memw(##called);"
        "r11 = add(r11, #1);"
        "memw(##called) = r11;"

        /* If this is the second pass jump diag read and return to main */
        "{p0 = cmp.gt(r11, #1); if (p0.new) jump:t 1f}"

        /* First Pass, we trigger a double exception */
        "r12 = ssr;"
        "memw(##ssr_cc1) = r12;"
        "r12 = elr;"
        ".long 0x1f1f1f1f;"
        /* Second pass, we should now be in the double exception */
        "1: r10 = ssr;"
        "memw(##ssr_cc2) = r10;"
        "r10 = diag;"
        "memw(##diag) = r10;"
        "r12 = add(r12, #4);"
        "elr = r12;"
        "rte;");
}

int main(int argc, char *argv[])
{
    int err = 0;

    set_event_handler(2, my_isr);
    asm volatile(".long 0x1f1f1f1f\n\t" : : : "r10", "r11", "r12");

    if (!called) {
        printf("Error event override failed.\n");
        err = 1;
    }
    if ((ssr_cc1 & 0xff) != 0x1d) {
        printf("Error unexpected first pass ssr:cc expected 0x1d got 0x%x\n",
               ssr_cc1 & 0xff);
        err = 1;
    }
    if ((ssr_cc2 & 0xff) != 0x03) {
        printf("Error unexpected second pass ssr:cc expected 0x3 got 0x%x\n",
               ssr_cc2 & 0xff);
        err = 1;
    }
    if ((diag & 0xff) != 0x1d) {
        printf("Error unexpected diag expected 0x1d got 0x%x\n", diag & 0xff);
        err = 1;
    }

    printf("%s\n", ((err) ? "FAIL" : "PASS"));
    printf(" diag = 0x%x, ssr_cc1 = 0x%x, ssr_cc2 = 0x%x\n", diag,
           ssr_cc1 & 0xff, ssr_cc2 & 0xff);
    return err;
}
