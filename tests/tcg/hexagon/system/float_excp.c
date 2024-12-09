
/*
 *  Copyright(c) 2020 Qualcomm Innovation Center, Inc. All Rights Reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdbool.h>

#include "mmu.h"

#define HEX_EVENT_FPTRAP 0xb
#define FPTRAP_CAUSE_BADFLOAT 0xbf

static bool fp_exception_found;
static uint32_t exception_elr, pre_exception_pc;

void set_usr_fp_exception_bits(void)
{
    asm volatile("r0 = usr\n\t"
                 "r0 = setbit(r0, #25)\n\t"
                 "r0 = setbit(r0, #26)\n\t"
                 "r0 = setbit(r0, #27)\n\t"
                 "r0 = setbit(r0, #28)\n\t"
                 "r0 = setbit(r0, #29)\n\t"
                 "usr = r0\n\t" : : : "r0");
}

void clear_usr_invalid(void)
{
    asm volatile("r0 = usr\n\t"
                 "r0 = clrbit(r0, #1)\n\t"
                 "usr = r0\n\t" : : : "r0");
}

void clear_usr_div_by_zero(void)
{
    asm volatile("r0 = usr\n\t"
                 "r0 = clrbit(r0, #2)\n\t"
                 "usr = r0\n\t" : : : "r0");
}

int get_usr(void)
{
    int ret;
    asm volatile("%0 = usr\n\t" : "=r"(ret));
    return ret;
}

int get_usr_invalid(void)
{
    return (get_usr() >> 1) & 1;
}

int get_usr_div_by_zero(void)
{
    return (get_usr() >> 2) & 1;
}

void gen_sfinvsqrta_exception(void)
{
    /* Force a invalid exception */
    float RsV = -1.0;
    asm volatile("R2,P0 = sfinvsqrta(%0)\n\t"
                 "R4 = sffixupd(R0, R1)\n\t"
                 : : "r"(RsV) : "r2", "p0", "r4");
}

void gen_sfrecipa_exception(void)
{
    /* Force a divide-by-zero exception */
    int RsV = 0x3f800000;
    int RtV = 0x00000000;
    asm volatile("%0 = pc\n\t"
                 "R2,P0 = sfrecipa(%1, %2)\n\t"
                 : "=r"(pre_exception_pc)
                 : "r"(RsV), "r"(RtV)
                 : "r2", "p0");
}

void check_fp_exception_helper(uint32_t ssr)
{
    uint32_t cause = GET_FIELD(ssr, SSR_CAUSE);

    if (cause == FPTRAP_CAUSE_BADFLOAT) {
        fp_exception_found = true;
        asm volatile("%0 = elr\n" : "=r"(exception_elr));
        inc_elr(4);
    } else {
        do_coredump();
    }
}

/* Set up the event handlers */
MY_EVENT_HANDLE(my_event_handle_fperror, check_fp_exception_helper)

DEFAULT_EVENT_HANDLE(my_event_handle_error,       HANDLE_ERROR_OFFSET)
DEFAULT_EVENT_HANDLE(my_event_handle_nmi,         HANDLE_NMI_OFFSET)
DEFAULT_EVENT_HANDLE(my_event_handle_tlbmissrw,   HANDLE_TLBMISSRW_OFFSET)
DEFAULT_EVENT_HANDLE(my_event_handle_tlbmissx,    HANDLE_TLBMISSX_OFFSET)
DEFAULT_EVENT_HANDLE(my_event_handle_reset,       HANDLE_RESET_OFFSET)
DEFAULT_EVENT_HANDLE(my_event_handle_rsvd,        HANDLE_RSVD_OFFSET)
DEFAULT_EVENT_HANDLE(my_event_handle_trap0,       HANDLE_TRAP0_OFFSET)
DEFAULT_EVENT_HANDLE(my_event_handle_trap1,       HANDLE_TRAP1_OFFSET)
DEFAULT_EVENT_HANDLE(my_event_handle_int,         HANDLE_INT_OFFSET)

int main(int argc, char *argv[])
{
    clear_usr_invalid();
    if (get_usr_invalid()) {
        printf("ERROR: usr invalid bit not cleared\n");
        err = 1;
        goto out;
    }

    gen_sfinvsqrta_exception();
    if (get_usr_invalid() == 0) {
        printf("ERROR: usr invalid bit not set\n");
        err = 1;
        goto out;
    }

    clear_usr_div_by_zero();
    if (get_usr_div_by_zero()) {
        printf("ERROR: usr div-by-zero bit not cleared\n");
        err = 1;
        goto out;
    }

    install_my_event_vectors();
    set_usr_fp_exception_bits();
    gen_sfrecipa_exception();
    check32(fp_exception_found, true);
    /*
     * ELR should have been the next PC after the failing instruction. See
     * section 5.10 of the System-Level spec:
     *
     * Floating point exceptions establish the exception point after the packet
     * that caused the error (like TRAP). The packet with the floating point
     * exception commits, and all register values are updated. Program flow
     * resumes at the exception handler.
     */
    check32(exception_elr, pre_exception_pc + 8);

out:
    printf("%s\n", ((err) ? "FAIL" : "PASS"));
    return err;
}
