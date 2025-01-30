/*
 *  Copyright(c) 2019-2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#define DEBUG        0

#include "mmu.h"


/* Set up the event handlers */
MY_EVENT_HANDLE(my_event_handle_error,            my_event_handle_error_helper)

DEFAULT_EVENT_HANDLE(my_event_handle_nmi,         HANDLE_NMI_OFFSET)
DEFAULT_EVENT_HANDLE(my_event_handle_tlbmissrw,   HANDLE_TLBMISSRW_OFFSET)
DEFAULT_EVENT_HANDLE(my_event_handle_tlbmissx,    HANDLE_TLBMISSX_OFFSET)
DEFAULT_EVENT_HANDLE(my_event_handle_reset,       HANDLE_RESET_OFFSET)
DEFAULT_EVENT_HANDLE(my_event_handle_rsvd,        HANDLE_RSVD_OFFSET)
DEFAULT_EVENT_HANDLE(my_event_handle_trap0,       HANDLE_TRAP0_OFFSET)
DEFAULT_EVENT_HANDLE(my_event_handle_trap1,       HANDLE_TRAP1_OFFSET)
DEFAULT_EVENT_HANDLE(my_event_handle_int,         HANDLE_INT_OFFSET)
DEFAULT_EVENT_HANDLE(my_event_handle_fperror,     HANDLE_FPERROR_OFFSET)

void test_permissions(void)
{
    uint32_t data_addr = (uint32_t)&data;
    uint32_t data_page = page_start(data_addr, TARGET_PAGE_BITS);
    uint32_t data_offset = ONE_MB;
    uint32_t new_data_page = data_page + data_offset;
    uint32_t read_data_addr = data_addr + data_offset;
    uint8_t data_perm = TLB_X | TLB_W | TLB_U;
    mmu_func_t f = func_return_pc;
    uint32_t func_addr = (uint32_t)f;
    uint32_t func_page = page_start(func_addr, TARGET_PAGE_BITS);
    uint32_t func_offset = FIVE_MB;
    uint32_t new_func_page = func_page + func_offset;
    uint32_t exec_addr = func_addr + func_offset;
    uint8_t func_perm = TLB_W | TLB_R;
    uint32_t read_user_data_addr;
    uint32_t write_data_addr;
    uint32_t write_user_data_addr;

    exception_vector expected_exceptions;

    clear_exception_vector(my_exceptions);

    data = 0xdeadbeef;

    add_trans(1, new_data_page, data_page,
              PAGE_4K, data_perm, 0, 1, 1);
    check32(tlbp(0, read_data_addr), 1);

    data_offset = TWO_MB;
    new_data_page = data_page + data_offset;
    read_user_data_addr = data_addr + data_offset;
    data_perm = TLB_X | TLB_W | TLB_R;
    add_trans(2, new_data_page, data_page,
              PAGE_4K, data_perm, 0, 1, 1);
    check32(tlbp(0, read_user_data_addr), 2);

    data_offset = THREE_MB;
    new_data_page = data_page + data_offset;
    write_data_addr = data_addr + data_offset;
    data_perm = TLB_X | TLB_R | TLB_U;
    add_trans(3, new_data_page, data_page,
              PAGE_4K, data_perm, 0, 1, 1);
    check32(tlbp(0, write_data_addr), 3);

    data_offset = FOUR_MB;
    new_data_page = data_page + data_offset;
    write_user_data_addr = data_addr + data_offset;
    data_perm = TLB_X | TLB_R | TLB_W;
    add_trans(4, new_data_page, data_page,
              PAGE_4K, data_perm, 0, 1, 1);
    check32(tlbp(0, write_user_data_addr), 4);

    add_trans(5, new_func_page, func_page,
              PAGE_4K, func_perm, 0, 1, 1);
    check32(tlbp(0, exec_addr), 5);

    install_my_event_vectors();
    enter_user_mode();

    /* Load through the new VA */
    check32(*(mmu_variable *)read_data_addr, 0xdeadbeef);
    check32(*(mmu_variable *)read_user_data_addr, 0xdeadbeef);

    /* Store through the new VA */
    /* Make sure the replay happens before anything else */
    mmu_variable *p = (mmu_variable *)write_data_addr;
    asm volatile("memw(%0++#4) = %1\n\t"
                 : "+r"(p) : "r"(0xc0ffee) : "memory");
    check32(data, 0xc0ffee);
    check32((uint32_t)p, (uint32_t)write_data_addr + 4);

    *(mmu_variable *)write_user_data_addr = 0xc0ffee;
    check32(data, 0xc0ffee);

    mmu_func_t new_f = (mmu_func_t)exec_addr;
    check32((new_f()), (int)exec_addr);

    clear_exception_vector(expected_exceptions);
    set_exception_vector_bit(expected_exceptions, HEX_CAUSE_FETCH_NO_XPAGE);
    set_exception_vector_bit(expected_exceptions, HEX_CAUSE_FETCH_NO_UPAGE);
    set_exception_vector_bit(expected_exceptions, HEX_CAUSE_PRIV_NO_READ);
    set_exception_vector_bit(expected_exceptions, HEX_CAUSE_PRIV_NO_WRITE);
    set_exception_vector_bit(expected_exceptions, HEX_CAUSE_PRIV_NO_UREAD);
    set_exception_vector_bit(expected_exceptions, HEX_CAUSE_PRIV_NO_UWRITE);

#if DEBUG
    print_exception_vector(my_exceptions);
    print_exception_vector(expected_exceptions);
#endif
    check_exception_vector(my_exceptions, expected_exceptions);
}

int main()
{
    puts("Hexagon MMU permissions test");

    test_permissions();

    printf("%s\n", ((err) ? "FAIL" : "PASS"));
    return err;
}
