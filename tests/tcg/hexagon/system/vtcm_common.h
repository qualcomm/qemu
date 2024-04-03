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


#ifndef VTCM_COMMON_H
#define VTCM_COMMON_H 1

#include <hexagon_standalone.h>
#include <stdlib.h>

#define VTCM_SIZE_KB (2048)
#define VTCM_BYTES_PER_KB (1024)
#define VTCM_PAGE_SIZE_MULT (128)

void *setup_vtcm(int page_size)
{
    unsigned char *vtcm_base = NULL;
    asm volatile("r1 = cfgbase\n"
                 "r1 = asl(r1, #5)\n"
                 "r2 = #0x38\n"
                 "r1 = memw_phys(r2, r1)\n"
                 "%0 = asl(r1, #16)\n"
                 : "=r"(vtcm_base)
                 :
                 : "r1", "r2");

    void *va = (void *)vtcm_base;
    uint64_t pa = (uint64_t)(void *)vtcm_base;
    add_translation_fixed(1, va, (void *)pa, 6, 7);
    add_translation_fixed(2, (char *)va + 1024 * 1024, (char *)pa + 1024 * 1024,
                          6, 7);

    printf("Adding %dKB VTCM Page at VA:%x PA:%llx\n",
           page_size * VTCM_PAGE_SIZE_MULT, (uintptr_t)va, pa);
    return va;
}

#define setup_default_vtcm() setup_vtcm(VTCM_SIZE_KB / VTCM_PAGE_SIZE_MULT)

#endif /* VTCM_COMMON_H */
