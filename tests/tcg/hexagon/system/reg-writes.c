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
#include <stdlib.h>
#define SETVAL 0x12345678

void failed(void)
{
    puts("FAIL");
    exit(1);
    __builtin_trap();
}

void passed(void)
{
    puts("PASS");
    exit(0);
    __builtin_trap();
}

/* naked to avoid having codegen alter the regs we want to test */
void __attribute__((naked, noreturn)) finalize(void)
{
    asm volatile(
        "r0 = p3:0\n"
        "p0 = cmp.eq(r0, #%0)\n"
        "if (!p0) call #failed\n"

        "r0 = r10\n"
        "p0 = cmp.eq(r0, #%0)\n"
        "if (!p0) call #failed\n"

        "r0 = g0\n"
        "p0 = cmp.eq(r0, #%0)\n"
        "if (!p0) call #failed\n"

        "r0 = imask\n"
        "p0 = cmp.eq(r0, #%0)\n"
        "if (!p0) call #failed\n"

        "call #passed\n"
        ".word 0x6fffdffc\n" /* invalid packet to cause an abort */
        :
        : "i"(SETVAL)
        : "r0", "p0"
    );
}

int main()
{
    failed(); /* should never reach here as lldb will change PC */
    return 0;
}
