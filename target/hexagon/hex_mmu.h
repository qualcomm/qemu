/*
 *  Copyright(c) 2019-2020 Qualcomm Innovation Center, Inc. All Rights Reserved.
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

#ifndef HEXAGON_MMU_H
#define HEXAGON_MMU_H

#define NUM_TLB_ENTRIES   256
struct CPUHexagonTLBContext {
    uint64_t entries[NUM_TLB_ENTRIES];
};

extern void hex_tlbw(CPUHexagonState *env, uint32_t index, uint64_t value);
extern uint32_t hex_tlb_lookup(CPUHexagonState *env, uint32_t ssr, uint32_t VA);
extern void hex_mmu_realize(CPUHexagonState *env);
extern void hex_mmu_reset(CPUHexagonState *env);
extern void hex_mmu_on(CPUHexagonState *env);
extern void hex_mmu_off(CPUHexagonState *env);
extern void hex_mmu_mode_change(CPUHexagonState *env);
extern bool hex_tlb_find_match(CPUHexagonState *env, target_ulong VA,
                               MMUAccessType access_type,
                               hwaddr *PA, int *prot, int *size,
                               int32_t *excp, int mmu_idx);
extern int hex_tlb_check_overlap(CPUHexagonState *env, uint64_t entry, uint64_t index);
extern void hex_tlb_lock(CPUHexagonState *env);
extern void hex_tlb_unlock(CPUHexagonState *env);
void dump_mmu(CPUHexagonState *env);
#endif
