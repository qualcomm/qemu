/*
 *  Copyright(c) 2024-2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef HSV39_H
#define HSV39_H

#include <assert.h>
#include <hexagon_standalone.h>
#define NO_DEFAULT_EVENT_HANDLES
#include "mmu.h"

static inline void remove_hsv39_trans(int index)
{
    TLBEntry64 entry;
    entry.raw = tlbr(index);
    entry.V = 0;
    tlbw(entry.raw, index);
}

static inline uint32_t tlbp64(uint32_t asid, uint64_t VA)
{
    uint64_t lookup = (asid & 0x7f) | (uint64_t)(VA & 0xfffffffffffff000);
    uint32_t ret;
    asm volatile ("%0 = tlbp(%1)\n\t" : "=r"(ret) : "r"(lookup));
    return ret;
}

static TLBEntry64 add_hsv39_tlb_entry(int index, uint64_t va, uint64_t pa,
					            HSV39_PageSize page_size,
					            uint32_t xwru, uint32_t asid,
					            bool G, bool V)
{

    TLBEntry64 entry = hsv39_make_tlb_entry(va, pa, page_size, xwru, asid, G);
    entry.V = V;
    int32_t lookup_index = tlbp64(asid, va);
    if (lookup_index != TLB_NOT_FOUND) {
        remove_hsv39_trans(lookup_index);
    }
    hsv39_write_tlb_entry(entry.raw, index);
    return entry;
}

static inline uint64_t page_start64(uint64_t addr, uint64_t page_size)
{
    return addr & ~(page_size - 1ull);
}

#endif /* HSV39_H */
