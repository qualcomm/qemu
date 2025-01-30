/*
 *  Copyright(c) 2022-2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <stdio.h>
#include <string.h>
#include "hexagon_standalone.h"
#define NO_DEFAULT_EVENT_HANDLES
#include "mmu.h"


#define HEX_CAUSE_UNSUPORTED_HVX_64B 0x002
#define HEX_CAUSE_NO_COPROC_ENABLE 0x016

static inline void hvx_insn(void)
{
    asm volatile("v0 = vrmpyb(v0, v1)\n\t");
}

#define CHANGE_V2X(bit_op) \
    asm volatile("r0 = syscfg\n\t" \
                 "r0 = " #bit_op "(r0, #7)\n\t" /* The V2X bit */ \
                 "syscfg = r0\n\t" \
                 "isync\n\t" \
                 : : : "r0")

static inline void set_hvx_64b(void)
{
    CHANGE_V2X(clrbit);
}

static inline void set_hvx_128b(void)
{
    CHANGE_V2X(setbit);
}

void my_err_handler_helper(uint32_t ssr)
{
    uint32_t cause = GET_FIELD(ssr, SSR_CAUSE);

    if (cause < 64) {
        *my_exceptions |= 1LL << cause;
    } else {
        *my_exceptions = cause;
    }

    switch (cause) {
    case HEX_CAUSE_UNSUPORTED_HVX_64B:
    case HEX_CAUSE_NO_COPROC_ENABLE:
        /* We don't want to replay this instruction, just note the exception */
        inc_elr(4);
        break;
    default:
        do_coredump();
        break;
    }
}

static void check_hvx(void (setup_fn)(void), int excp)
{
    setup_fn();
    hvx_insn();
    check32(*my_exceptions, excp);
    clear_exception_vector(my_exceptions);
}

MAKE_ERR_HANDLER(my_err_handler, my_err_handler_helper)

int main(int argc, char **argv)
{
    int rev;

    puts("Hexagon unsupported 64b mode hvx test");
    INSTALL_ERR_HANDLER(my_err_handler);

    /* Get running arch version. */
    asm volatile("r0 = rev\n"
                 "r1 = #255\n"
                 "%0 = and(r0, r1)\n"
                 : "=r"(rev)
                 :
                 : "r0", "r1");

    /* Version > V66 just ignored 64 bits config and calculates with 128 bits. */
    int excp_64b = rev <= 0x66
                   ? 1 << HEX_CAUSE_UNSUPORTED_HVX_64B
                   : 0;

    /*
     * The loop is to check that we get the right behavior even when
     * alternating the vector length during the program execution.
     */
    for (int i = 0; i < 4; i++) {
        if (i % 2) {
            check_hvx(set_hvx_64b, excp_64b);
        } else {
            check_hvx(set_hvx_128b, 0);
        }
    }

    /* Make sure that the NO_COPROC exception has higher priority. */
    asm volatile("r0 = ssr\n\t"
             "r0 = clrbit(r0, #31)\n\t" /* clear SSR:XE bit */
             "ssr = r0\n\t"
             "isync\n\t" \
             : : : "r0");
    check_hvx(set_hvx_64b, 1 << HEX_CAUSE_NO_COPROC_ENABLE);

    printf("%s\n", ((err) ? "FAIL" : "PASS"));
    return err;
}
