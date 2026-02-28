/*
 * Copyright(c) 2023-2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "migration/vmstate.h"
#include "cpu.h"
#include "hex_mmu.h"

const VMStateDescription vmstate_hexagon_cpu = {
    .name = "cpu",
    .version_id = 0,
    .minimum_version_id = 0,
    .fields = (VMStateField[]) {
        VMSTATE_STRUCT(parent_obj, HexagonCPU, 0, vmstate_cpu_common, CPUState),
        VMSTATE_UINT32_ARRAY(env.gpr, HexagonCPU, TOTAL_PER_THREAD_REGS),
        VMSTATE_UINT32_ARRAY(env.pred, HexagonCPU, NUM_PREGS),
        VMSTATE_UINT32_ARRAY(env.t_sreg, HexagonCPU, NUM_SREGS),
        VMSTATE_UINT32_ARRAY(env.greg, HexagonCPU, NUM_GREGS),
        VMSTATE_UINT32(env.next_PC, HexagonCPU),
        VMSTATE_UINT32(env.tlb_lock_state, HexagonCPU),
        VMSTATE_UINT32(env.k0_lock_state, HexagonCPU),
        VMSTATE_UINT32(env.tlb_lock_count, HexagonCPU),
        VMSTATE_UINT32(env.k0_lock_count, HexagonCPU),
        VMSTATE_UINT32(env.threadId, HexagonCPU),
        VMSTATE_UINT32(env.cause_code, HexagonCPU),
        VMSTATE_UINT32(env.wait_next_pc, HexagonCPU),
        /* TLB state is now handled by the hexagon_tlb device */
        VMSTATE_UINT64(env.t_cycle_count, HexagonCPU),
        VMSTATE_UINT32(env.hvx_vec_len, HexagonCPU),

        VMSTATE_END_OF_LIST()
    },
};

