/*
 *  Copyright(c) 2019-2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "hexagon_standalone.h"
#define NO_DEFAULT_EVENT_HANDLES
#include "mmu.h"


#define HEX_CAUSE_INVALID_OPCODE 0x015

void invalid_opcode(void)
{
    asm volatile (".word 0x6fffdffc\n\t");
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
    case HEX_CAUSE_INVALID_OPCODE:
        /* We don't want to replay this instruction, just note the exception */
        inc_elr(4);
        break;
    default:
        do_coredump();
        break;
    }
}

MAKE_ERR_HANDLER(my_err_handler, my_err_handler_helper)

#define INVALID_OPCODE_MAIN(test_name, test_func, exp_fail) \
    int main(void) \
    { \
        puts(test_name); \
        clear_exception_vector(my_exceptions); \
        INSTALL_ERR_HANDLER(my_err_handler); \
        test_func(); \
        check32(*my_exceptions, exp_fail ? 1 << HEX_CAUSE_INVALID_OPCODE : 0); \
        printf("%s\n", ((err) ? "FAIL" : "PASS"));\
        return err; \
    }
