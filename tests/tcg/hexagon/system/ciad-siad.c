/*
 *  Copyright(c) 2023 Qualcomm Innovation Center, Inc. All Rights Reserved.
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


static inline void siad(uint32_t val)
{
    asm volatile ("siad(%0);"
                  : : "r"(val));
    return;
}
static inline void ciad(uint32_t val)
{
    asm volatile ("ciad(%0);"
                  : : "r"(val));
    return;
}

static inline uint32_t getipendad()
{
    uint32_t reg;
    asm volatile ("%0=s20;"
                  : "=r"(reg));
    return reg;
}
int
main(int argc, char *argv[])
{
    siad(4);
    int ipend = getipendad();
    if (ipend != (0x4 << 16)) {
        goto fail;
    }
    ciad(4);
    ipend = getipendad();
    if (ipend) {
        goto fail;
    }

    printf("PASS\n");
    return 0;
fail:
    printf("FAIL\n");
    return 1;
}
