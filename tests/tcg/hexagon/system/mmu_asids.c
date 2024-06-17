/*
 *  Copyright(c) 2019-2023 Qualcomm Innovation Center, Inc. All Rights Reserved.
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

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>


#define DEBUG        0

#include "mmu.h"

DEFAULT_EVENT_HANDLES

void test_asids(void)
{
    uint32_t addr = (uint32_t)&data;
    uint32_t page = page_start(addr, TARGET_PAGE_BITS);
    uint32_t offset = FIVE_MB;
    uint32_t new_addr = addr + offset;
    uint32_t new_page = page + offset;
    uint64_t entry =
        create_mmu_entry(0, 0, 0, 1, new_page, 1, 1, 1, 0, 7, page, PAGE_4K);
    /*
     * Create a TLB entry for ASID=1
     * Write it at index 1
     * Check that it is present
     * Invalidate the ASID
     * Check that it is not found
     */
    tlbw(entry, 1);
    check32(tlboc(entry), 1);
    tlbinvasid(entry >> 32);
    check32(tlboc(entry), TLB_NOT_FOUND);

    /*
     * Re-install the entry
     * Put ourselves in ASID=1
     * Do a load and a store
     */
    data = 0xdeadbeef;
    tlbw(entry, 1);
    set_asid(1);
    check32(*(mmu_variable *)new_addr, 0xdeadbeef);
    *(mmu_variable *)new_addr = 0xcafebabe;
    check32(data, 0xcafebabe);

    /*
     * Make sure a load from ASID 2 gets a different value.
     * The standalone runtime will create a VA==PA entry on
     * a TLB miss, so the load will be reading from uninitialized
     * memory.
     */
    set_asid(2);
    data = 0xdeadbeef;
    check32_ne(*(mmu_variable *)new_addr, 0xdeadbeef);

    /*
     * Invalidate the ASID and make sure a loads from ASID 1
     * gets a different value.
     */
    tlbinvasid(entry >> 32);
    set_asid(1);
    data = 0xcafebabe;
    check32_ne(*(mmu_variable *)new_addr, 0xcafebabe);
}

int main()
{
    puts("Hexagon MMU ASID test");

    test_asids();

    printf("%s\n", ((err) ? "FAIL" : "PASS"));
    return err;
}
