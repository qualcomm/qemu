/*
 *  Copyright(c) 2024 Qualcomm Innovation Center, Inc. All Rights Reserved.
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

#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#define SETVAL 0x12345678

#define SSR_CE_BIT 23
static void set_ssr_ce(void)
{
    asm volatile("r2 = ssr\n\t"
                 "r2 = setbit(r2, #%0)\n\t"
                 "ssr = r2\n\t"
                 : : "i"(SSR_CE_BIT));
}

void end_of_preparation(void)
{
    uint32_t r10, g0, upcyclelo, imask;
    asm volatile(
        "%0 = r10\n"
        "%1 = g0\n"
        "%2 = upcyclelo\n"
        "%3 = imask\n"
        : "=r"(r10), "=r"(g0), "=r"(upcyclelo), "=r"(imask)
    );
    assert(r10 == SETVAL);
    assert(g0 == SETVAL);
    assert(upcyclelo > 0x1200 && upcyclelo < 0x1400);
    assert(imask == (SETVAL & 0xffff));
}

int main()
{
    set_ssr_ce();
    end_of_preparation();
    puts("PASS");
    return 0;
}
