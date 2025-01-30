/*
 *  Copyright(c) 2019-2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "mmu.h"

bool framelimit_exception_found;

#define MIN_ALLOC_SIZE 8
#define STACK_SIZE 0x1000
#define OVERFLOW_SIZE (STACK_SIZE + MIN_ALLOC_SIZE)

uint32_t get_stack_ptr(void)
{
    uint32_t retval;
    asm volatile("%0 = r29\n" : "=r"(retval));
    return retval;
}

void set_framelimit(uint32_t x)
{
    asm volatile("framelimit = %0\n" : : "r"(x));
}

#define HEX_CAUSE_STACK_LIMIT 0x27
#define HEX_CAUSE_PRIV_USER_NO_SINS 0x1b
void check_for_framelimit_error_helper(uint32_t ssr)
{
    uint32_t cause = GET_FIELD(ssr, SSR_CAUSE);
    switch (cause) {
    case HEX_CAUSE_STACK_LIMIT:
        framelimit_exception_found = true;
        inc_elr(4); /* don't try to allocframe again */
        break;
    case HEX_CAUSE_PRIV_USER_NO_SINS:
        enter_kernel_mode();
        break;
    default:
        do_coredump();
        break;
    }
}

/* Set up the event handlers */
MY_EVENT_HANDLE(my_event_handle_error, check_for_framelimit_error_helper)

DEFAULT_EVENT_HANDLE(my_event_handle_nmi,         HANDLE_NMI_OFFSET)
DEFAULT_EVENT_HANDLE(my_event_handle_tlbmissrw,   HANDLE_TLBMISSRW_OFFSET)
DEFAULT_EVENT_HANDLE(my_event_handle_tlbmissx,    HANDLE_TLBMISSX_OFFSET)
DEFAULT_EVENT_HANDLE(my_event_handle_reset,       HANDLE_RESET_OFFSET)
DEFAULT_EVENT_HANDLE(my_event_handle_rsvd,        HANDLE_RSVD_OFFSET)
DEFAULT_EVENT_HANDLE(my_event_handle_trap0,       HANDLE_TRAP0_OFFSET)
DEFAULT_EVENT_HANDLE(my_event_handle_trap1,       HANDLE_TRAP1_OFFSET)
DEFAULT_EVENT_HANDLE(my_event_handle_int,         HANDLE_INT_OFFSET)
DEFAULT_EVENT_HANDLE(my_event_handle_fperror,     HANDLE_FPERROR_OFFSET)

void test_framelimit(bool overflow, bool user_mode)
{
    framelimit_exception_found = false;
    if (user_mode) {
        enter_user_mode();
    }
    set_framelimit(get_stack_ptr() - STACK_SIZE);
    if (overflow) {
        asm volatile("allocframe(#%0)\n" : : "i"(OVERFLOW_SIZE));
        if (!user_mode) {
            /*
             * In user mode we should have triggered an exception and, thus,
             * the stack was not updated.
             */
            asm volatile("deallocframe\n");
        }
    } else {
        asm volatile("allocframe(#%0)\n"
                     "deallocframe\n"
                     : : "i"(MIN_ALLOC_SIZE));
    }
    set_framelimit(0);
    enter_kernel_mode();
    check32(framelimit_exception_found, overflow && user_mode);
}

int main()
{
    puts("Testing FRAMELIMIT");
    install_my_event_vectors();

    test_framelimit(true, false);
    test_framelimit(false, true);
    test_framelimit(true, true);

    puts(err ? "FAIL" : "PASS");
    return err;
}
