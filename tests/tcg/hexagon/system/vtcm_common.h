/*
 *  Copyright(c) 2023-2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
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
