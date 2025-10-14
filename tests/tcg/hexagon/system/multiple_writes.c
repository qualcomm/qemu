/*
 *  Copyright(c) 2022-2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
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
#include "multiple_writes_tgts.h"

/*
 * Test packets below are intended to trigger exceptions.  In order
 * to handle the exceptions uniformly, the test cases are padded to
 * four instruction words.
 */
static int ELR_SKIP_BYTES = 4 * sizeof(int32_t);

#define HEX_CAUSE_REG_WRITE_CONFLICT 0x01d

void my_err_handler_helper(uint32_t ssr)
{
    uint32_t cause = GET_FIELD(ssr, SSR_CAUSE);

    if (cause < 64) {
        *my_exceptions |= 1LL << cause;
    } else {
        *my_exceptions = cause;
    }

    switch (cause) {
    case HEX_CAUSE_REG_WRITE_CONFLICT:
        /* We don't want to replay this instruction, just note the exception */
        inc_elr(ELR_SKIP_BYTES);
        break;
    default:
        do_coredump();
        break;
    }
}
MAKE_ERR_HANDLER(my_err_handler, my_err_handler_helper)

int main()
{
    puts("Hexagon multiple writes to the same reg test");

    multiple_write_legal();

    INSTALL_ERR_HANDLER(my_err_handler);

    multiple_writes_static();
    check32(*my_exceptions, 1 << HEX_CAUSE_REG_WRITE_CONFLICT);
    *my_exceptions &= ~(1 << HEX_CAUSE_REG_WRITE_CONFLICT);

    multiple_writes_mixed();
    check32(*my_exceptions, 1 << HEX_CAUSE_REG_WRITE_CONFLICT);
    *my_exceptions &= ~(1 << HEX_CAUSE_REG_WRITE_CONFLICT);

    check32(multiple_writes(), 0xff);
    check32(*my_exceptions, 1 << HEX_CAUSE_REG_WRITE_CONFLICT);
    *my_exceptions &= ~(1 << HEX_CAUSE_REG_WRITE_CONFLICT);

    printf("%s\n", ((err) ? "FAIL" : "PASS"));
    return err;
}
