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

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <hexagon_standalone.h>
#include "hsv39.h"

/* Returns 4 ^ n */
static inline uint64_t pow4(uint64_t n)
{
    return 1ull << (n * 2);
}

void test_page_size(int pgsize_id)
{
    uint64_t page_size = pow4(__builtin_ctzll(pgsize_id)) * 1024 * 1024;
    uint64_t addr = (uint64_t)&data;
    uint64_t page = page_start64(addr, page_size);
    uint64_t offset = 4ull * 1024 * 1024 * 1024;
    uint64_t virt_addr = addr + offset;
    uint64_t virt_page = page + offset;
    int asid = 1, index = 512; /* HSV39 DMA TLB entries start from 512 */
    mmu_func_t f = func_return_pc;
    mmu_func_t new_f;
    printf("Testing page size 0x%llx\n", page_size);

    add_hsv39_tlb_entry(index, virt_page, page, pgsize_id, 0xf, asid, 1, 1);
    check32(tlbp64(asid, virt_addr), index);

    /* Load through the new VA */
    data = 0xdeadbeef;
    check32(*(mmu_variable *)virt_addr, 0xdeadbeef);

    /* Store through the new VA */
    *(mmu_variable *)virt_addr = 0xcafebabe;
    check32(data, 0xcafebabe);

    /* Clear out this entry */
    remove_hsv39_trans(index);
    check32(tlbp64(asid, virt_addr), TLB_NOT_FOUND);

    /* Set up a mapping for function execution */
    addr = (uint32_t)f;
    page = page_start64(addr, page_size);
    virt_page = page + offset;
    virt_addr = addr + offset;
    index++;
    add_hsv39_tlb_entry(index, virt_page, page, pgsize_id, 0xf, asid, 1, 1);
    check32(tlbp64(asid, virt_addr), index);

    /*
     * Call the function at the new address
     * It will return it's PC, which should be the new address
     */
    new_f = (mmu_func_t)virt_addr;
    check32((new_f()), (int)virt_addr);

    /* Clear out this entry */
    remove_hsv39_trans(index);
    check32(tlbp64(asid, virt_addr), TLB_NOT_FOUND);
}

int main()
{
    puts("Hexagon HSV39 MMU page size test");

    test_page_size(HSV39_PAGE_1M);
    test_page_size(HSV39_PAGE_4M);
    test_page_size(HSV39_PAGE_16M);
    test_page_size(HSV39_PAGE_256M);
    test_page_size(HSV39_PAGE_1G);
    test_page_size(HSV39_PAGE_4G);
    test_page_size(HSV39_PAGE_16G);
    test_page_size(HSV39_PAGE_64G);

    printf("%s\n", ((err) ? "FAIL" : "PASS"));
    return err;
}
