// SPDX-License-Identifier: BSD-3-Clause-Clear
// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//
// Tier 2: Calls — call graph and indirect call target tracking.
//
// At translation time: classify instructions via opcode bitmask tables
// (decoder module) operating on raw instruction bytes.  No disassembly
// string allocation or text matching.
//
// Direct calls are recorded statically (pair existence only — actual
// execution count is derived from containing TB exec_count at exit).
// Indirect calls are tracked via per-vCPU scoreboard: an inline
// STORE sets pending_indirect_callsite when an indirect call/branch
// executes, and the next TB's branch_taken_cb reads it to pair
// the callsite with the actual target.

use std::os::raw::c_void;

use crate::decoder;
use crate::ffi::*;
use crate::state::{PluginState, TbMeta, VcpuState};

/// Set up Tier 2 instrumentation on a TB's instructions.
///
/// # Safety
/// Must be called from tb_trans with valid tb and meta pointers.
pub unsafe fn instrument_tb(state: &PluginState, tb: *mut qemu_plugin_tb, meta: *const TbMeta) {
    let meta_ref = &*meta;
    let n_insns = qemu_plugin_tb_n_insns(tb);

    let off_pending = memoffset::offset_of!(VcpuState, pending_indirect_callsite);
    let pending_u64 = qemu_plugin_u64::with_offset(state.vcpu_scoreboard, off_pending);

    // TB-level: clear pending_indirect_callsite at TB start.
    // This runs AFTER tier1's branch_taken_cb has read the
    // previous TB's pending callsite (registration order:
    // tier0 first, tier1 second, tier2 third within tb_trans).
    qemu_plugin_register_vcpu_tb_exec_inline_per_vcpu(
        tb,
        qemu_plugin_op::QEMU_PLUGIN_INLINE_STORE_U64,
        pending_u64,
        0,
    );

    for i in 0..n_insns {
        let insn = qemu_plugin_tb_get_insn(tb, i);
        let insn_addr = qemu_plugin_insn_vaddr(insn);

        // Read raw instruction bytes for opcode classification
        let mut buf = [0u8; 16];
        let len = qemu_plugin_insn_data(insn, buf.as_mut_ptr() as *mut c_void, buf.len());
        let insn_size = qemu_plugin_insn_size(insn);

        match decoder::classify(state.target_arch, &buf[..len], insn_addr, insn_size) {
            decoder::InsnClass::Direct { target } => {
                let mut dc = state.direct_calls.lock().unwrap();
                dc.entry((insn_addr, target)).or_insert(());
            }
            decoder::InsnClass::Indirect => {
                // Inline STORE: set pending_indirect_callsite = insn_addr
                // when this indirect call/jump executes. The next TB's
                // branch_taken_cb will read it and pair with the target.
                qemu_plugin_register_vcpu_insn_exec_inline_per_vcpu(
                    insn,
                    qemu_plugin_op::QEMU_PLUGIN_INLINE_STORE_U64,
                    pending_u64,
                    insn_addr,
                );

                // Mark the TB if this is the last instruction
                if i == n_insns - 1 {
                    let meta_mut = meta as *mut TbMeta;
                    (*meta_mut).ends_with_indirect = true;
                }
            }
            decoder::InsnClass::Other => {}
        }
    }

    let _ = meta_ref; // suppress unused warning
}
