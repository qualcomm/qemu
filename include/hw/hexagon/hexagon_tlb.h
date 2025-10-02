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

    /* TLB entries - dynamically allocated based on num_entries */
    uint64_t *entries;
};

/* TLB interface functions - use void* for env to avoid circular dependencies */
uint64_t hexagon_tlb_read(HexagonTLBState *tlb, void *env, uint32_t index);
void hexagon_tlb_write(HexagonTLBState *tlb, void *env,
                       uint32_t index, uint64_t value);
bool hexagon_tlb_find_match(HexagonTLBState *tlb, void *env,
                            target_ulong VA, MMUAccessType access_type,
                            hwaddr *PA, int *prot, uint64_t *size,
                            int32_t *excp, int mmu_idx);
uint32_t hexagon_tlb_lookup(HexagonTLBState *tlb, void *env,
                            uint32_t ssr, uint32_t VA);
uint32_t hexagon_tlb_lookup_extended(HexagonTLBState *tlb, void *env,
                                     uint32_t ssr, uint64_t VA);
int hexagon_tlb_check_overlap(HexagonTLBState *tlb, void *env,
                              uint64_t entry, uint64_t index);
void hexagon_tlb_lock(HexagonTLBState *tlb, void *env);
void hexagon_tlb_unlock(HexagonTLBState *tlb, void *env);
void hexagon_tlb_dump(HexagonTLBState *tlb, void *env);

#endif /* HEXAGON_TLB_H */
