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


#define HEX_CAUSE_NO_COPROC2_ENABLE 0x18

void invalid_coproc(void)
{
    asm volatile (".word 0xa6e0c011\n\t"); /* mxclracc */
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
    case HEX_CAUSE_NO_COPROC2_ENABLE:
        /* We don't want to replay this instruction, just note the exception */
        inc_elr(4);
        break;
    default:
        do_coredump();
        break;
    }
}

MAKE_ERR_HANDLER(my_err_handler, my_err_handler_helper)

int main()
{
    puts("Hexagon invalid coproc test");

    INSTALL_ERR_HANDLER(my_err_handler);
    invalid_coproc();
    check32(*my_exceptions, 1 << HEX_CAUSE_NO_COPROC2_ENABLE);

    printf("%s\n", ((err) ? "FAIL" : "PASS"));
    return err;
}
