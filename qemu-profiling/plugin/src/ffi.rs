// SPDX-License-Identifier: BSD-3-Clause-Clear
// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//
// Hand-rolled Rust bindings for the QEMU plugin API (v2–v6).

#![allow(non_camel_case_types, dead_code)]

use std::os::raw::{c_char, c_int, c_void};

pub type qemu_plugin_id_t = u64;

// Opaque handles
#[repr(C)]
pub struct qemu_plugin_tb {
    _opaque: [u8; 0],
}

#[repr(C)]
pub struct qemu_plugin_insn {
    _opaque: [u8; 0],
}

#[repr(C)]
pub struct qemu_plugin_scoreboard {
    _opaque: [u8; 0],
}

#[repr(C)]
pub struct qemu_plugin_hwaddr {
    _opaque: [u8; 0],
}

// System info passed to qemu_plugin_install
#[repr(C)]
pub struct qemu_info_version {
    pub min: c_int,
    pub cur: c_int,
}

#[repr(C)]
pub struct qemu_info_system {
    pub smp_vcpus: c_int,
    pub max_vcpus: c_int,
}

#[repr(C)]
pub union qemu_info_union {
    pub system: std::mem::ManuallyDrop<qemu_info_system>,
}

#[repr(C)]
pub struct qemu_info_t {
    pub target_name: *const c_char,
    pub version: qemu_info_version,
    pub system_emulation: bool,
    pub u: qemu_info_union,
}

// Scoreboard U64 descriptor
#[repr(C)]
#[derive(Copy, Clone)]
pub struct qemu_plugin_u64 {
    pub score: *mut qemu_plugin_scoreboard,
    pub offset: usize,
}

unsafe impl Send for qemu_plugin_u64 {}
unsafe impl Sync for qemu_plugin_u64 {}

impl qemu_plugin_u64 {
    pub fn from_scoreboard(score: *mut qemu_plugin_scoreboard) -> Self {
        Self { score, offset: 0 }
    }

    pub fn with_offset(score: *mut qemu_plugin_scoreboard, offset: usize) -> Self {
        Self { score, offset }
    }
}

// Enums

#[repr(C)]
#[derive(Copy, Clone, Debug, PartialEq, Eq)]
pub enum qemu_plugin_cb_flags {
    QEMU_PLUGIN_CB_NO_REGS = 0,
    QEMU_PLUGIN_CB_R_REGS = 1,
    QEMU_PLUGIN_CB_RW_REGS = 2,
    QEMU_PLUGIN_CB_RW_REGS_PC = 3,
}

#[repr(C)]
#[derive(Copy, Clone, Debug, PartialEq, Eq)]
pub enum qemu_plugin_op {
    QEMU_PLUGIN_INLINE_ADD_U64 = 0,
    QEMU_PLUGIN_INLINE_STORE_U64 = 1,
}

#[repr(C)]
#[derive(Copy, Clone, Debug, PartialEq, Eq)]
pub enum qemu_plugin_cond {
    QEMU_PLUGIN_COND_NEVER = 0,
    QEMU_PLUGIN_COND_ALWAYS = 1,
    QEMU_PLUGIN_COND_EQ = 2,
    QEMU_PLUGIN_COND_NE = 3,
    QEMU_PLUGIN_COND_LT = 4,
    QEMU_PLUGIN_COND_LE = 5,
    QEMU_PLUGIN_COND_GT = 6,
    QEMU_PLUGIN_COND_GE = 7,
}

#[repr(C)]
#[derive(Copy, Clone, Debug, PartialEq, Eq)]
pub enum qemu_plugin_mem_rw {
    QEMU_PLUGIN_MEM_R = 1,
    QEMU_PLUGIN_MEM_W = 2,
    QEMU_PLUGIN_MEM_RW = 3,
}

pub type qemu_plugin_meminfo_t = u32;

#[cfg(feature = "tier3")]
#[repr(C)]
#[derive(Copy, Clone, Debug, PartialEq, Eq)]
pub enum qemu_plugin_discon_type {
    QEMU_PLUGIN_DISCON_INTERRUPT = 1,
    QEMU_PLUGIN_DISCON_EXCEPTION = 2,
    QEMU_PLUGIN_DISCON_HOSTCALL = 4,
}

#[cfg(feature = "tier3")]
pub const QEMU_PLUGIN_DISCON_ALL: i32 = -1;

// Callback type definitions
pub type qemu_plugin_simple_cb_t = unsafe extern "C" fn(id: qemu_plugin_id_t);

pub type qemu_plugin_udata_cb_t = unsafe extern "C" fn(id: qemu_plugin_id_t, userdata: *mut c_void);

pub type qemu_plugin_vcpu_simple_cb_t = unsafe extern "C" fn(id: qemu_plugin_id_t, vcpu_index: u32);

pub type qemu_plugin_vcpu_udata_cb_t = unsafe extern "C" fn(vcpu_index: u32, userdata: *mut c_void);

pub type qemu_plugin_vcpu_tb_trans_cb_t =
    unsafe extern "C" fn(id: qemu_plugin_id_t, tb: *mut qemu_plugin_tb);

pub type qemu_plugin_vcpu_mem_cb_t = unsafe extern "C" fn(
    vcpu_index: u32,
    info: qemu_plugin_meminfo_t,
    vaddr: u64,
    userdata: *mut c_void,
);

#[cfg(feature = "tier3")]
pub type qemu_plugin_vcpu_discon_cb_t = unsafe extern "C" fn(
    id: qemu_plugin_id_t,
    vcpu_index: u32,
    discon_type: u32,
    from_pc: u64,
    to_pc: u64,
);

unsafe impl Send for qemu_plugin_tb {}
unsafe impl Sync for qemu_plugin_tb {}
unsafe impl Send for qemu_plugin_insn {}
unsafe impl Sync for qemu_plugin_insn {}

extern "C" {
    // Plugin lifecycle
    pub fn qemu_plugin_uninstall(id: qemu_plugin_id_t, cb: qemu_plugin_simple_cb_t);
    pub fn qemu_plugin_reset(id: qemu_plugin_id_t, cb: qemu_plugin_simple_cb_t);

    // vCPU lifecycle
    pub fn qemu_plugin_register_vcpu_init_cb(
        id: qemu_plugin_id_t,
        cb: qemu_plugin_vcpu_simple_cb_t,
    );
    pub fn qemu_plugin_register_vcpu_exit_cb(
        id: qemu_plugin_id_t,
        cb: qemu_plugin_vcpu_simple_cb_t,
    );

    // Translation block translation callback
    pub fn qemu_plugin_register_vcpu_tb_trans_cb(
        id: qemu_plugin_id_t,
        cb: qemu_plugin_vcpu_tb_trans_cb_t,
    );

    // TB execution callbacks
    pub fn qemu_plugin_register_vcpu_tb_exec_cb(
        tb: *mut qemu_plugin_tb,
        cb: qemu_plugin_vcpu_udata_cb_t,
        flags: qemu_plugin_cb_flags,
        userdata: *mut c_void,
    );
    pub fn qemu_plugin_register_vcpu_tb_exec_cond_cb(
        tb: *mut qemu_plugin_tb,
        cb: qemu_plugin_vcpu_udata_cb_t,
        flags: qemu_plugin_cb_flags,
        cond: qemu_plugin_cond,
        entry: qemu_plugin_u64,
        imm: u64,
        userdata: *mut c_void,
    );
    pub fn qemu_plugin_register_vcpu_tb_exec_inline_per_vcpu(
        tb: *mut qemu_plugin_tb,
        op: qemu_plugin_op,
        entry: qemu_plugin_u64,
        imm: u64,
    );

    // Instruction execution callbacks
    pub fn qemu_plugin_register_vcpu_insn_exec_cb(
        insn: *mut qemu_plugin_insn,
        cb: qemu_plugin_vcpu_udata_cb_t,
        flags: qemu_plugin_cb_flags,
        userdata: *mut c_void,
    );
    pub fn qemu_plugin_register_vcpu_insn_exec_cond_cb(
        insn: *mut qemu_plugin_insn,
        cb: qemu_plugin_vcpu_udata_cb_t,
        flags: qemu_plugin_cb_flags,
        cond: qemu_plugin_cond,
        entry: qemu_plugin_u64,
        imm: u64,
        userdata: *mut c_void,
    );
    pub fn qemu_plugin_register_vcpu_insn_exec_inline_per_vcpu(
        insn: *mut qemu_plugin_insn,
        op: qemu_plugin_op,
        entry: qemu_plugin_u64,
        imm: u64,
    );

    // Memory callbacks
    pub fn qemu_plugin_register_vcpu_mem_cb(
        insn: *mut qemu_plugin_insn,
        cb: qemu_plugin_vcpu_mem_cb_t,
        flags: qemu_plugin_cb_flags,
        rw: qemu_plugin_mem_rw,
        userdata: *mut c_void,
    );

    // Discontinuity callbacks (API v6+)
    #[cfg(feature = "tier3")]
    pub fn qemu_plugin_register_vcpu_discon_cb(
        id: qemu_plugin_id_t,
        discon_type: c_int,
        cb: qemu_plugin_vcpu_discon_cb_t,
    );

    // Global callbacks
    pub fn qemu_plugin_register_flush_cb(id: qemu_plugin_id_t, cb: qemu_plugin_simple_cb_t);
    pub fn qemu_plugin_register_atexit_cb(
        id: qemu_plugin_id_t,
        cb: qemu_plugin_udata_cb_t,
        userdata: *mut c_void,
    );

    // Scoreboard
    pub fn qemu_plugin_scoreboard_new(element_size: usize) -> *mut qemu_plugin_scoreboard;
    pub fn qemu_plugin_scoreboard_free(score: *mut qemu_plugin_scoreboard);
    pub fn qemu_plugin_scoreboard_find(
        score: *mut qemu_plugin_scoreboard,
        vcpu_index: u32,
    ) -> *mut c_void;
    pub fn qemu_plugin_u64_add(entry: qemu_plugin_u64, vcpu_index: u32, added: u64);
    pub fn qemu_plugin_u64_get(entry: qemu_plugin_u64, vcpu_index: u32) -> u64;
    pub fn qemu_plugin_u64_set(entry: qemu_plugin_u64, vcpu_index: u32, val: u64);
    pub fn qemu_plugin_u64_sum(entry: qemu_plugin_u64) -> u64;

    // TB introspection
    pub fn qemu_plugin_tb_n_insns(tb: *const qemu_plugin_tb) -> usize;
    pub fn qemu_plugin_tb_vaddr(tb: *const qemu_plugin_tb) -> u64;
    pub fn qemu_plugin_tb_get_insn(tb: *const qemu_plugin_tb, idx: usize) -> *mut qemu_plugin_insn;

    // Instruction introspection
    pub fn qemu_plugin_insn_data(
        insn: *const qemu_plugin_insn,
        dest: *mut c_void,
        len: usize,
    ) -> usize;
    pub fn qemu_plugin_insn_size(insn: *const qemu_plugin_insn) -> usize;
    pub fn qemu_plugin_insn_vaddr(insn: *const qemu_plugin_insn) -> u64;
    pub fn qemu_plugin_insn_disas(insn: *const qemu_plugin_insn) -> *mut c_char;
    pub fn qemu_plugin_insn_symbol(insn: *const qemu_plugin_insn) -> *const c_char;

    // Memory introspection
    pub fn qemu_plugin_get_hwaddr(
        info: qemu_plugin_meminfo_t,
        vaddr: u64,
    ) -> *mut qemu_plugin_hwaddr;
    pub fn qemu_plugin_hwaddr_is_io(haddr: *const qemu_plugin_hwaddr) -> bool;
    pub fn qemu_plugin_hwaddr_device_name(haddr: *const qemu_plugin_hwaddr) -> *const c_char;
    pub fn qemu_plugin_mem_is_store(info: qemu_plugin_meminfo_t) -> bool;

    // Binary/code segment info
    pub fn qemu_plugin_path_to_binary() -> *const c_char;
    pub fn qemu_plugin_start_code() -> u64;
    pub fn qemu_plugin_end_code() -> u64;
    pub fn qemu_plugin_entry_code() -> u64;

    // Utility
    pub fn qemu_plugin_num_vcpus() -> c_int;
    pub fn qemu_plugin_outs(string: *const c_char);
    pub fn qemu_plugin_bool_parse(name: *const c_char, val: *const c_char, ret: *mut bool) -> bool;

    // libc
    pub fn free(ptr: *mut c_void);
}

/// Helper to print a message via QEMU's output mechanism.
pub fn plugin_out(msg: &str) {
    let c_msg = std::ffi::CString::new(msg).unwrap_or_default();
    unsafe {
        qemu_plugin_outs(c_msg.as_ptr());
    }
}
