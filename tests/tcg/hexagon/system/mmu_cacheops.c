/*
 *  Copyright(c) 2022-2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 * Test that cache operations raise the proper exceptions
 * Strategy
 *     Install special exception handlers
 *         1) Record what happened so we can check they were invoked
 *         2) On TLB miss, create an entry with no r/w/x permission
 *         3) On a permission error, turn on the permission
 *     Perform cache operations on unmapped addresses
 *     Check for TLB miss and permission errors
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>


#define DEBUG        0

#include "mmu.h"

/* Set up the event handlers */
MY_EVENT_HANDLE(my_event_handle_error,        my_event_handle_error_helper)
MY_EVENT_HANDLE(my_event_handle_tlbmissrw,    my_event_handle_tlbmissrw_helper)
MY_EVENT_HANDLE(my_event_handle_tlbmissx,     my_event_handle_tlbmissx_helper)

DEFAULT_EVENT_HANDLE(my_event_handle_reset,       HANDLE_RESET_OFFSET)
DEFAULT_EVENT_HANDLE(my_event_handle_nmi,         HANDLE_NMI_OFFSET)
DEFAULT_EVENT_HANDLE(my_event_handle_rsvd,        HANDLE_RSVD_OFFSET)
DEFAULT_EVENT_HANDLE(my_event_handle_trap0,       HANDLE_TRAP0_OFFSET)
DEFAULT_EVENT_HANDLE(my_event_handle_trap1,       HANDLE_TRAP1_OFFSET)
DEFAULT_EVENT_HANDLE(my_event_handle_int,         HANDLE_INT_OFFSET)
DEFAULT_EVENT_HANDLE(my_event_handle_fperror,     HANDLE_FPERROR_OFFSET)

static void test_data_cache_ops(void)
{
    exception_vector expected_exceptions;

    install_my_event_vectors();
    enter_user_mode();

    /*
     * Check the data cache ops are touching the MMU
     * Each one should generate a TLB miss possibly followed by a
     * privilege exception
     *     ONE_MB will get no permissions, so will have a privilege exception
     *     TWO_MB will get read permission, so no privilege exception
     *     THREE_MB will get write permission, so no privilege exception
     */

    clear_exception_vector(my_exceptions);
    clear_exception_vector(expected_exceptions);
    set_exception_vector_bit(expected_exceptions, HEX_CAUSE_TLBMISSRW_READ);
    set_exception_vector_bit(expected_exceptions, HEX_CAUSE_PRIV_NO_READ);
    asm volatile("dcinva(%0)\n\t" : : "r"((uint32_t)&data + ONE_MB));
    check_exception_vector(my_exceptions, expected_exceptions);
#if DEBUG
    print_exception_vector(my_exceptions);
#endif

    clear_exception_vector(my_exceptions);
    clear_exception_vector(expected_exceptions);
    set_exception_vector_bit(expected_exceptions, HEX_CAUSE_TLBMISSRW_READ);
    asm volatile("dccleaninva(%0)\n\t" : : "r"((uint32_t)&data + TWO_MB));
    check_exception_vector(my_exceptions, expected_exceptions);
#if DEBUG
    print_exception_vector(my_exceptions);
#endif

    clear_exception_vector(my_exceptions);
    clear_exception_vector(expected_exceptions);
    set_exception_vector_bit(expected_exceptions, HEX_CAUSE_TLBMISSRW_READ);
    asm volatile("dccleana(%0)\n\t" : : "r"((uint32_t)&data + THREE_MB));
    check_exception_vector(my_exceptions, expected_exceptions);
#if DEBUG
    print_exception_vector(my_exceptions);
#endif

    /*
     * Check the instruction cache ops are touching the MMU
     * Instruction cache operations only generate TLB miss exceptions
     * They do not generate privilege exceptions
     */
    clear_exception_vector(my_exceptions);
    clear_exception_vector(expected_exceptions);
    set_exception_vector_bit(expected_exceptions, HEX_CAUSE_TLBMISSX_NORMAL);
    asm volatile("icinva(%0)\n\t" : : "r"((uint32_t)&data + FOUR_MB));
    check_exception_vector(my_exceptions, expected_exceptions);
#if DEBUG
    print_exception_vector(my_exceptions);
#endif
}

int main()
{
    puts("Test exception behavior of cache operations");

    test_data_cache_ops();

    printf("%s\n", ((err) ? "FAIL" : "PASS"));
    return err;
}
