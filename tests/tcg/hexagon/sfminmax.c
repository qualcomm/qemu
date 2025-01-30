/*
 *  Copyright(c) 2020-2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <stdio.h>

/*
 * This test checks that the FP invalid bit in USR is not set
 * when one of the operands is NaN.
 */
const int FPINVF_BIT = 1;

int err;

int main()
{
    int usr;

    asm volatile("r2 = usr\n\t"
                 "r2 = clrbit(r2, #%1)\n\t"
                 "usr = r2\n\t"
                 "r2 = ##0x7fc00000\n\t"    /* NaN */
                 "r3 = ##0x7f7fffff\n\t"
                 "r2 = sfmin(r2, r3)\n\t"
                 "%0 = usr\n\t"
                 : "=r"(usr) : "i"(FPINVF_BIT) : "r2", "r3", "usr");

    if (usr & (1 << FPINVF_BIT)) {
        puts("sfmin test failed");
        err++;
    }

    asm volatile("r2 = usr\n\t"
                 "r2 = clrbit(r2, #%1)\n\t"
                 "usr = r2\n\t"
                 "r2 = ##0x7fc00000\n\t"    /* NaN */
                 "r3 = ##0x7f7fffff\n\t"
                 "r2 = sfmax(r2, r3)\n\t"
                 "%0 = usr\n\t"
                 : "=r"(usr) : "i"(FPINVF_BIT) : "r2", "r3", "usr");

    if (usr & (1 << FPINVF_BIT)) {
        puts("sfmax test failed");
        err++;
    }

    puts(err ? "FAIL" : "PASS");
    return err ? 1 : 0;
}
