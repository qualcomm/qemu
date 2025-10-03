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

/*
 * Forward declarations - we can't include cpu.h here due to
 * circular dependencies
 */

#define TYPE_HEXAGON_TLB "hexagon-tlb"
OBJECT_DECLARE_SIMPLE_TYPE(HexagonTLBState, HEXAGON_TLB)

struct HexagonTLBState {
    SysBusDevice parent_obj;

    /* Properties */
    uint32_t num_entries;
    uint32_t dma_entries;

    /* TLB entries - dynamically allocated based on num_entries */
    uint64_t *entries;
};

/* TLB interface functions - device-independent interface */
uint64_t hexagon_tlb_read(HexagonTLBState *tlb, uint32_t index);
void hexagon_tlb_write(HexagonTLBState *tlb, uint32_t index, uint64_t value,
                       bool old_entry_valid, bool mmu_enabled,
                       uint32_t threadId, uint32_t wrapped_index);
bool hexagon_tlb_find_match(HexagonTLBState *tlb, uint8_t asid,
                            target_ulong VA, MMUAccessType access_type,
                            hwaddr *PA, int *prot, uint64_t *size,
                            int32_t *excp, int32_t *cause_code, int mmu_idx,
                            uint32_t num_tlbs);
uint32_t hexagon_tlb_lookup(HexagonTLBState *tlb, uint8_t asid, uint32_t VA,
                            uint32_t *imprecise_exception, int32_t *cause_code,
                            uint32_t jtlb_entries);
uint32_t hexagon_tlb_lookup_extended(HexagonTLBState *tlb, uint8_t asid,
                                     uint64_t VA,
                                     uint32_t *imprecise_exception,
                                     int32_t *cause_code,
                                     uint32_t jtlb_entries);
int hexagon_tlb_check_overlap(HexagonTLBState *tlb, uint64_t entry,
                              uint64_t index, uint32_t num_tlbs);
void hexagon_tlb_dump(HexagonTLBState *tlb);

#endif /* HEXAGON_TLB_H */
