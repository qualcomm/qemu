// SPDX-License-Identifier: BSD-3-Clause-Clear
// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//
// Tier 1: Edges — branch and fall-through profiling.
//
// At the last instruction of each TB, STORE last_tb_end and
// fall_through_addr into per-vCPU scoreboard. At the START of each
// TB, a conditional callback fires when the previous TB did NOT
// fall through (i.e. the stored fall_through_addr != this TB's
// start_addr). This ordering ensures the comparison sees the
// PREVIOUS TB's values, not the current one's.

use std::os::raw::c_void;

use crate::ffi::*;
use crate::state::{PluginState, VcpuState};

/// Set up Tier 1 instrumentation on a TB (in addition to Tier 0).
///
/// # Safety
/// Must be called from tb_trans with valid tb and meta pointers.
pub unsafe fn instrument_tb(
    state: &PluginState,
    tb: *mut qemu_plugin_tb,
    meta: *const crate::state::TbMeta,
) {
    let meta_ref = &*meta;
    let n_insns = qemu_plugin_tb_n_insns(tb);
    if n_insns == 0 {
        return;
    }

    let off_last_tb_end = memoffset::offset_of!(VcpuState, last_tb_end);
    let off_fall_through = memoffset::offset_of!(VcpuState, fall_through_addr);

    // === TB-level: conditional callback fires FIRST ===
    // Compares the fall_through_addr stored by the PREVIOUS TB's
    // last instruction against this TB's start_addr.
    let prev_ft_u64 = qemu_plugin_u64::with_offset(state.vcpu_scoreboard, off_fall_through);
    qemu_plugin_register_vcpu_tb_exec_cond_cb(
        tb,
        branch_taken_cb,
        qemu_plugin_cb_flags::QEMU_PLUGIN_CB_NO_REGS,
        qemu_plugin_cond::QEMU_PLUGIN_COND_NE,
        prev_ft_u64,
        meta_ref.start_addr,
        meta_ref.start_addr as usize as *mut c_void,
    );

    // === Last instruction-level: STORE ops execute AFTER ===
    // These update the scoreboard with THIS TB's values, which will
    // be visible to the NEXT TB's conditional callback.
    let last_insn = qemu_plugin_tb_get_insn(tb, n_insns - 1);

    let last_tb_end_u64 = qemu_plugin_u64::with_offset(state.vcpu_scoreboard, off_last_tb_end);
    qemu_plugin_register_vcpu_insn_exec_inline_per_vcpu(
        last_insn,
        qemu_plugin_op::QEMU_PLUGIN_INLINE_STORE_U64,
        last_tb_end_u64,
        meta_ref.end_addr,
    );

    let ft_u64 = qemu_plugin_u64::with_offset(state.vcpu_scoreboard, off_fall_through);
    qemu_plugin_register_vcpu_insn_exec_inline_per_vcpu(
        last_insn,
        qemu_plugin_op::QEMU_PLUGIN_INLINE_STORE_U64,
        ft_u64,
        meta_ref.fall_through_addr,
    );
}

/// Callback invoked when a branch is taken (previous TB did not
/// fall through to this TB). Records the (from, to) edge.
/// Also checks for a pending indirect call from tier 2 and pairs
/// the callsite with the resolved target address.
unsafe extern "C" fn branch_taken_cb(vcpu_index: u32, userdata: *mut c_void) {
    let state = crate::plugin_state();
    let to_addr = userdata as usize as u64;

    // Read last_tb_end from per-vCPU scoreboard
    let ptr = qemu_plugin_scoreboard_find(state.vcpu_scoreboard, vcpu_index) as *const VcpuState;
    let from_addr = (*ptr).last_tb_end;

    // Skip the edge from address 0 (initial state before any TB)
    if from_addr == 0 {
        return;
    }

    state.record_edge(vcpu_index, from_addr, to_addr);

    // Tier 2: resolve pending indirect call target.
    // The previous TB's indirect call instruction set
    // pending_indirect_callsite via inline STORE. We read it here
    // before the current TB's tier2 inline STORE clears it to 0.
    if state.config.tier >= 2 {
        let pending_callsite = (*ptr).pending_indirect_callsite;
        if pending_callsite != 0 {
            state.record_indirect_call(vcpu_index, pending_callsite, to_addr);
        }
    }
}

/// Compute fall-through edges at exit time.
///
/// For each TB_B: fall_through_count = total_exec - branch_entries
/// where branch_entries = sum of all branch edge counts targeting
/// TB_B.start_addr.
///
/// The `branch_edges` parameter contains already-merged edges from
/// the exit handler (so we don't need to peek into accumulators).
/// `extra_branch_entries` contains accumulated branch-entry counts
/// from streaming flushes (pass empty map in non-streaming mode).
pub fn compute_fall_throughs(
    state: &PluginState,
    branch_edges: &[(u64, u64, u64)],
    extra_branch_entries: &rustc_hash::FxHashMap<u64, u64>,
) -> Vec<(u64, u64, u64)> {
    let tb_map = state.tb_map.lock().unwrap();

    // Build map: target_addr → total branch entry count
    let mut branch_entry_counts: std::collections::HashMap<u64, u64> =
        std::collections::HashMap::new();
    // Merge in counts from streaming flushes first
    for (&addr, &count) in extra_branch_entries {
        *branch_entry_counts.entry(addr).or_insert(0) += count;
    }
    for &(_, to, count) in branch_edges {
        *branch_entry_counts.entry(to).or_insert(0) += count;
    }

    // Build map: fall_through_addr → predecessor's end_addr
    let mut predecessors: std::collections::HashMap<u64, u64> = std::collections::HashMap::new();
    for meta in tb_map.values() {
        predecessors.insert(meta.fall_through_addr, meta.end_addr);
    }

    let mut fall_throughs = Vec::new();

    for meta in tb_map.values() {
        let exec_u64 = qemu_plugin_u64::from_scoreboard(meta.exec_count);
        let total_exec = unsafe { qemu_plugin_u64_sum(exec_u64) };
        if total_exec == 0 {
            continue;
        }

        let branch_count = branch_entry_counts
            .get(&meta.start_addr)
            .copied()
            .unwrap_or(0);
        let ft_count = total_exec.saturating_sub(branch_count);

        if ft_count > 0 {
            if let Some(&pred_end) = predecessors.get(&meta.start_addr) {
                fall_throughs.push((pred_end, meta.start_addr, ft_count));
            }
        }
    }

    fall_throughs
}
