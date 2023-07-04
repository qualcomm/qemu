/*
 *  Copyright(c) 2019-2021 Qualcomm Innovation Center, Inc. All Rights Reserved.
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

#ifndef HEXAGON_OP_HELPER_H
#define HEXAGON_OP_HELPER_H

#include "internal.h"
#include "exec/cpu_ldst.h"
#include "exec/exec-all.h"

#ifdef CONFIG_USER_ONLY
#define CPU_MMU_INDEX(ENV) MMU_USER_IDX
#else
#define CPU_MMU_INDEX(ENV) cpu_mmu_index((ENV), false)
#endif

#define mem_read1(env, paddr) \
    (size1u_t)cpu_ldub_mmuidx_ra(env, paddr, CPU_MMU_INDEX(env), CPU_MEMOP_PC(env));
#define mem_read2(env, paddr) \
    (size2u_t)cpu_lduw_mmuidx_ra(env, paddr, CPU_MMU_INDEX(env), CPU_MEMOP_PC(env));
#define mem_read4(env, paddr) \
    (size4u_t)cpu_ldl_mmuidx_ra(env, paddr, CPU_MMU_INDEX(env), CPU_MEMOP_PC(env));
#define mem_read8(env, paddr) \
    (size8u_t)cpu_ldq_mmuidx_ra(env, paddr, CPU_MMU_INDEX(env), CPU_MEMOP_PC(env));

/* Misc functions */
void cancel_slot(CPUHexagonState *env, uint32_t slot);
void write_new_pc(CPUHexagonState *env, bool pkt_has_multi_cof,
                  target_ulong addr, target_ulong PC);

void log_reg_write(CPUHexagonState *env, int rnum,
                   target_ulong val, uint32_t slot);
void check_noshuf(CPUHexagonState *env, bool pkt_has_store_s1,
                  uint32_t slot, target_ulong vaddr, int size, uintptr_t ra);
void log_store64(CPUHexagonState *env, target_ulong addr,
                 int64_t val, int width, int slot);
void log_store32(CPUHexagonState *env, target_ulong addr,
                 target_ulong val, int width, int slot);
#endif
