/*
 *  Copyright(c) 2020-2023 Qualcomm Innovation Center, Inc. All Rights Reserved.
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


#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "timer.h"

#if 0
#define DEBUG printf
#else
#define DEBUG(...)
#endif

int main()
{
    timer_init();

    uint64_t start = timer_read();
    for (int i = 0; i < 30; i++) {
        uint64_t val = timer_read();
        DEBUG("\treg:   %llu | %08llx\n", val, val);
        val = timer_read_pair();
        assert(val != 0);
        DEBUG("\tpair:  %llu | %08llx\n", val, val);
        val = utimer_read();
        assert(val != 0);
        DEBUG("\tureg:  %llu | %08llx\n", val, val);
        val = utimer_read_pair();
        DEBUG("\tupair: %llu | %08llx\n", val, val);
        assert(val != 0);
    }
    while (start == timer_read()) {
        ;
    }
    printf("PASS\n");
}
