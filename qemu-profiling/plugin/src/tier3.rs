// SPDX-License-Identifier: BSD-3-Clause-Clear
// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//
// Tier 3: Resources — device, coprocessor, and interrupt tracking.
//
// At translation time: classify instructions by coprocessor flags.
// At runtime: track MMIO device accesses (sysemu only) and
// discontinuity events (interrupts, exceptions, hostcalls).

use std::os::raw::c_void;

use crate::decoder;
use crate::ffi::*;
use crate::state::PluginState;

/// Classify a TB's instructions to determine coprocessor flags.
///
/// # Safety
/// Must be called with a valid tb pointer during translation.
pub unsafe fn classify_tb(state: &PluginState, tb: *mut qemu_plugin_tb) -> u32 {
    let n_insns = qemu_plugin_tb_n_insns(tb);
    let mut flags: u32 = 0;

    for i in 0..n_insns {
        let insn = qemu_plugin_tb_get_insn(tb, i);
        let mut buf = [0u8; 16];
        let len = qemu_plugin_insn_data(
            insn,
            buf.as_mut_ptr() as *mut c_void,
            buf.len(),
        );
        flags |= decoder::classify_resources(state.target_arch, &buf[..len]);
    }

    flags
}

/// Register MMIO memory callbacks on a TB's instructions (sysemu).
///
/// # Safety
/// Must be called with valid tb pointer during translation.
pub unsafe fn instrument_mem_cb(_state: &PluginState, tb: *mut qemu_plugin_tb, tb_start: u64) {
    let n_insns = qemu_plugin_tb_n_insns(tb);
    for i in 0..n_insns {
        let insn = qemu_plugin_tb_get_insn(tb, i);
        qemu_plugin_register_vcpu_mem_cb(
            insn,
            mem_access_cb,
            qemu_plugin_cb_flags::QEMU_PLUGIN_CB_NO_REGS,
            qemu_plugin_mem_rw::QEMU_PLUGIN_MEM_RW,
            tb_start as usize as *mut c_void,
        );
    }
}

/// Memory access callback — checks if access is MMIO and records
/// the device name.
unsafe extern "C" fn mem_access_cb(
    _vcpu_index: u32,
    info: qemu_plugin_meminfo_t,
    vaddr: u64,
    userdata: *mut c_void,
) {
    let state = crate::plugin_state();
    let tb_addr = userdata as usize as u64;

    let hwaddr = qemu_plugin_get_hwaddr(info, vaddr);
    if hwaddr.is_null() {
        return;
    }

    if !qemu_plugin_hwaddr_is_io(hwaddr) {
        return;
    }

    let dev_name_ptr = qemu_plugin_hwaddr_device_name(hwaddr);
    if dev_name_ptr.is_null() {
        return;
    }

    let dev_name = std::ffi::CStr::from_ptr(dev_name_ptr).to_string_lossy();
    let mut accesses = state.device_accesses.lock().unwrap();
    let entry = accesses.entry(tb_addr).or_default();
    let name_str = dev_name.to_string();
    if !entry.contains(&name_str) {
        entry.push(name_str);
    }
}

/// Discontinuity callback (API v6+). Records interrupt, exception,
/// and hostcall events.
pub unsafe extern "C" fn discon_cb(
    _id: qemu_plugin_id_t,
    _vcpu_index: u32,
    discon_type: u32,
    from_pc: u64,
    to_pc: u64,
) {
    let state = crate::plugin_state();
    let mut events = state.discon_events.lock().unwrap();
    *events.entry((discon_type, from_pc, to_pc)).or_insert(0) += 1;
}
