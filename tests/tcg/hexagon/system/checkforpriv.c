/*
 *  Copyright(c) 2022-2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>


#define DEBUG        0

#include "mmu.h"

#define HEX_CAUSE_PRIV_USER_NO_SINS         0x1b
#define HEX_CAUSE_PRIV_USER_NO_GINS         0x1a

static bool priv_exception_found;
static bool guest_exception_found;

static inline void increment_elr(int x)
{
    asm volatile("r7 = elr\n\t"
                 "r7 = add(r7, %0)\n\t"
                 "elr = r7\n\t"
                 : : "r"(x) : "r7");
}

void checkforpriv_event_handle_error_helper(uint32_t ssr)
{
    uint32_t cause = GET_FIELD(ssr, SSR_CAUSE);

    /*
     * Once we have handled the exceptions we are looking for,
     * go back to kernel mode.
     * We need this because some of the subsequent functions
     * rely on this.
     */
    if (priv_exception_found && guest_exception_found &&
        (cause == HEX_CAUSE_PRIV_USER_NO_SINS ||
         cause == HEX_CAUSE_PRIV_USER_NO_GINS)) {
        enter_kernel_mode();
        return;
    }

    if (cause == HEX_CAUSE_PRIV_USER_NO_SINS) {
        priv_exception_found = true;
        increment_elr(4);
    } else if (cause == HEX_CAUSE_PRIV_USER_NO_GINS) {
        guest_exception_found = true;
        increment_elr(4);
    } else {
        do_coredump();
    }
}

/* Set up the event handlers */
MY_EVENT_HANDLE(my_event_handle_error, checkforpriv_event_handle_error_helper)

DEFAULT_EVENT_HANDLE(my_event_handle_nmi,         HANDLE_NMI_OFFSET)
DEFAULT_EVENT_HANDLE(my_event_handle_tlbmissrw,   HANDLE_TLBMISSRW_OFFSET)
DEFAULT_EVENT_HANDLE(my_event_handle_tlbmissx,    HANDLE_TLBMISSX_OFFSET)
DEFAULT_EVENT_HANDLE(my_event_handle_reset,       HANDLE_RESET_OFFSET)
DEFAULT_EVENT_HANDLE(my_event_handle_rsvd,        HANDLE_RSVD_OFFSET)
DEFAULT_EVENT_HANDLE(my_event_handle_trap0,       HANDLE_TRAP0_OFFSET)
DEFAULT_EVENT_HANDLE(my_event_handle_trap1,       HANDLE_TRAP1_OFFSET)
DEFAULT_EVENT_HANDLE(my_event_handle_int,         HANDLE_INT_OFFSET)
DEFAULT_EVENT_HANDLE(my_event_handle_fperror,     HANDLE_FPERROR_OFFSET)


int main()
{
    puts("Hexagon supervisor/guest permissions test");

    install_my_event_vectors();
    enter_user_mode();

    asm volatile("rte\n\t");
    check32(priv_exception_found, true);

    asm volatile("gelr = r0\n\t");
    check32(guest_exception_found, true);

    printf("%s\n", ((err) ? "FAIL" : "PASS"));
    return err;
}
