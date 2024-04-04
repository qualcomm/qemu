/*
 *  Copyright(c) 2022-2023 Qualcomm Innovation Center, Inc. All Rights Reserved.
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
#include <stdint.h>
#include "hexagon_standalone.h"


void do_activation_weight(uintptr_t activations_vtcm,
                          unsigned activations_range,
                          uintptr_t weights_vtcm, unsigned weights_range)

{
    __asm__ __volatile__ (
    "{    activation.ub = mxmem(%0,%1):above:cm\n"
    "     weight.b = mxmem(%2,%3)\n}"
    :
    : "r" (activations_vtcm) , "r" (activations_range),
      "r" (weights_vtcm), "r" (weights_range)
    : "memory"
    );
}

void acquire_coproc()

{
    /* acquire coproc */
    asm volatile("R6=SSR\n"
               "R6=setbit(R6, #26)\n"
               "SSR = R6\n"
               "{ nop; }\n"
               "{ nop; }\n"
               "isync;\n"
               :
               :
               : "r6");
}

void setup_translation()

{
    void *base = (void *)0xd8000000;
    const unsigned pageSizeEnum = 64; /* 16mb */
    const unsigned perms = 0xf;
    const unsigned cachability = 6;
    const unsigned asid = 0;
    const unsigned aa = 0;
    const unsigned vg = 3;

    add_translation_extended(1, base,
      (uint64_t)base, pageSizeEnum, perms,
      cachability, asid, aa, vg);
}


int main()

{
    setup_translation();
    acquire_coproc();
    do_activation_weight(0xd8702740, 0xffff971f, 0xd8677800, 0x00000bff);
    printf("PASS\n");

    return 0;
}

