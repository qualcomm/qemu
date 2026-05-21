// SPDX-License-Identifier: BSD-3-Clause-Clear
// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//
// Tier 0: Hotness — block frequency and temporal ordering.
//
// At translation time: register one inline ADD op for exec count,
// and one conditional callback that fires once on first execution
// to record the first-seen sequence number.

use std::os::raw::c_void;
use std::sync::atomic::Ordering;

use crate::ffi::*;
use crate::state::{PluginState, TbKey, TbMeta};

/// Set up Tier 0 instrumentation on a newly translated TB.
///
/// Called from the vcpu_tb_trans callback. Looks up or creates
/// TbMeta, registers the inline exec count increment and the
/// conditional first-seen callback.
///
/// # Safety
/// Must be called from the tb_trans callback with a valid tb pointer.
pub unsafe fn instrument_tb(state: &PluginState, tb: *mut qemu_plugin_tb) -> *const TbMeta {
    let start_addr = qemu_plugin_tb_vaddr(tb);
    let n_insns_raw = qemu_plugin_tb_n_insns(tb);
    let n_insns = n_insns_raw as u16;

    // Compute end_addr from last instruction
    let last_insn = qemu_plugin_tb_get_insn(tb, n_insns_raw - 1);
    let last_insn_vaddr = qemu_plugin_insn_vaddr(last_insn);
    let last_insn_size = qemu_plugin_insn_size(last_insn);
    let end_addr = last_insn_vaddr;
    let fall_through_addr = last_insn_vaddr + last_insn_size as u64;

    // Get symbol from first instruction
    let first_insn = qemu_plugin_tb_get_insn(tb, 0);
    let sym_ptr = qemu_plugin_insn_symbol(first_insn);
    let sym_id = if !sym_ptr.is_null() {
        let sym_name = std::ffi::CStr::from_ptr(sym_ptr).to_string_lossy();
        state.intern_symbol(&sym_name)
    } else {
        0
    };

    let key = TbKey {
        start_addr,
        n_insns,
    };

    let mut tb_map = state.tb_map.lock().unwrap();

    // If this TB was already translated (retranslation), reuse the
    // existing scoreboard so counts accumulate correctly.
    if let Some(existing) = tb_map.get(&key) {
        let exec_u64 = qemu_plugin_u64::from_scoreboard(existing.exec_count);

        // Re-register inline ADD on the new TB translation
        qemu_plugin_register_vcpu_tb_exec_inline_per_vcpu(
            tb,
            qemu_plugin_op::QEMU_PLUGIN_INLINE_ADD_U64,
            exec_u64,
            1,
        );

        return &**existing as *const TbMeta;
    }

    // New TB — create scoreboard and metadata
    let exec_scoreboard = qemu_plugin_scoreboard_new(std::mem::size_of::<u64>());
    let exec_u64 = qemu_plugin_u64::from_scoreboard(exec_scoreboard);

    let tb_meta = Box::new(TbMeta {
        start_addr,
        end_addr,
        n_insns,
        sym_id,
        exec_count: exec_scoreboard,
        first_seen_seq: std::sync::atomic::AtomicU64::new(0),
        fall_through_addr,
        ends_with_indirect: false,
        coproc_flags: 0,
    });
    let meta_ptr = &*tb_meta as *const TbMeta;

    // Register inline ADD for exec count (+1 per execution)
    qemu_plugin_register_vcpu_tb_exec_inline_per_vcpu(
        tb,
        qemu_plugin_op::QEMU_PLUGIN_INLINE_ADD_U64,
        exec_u64,
        1,
    );

    // Register conditional callback: fires when exec_count == 1
    // (i.e., on first execution after the inline ADD makes it 1).
    // The callback records a monotonic sequence number.
    qemu_plugin_register_vcpu_tb_exec_cond_cb(
        tb,
        first_seen_cb,
        qemu_plugin_cb_flags::QEMU_PLUGIN_CB_NO_REGS,
        qemu_plugin_cond::QEMU_PLUGIN_COND_EQ,
        exec_u64,
        1,
        meta_ptr as *mut c_void,
    );

    tb_map.insert(key, tb_meta);
    meta_ptr
}

/// Conditional callback that fires once per TB (on first execution).
/// Records a monotonic first-seen sequence number.
unsafe extern "C" fn first_seen_cb(_vcpu_index: u32, userdata: *mut c_void) {
    let state = crate::plugin_state();
    let meta = &*(userdata as *const TbMeta);
    let seq = state.sequence_counter.fetch_add(1, Ordering::Relaxed);
    meta.first_seen_seq.store(seq, Ordering::Relaxed);
}
