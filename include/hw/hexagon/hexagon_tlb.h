/*
 * Hexagon TLB QOM Object
 *
 * Copyright(c) 2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef HEXAGON_TLB_H
#define HEXAGON_TLB_H

#include "hw/qdev-core.h"
#include "hw/sysbus.h"
#include "qom/object.h"
#include "exec/cpu-defs.h"

#define TYPE_HEXAGON_TLB "hexagon-tlb"
OBJECT_DECLARE_SIMPLE_TYPE(HexagonTLBState, HEXAGON_TLB)

struct HexagonTLBState {
    SysBusDevice parent_obj;

    /*
     * 0    jtlb_entries   DMA_TLB_OFFSET    dma_jtlb_entries
     * v         v             v                    v
     * |*********|.............|++++++++++++++++++++|
     *
     * Where '*' are jtlb entries and '+' are dma jtlb entries.
     */
    uint32_t num_entries;    /* Number of regular TLB entries */
    uint32_t dma_entries;    /* Number of DMA TLB entries */

    /* Single TLB allocation with DMA entries at fixed offset */
    uint64_t *entries;       /* TLB entries array */

    /* Migration state */
    uint32_t save_size;      /* Size of array for migration */
};

/* TLB interface functions - device-independent interface */
uint64_t hexagon_tlb_read(HexagonTLBState *tlb, uint32_t index);
void hexagon_tlb_write(HexagonTLBState *tlb, uint32_t index, uint64_t value,
                       bool old_entry_valid, bool mmu_enabled,
                       uint32_t threadId, uint32_t wrapped_index);
bool hexagon_tlb_find_match(HexagonTLBState *tlb, uint8_t asid,
                            target_ulong VA, MMUAccessType access_type,
                            hwaddr *PA, int *prot, uint64_t *size,
                            int32_t *excp, int32_t *cause_code, int mmu_idx);
uint32_t hexagon_tlb_lookup(HexagonTLBState *tlb, uint8_t asid, uint32_t VA,
                            uint32_t *imprecise_exception, int32_t *cause_code);
uint32_t hexagon_tlb_lookup_extended(HexagonTLBState *tlb, uint8_t asid,
                                     uint64_t VA,
                                     uint32_t *imprecise_exception,
                                     int32_t *cause_code);
int hexagon_tlb_check_overlap(HexagonTLBState *tlb, uint64_t entry,
                              uint64_t index);
void hexagon_tlb_dump(HexagonTLBState *tlb);

/* TLB accessor functions */
uint32_t hexagon_tlb_get_num_entries(HexagonTLBState *tlb);
uint32_t hexagon_tlb_get_dma_entries(HexagonTLBState *tlb);
uint32_t hexagon_tlb_get_total_entries(HexagonTLBState *tlb);

#endif /* HEXAGON_TLB_H */
