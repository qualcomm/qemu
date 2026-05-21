// SPDX-License-Identifier: BSD-3-Clause-Clear
// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//
// QEMU TCG PGO Plugin — generates profiling data consumable by
// LLVM BOLT, clang PGO (via llvm-profgen), lld, and Propeller.
//
// Four tiers of increasing overhead:
//   0: Hotness (block frequency + temporal order)
//   1: Edges (branch + fall-through profiling)
//   2: Calls (call graph + indirect call targets)
//   3: Resources (device/coprocessor/interrupt tracking)

mod decoder;
mod edge_storage;
mod ffi;
mod output;
mod state;
mod tier0;
mod tier1;
mod tier2;
#[cfg(feature = "tier3")]
mod tier3;

use std::os::raw::{c_char, c_int, c_void};
use std::sync::atomic::Ordering;

use ffi::*;
use output::ProfileWriter;
use state::{PluginConfig, PluginState};

/// Global plugin state. Initialized in qemu_plugin_install, accessed
/// from callbacks via this static.
static mut PLUGIN_STATE: *mut PluginState = std::ptr::null_mut();

/// Get a reference to the global plugin state.
///
/// # Safety
/// Must only be called after qemu_plugin_install has initialized the
/// state and before plugin_exit has freed it.
pub(crate) unsafe fn plugin_state() -> &'static PluginState {
    &*PLUGIN_STATE
}

/// QEMU plugin version — must match or be compatible with the host.
/// Core build (tiers 0-2): API v2 (QEMU 9.0+).
/// Extended build with tier3 feature: API v6 (QEMU 11.0+).
#[cfg(not(feature = "tier3"))]
#[no_mangle]
pub static qemu_plugin_version: c_int = 2;
#[cfg(feature = "tier3")]
#[no_mangle]
pub static qemu_plugin_version: c_int = 6;

/// Plugin entry point. Called by QEMU when the plugin is loaded.
///
/// # Safety
/// Called by QEMU's plugin loader. `info`, `argc`, and `argv` must be
/// valid per the QEMU plugin API contract.
#[no_mangle]
pub unsafe extern "C" fn qemu_plugin_install(
    id: qemu_plugin_id_t,
    info: *const qemu_info_t,
    argc: c_int,
    argv: *mut *mut c_char,
) -> c_int {
    let info_ref = &*info;

    // Parse arguments
    let mut config = PluginConfig::default();
    for i in 0..argc as usize {
        let arg = std::ffi::CStr::from_ptr(*argv.add(i)).to_string_lossy();
        if let Some(val) = arg.strip_prefix("tier=") {
            if let Some(t) = state::parse_tier(val) {
                config.tier = t;
            } else {
                plugin_out(&format!(
                    "qemu-pgo: unknown tier '{}', expected \
                     hotness|edges|calls|resources or 0-3\n",
                    val
                ));
            }
        } else if let Some(val) = arg.strip_prefix("output=") {
            config.output_path = val.to_string();
        } else if let Some(val) = arg.strip_prefix("top_n=") {
            if let Ok(n) = val.parse::<usize>() {
                config.top_n = n;
            }
        } else if let Some(val) = arg.strip_prefix("stream=") {
            let mut b = false;
            let name = std::ffi::CString::new("stream").unwrap();
            let v = std::ffi::CString::new(val.as_bytes()).unwrap();
            qemu_plugin_bool_parse(name.as_ptr(), v.as_ptr(), &mut b);
            config.streaming = b;
        } else if let Some(val) = arg.strip_prefix("binary=") {
            config.binary_path = Some(val.to_string());
        }
    }

    let sysemu = info_ref.system_emulation;
    let api_version = info_ref.version.cur;
    let arch = std::ffi::CStr::from_ptr(info_ref.target_name)
        .to_string_lossy()
        .to_string();

    let state = Box::new(PluginState::new(id, config, sysemu, api_version, arch));
    PLUGIN_STATE = Box::into_raw(state);

    let state = plugin_state();

    plugin_out(&format!(
        "qemu-pgo: tier={} output={} top_n={} stream={} arch={}\n",
        state::tier_name(state.config.tier),
        state.config.output_path,
        state.config.top_n,
        state.config.streaming,
        state.arch,
    ));

    // Defer writer initialization to first TB translation, since
    // qemu_plugin_path_to_binary() and related functions require
    // CPU state to be initialized (crashes if called during install).
    // Writer will be lazily created in init_writer_if_needed().

    // Register callbacks
    qemu_plugin_register_vcpu_tb_trans_cb(id, vcpu_tb_trans);
    qemu_plugin_register_vcpu_init_cb(id, vcpu_init);
    qemu_plugin_register_atexit_cb(id, plugin_exit, std::ptr::null_mut());
    qemu_plugin_register_flush_cb(id, flush_cb);

    // Tier 3: register discontinuity callback (API v6+)
    #[cfg(feature = "tier3")]
    if state.config.tier >= 3 && api_version >= 6 {
        qemu_plugin_register_vcpu_discon_cb(id, QEMU_PLUGIN_DISCON_ALL, tier3::discon_cb);
    }

    0
}

/// vCPU init callback — ensure per-vCPU state is allocated.
unsafe extern "C" fn vcpu_init(_id: qemu_plugin_id_t, vcpu_index: u32) {
    let state = plugin_state();
    state.ensure_vcpu_state(vcpu_index as usize + 1);
}

/// Lazily initialize the profile writer on first TB translation.
/// This is deferred from plugin_install because functions like
/// qemu_plugin_path_to_binary() require CPU state to exist.
unsafe fn init_writer_if_needed(state: &PluginState) {
    if state
        .writer_initialized
        .load(std::sync::atomic::Ordering::Relaxed)
    {
        return;
    }

    // Use compare-exchange to ensure only one thread initializes
    if state
        .writer_initialized
        .compare_exchange(
            false,
            true,
            std::sync::atomic::Ordering::SeqCst,
            std::sync::atomic::Ordering::Relaxed,
        )
        .is_err()
    {
        return;
    }

    let binary_path = if let Some(ref p) = state.config.binary_path {
        p.clone()
    } else {
        let p = qemu_plugin_path_to_binary();
        if !p.is_null() {
            std::ffi::CStr::from_ptr(p).to_string_lossy().to_string()
        } else {
            String::new()
        }
    };

    let text_start = qemu_plugin_start_code();
    let text_end = qemu_plugin_end_code();
    let entry_addr = qemu_plugin_entry_code();

    match ProfileWriter::new(
        &state.config.output_path,
        state.config.tier,
        state.sysemu,
        state.config.streaming,
        text_start,
        text_start,
        text_end,
        entry_addr,
    ) {
        Ok(mut writer) => {
            let _ = writer.write_string_meta("binary_path", &binary_path);
            let _ = writer.write_string_meta("arch", &state.arch);
            let mode = if state.sysemu { "sysemu" } else { "linux-user" };
            let _ = writer.write_string_meta("mode", mode);
            *state.writer.lock().unwrap() = Some(writer);
        }
        Err(e) => {
            plugin_out(&format!("qemu-pgo: ERROR: failed to open output: {}\n", e));
        }
    }
}

/// TB translation callback — the main instrumentation point.
unsafe extern "C" fn vcpu_tb_trans(_id: qemu_plugin_id_t, tb: *mut qemu_plugin_tb) {
    let state = plugin_state();

    // Lazy init of the profile writer
    init_writer_if_needed(state);

    // Tier 0: always — exec count + first-seen sequence
    let meta_ptr = tier0::instrument_tb(state, tb);

    // Tier 1: edge tracking
    if state.config.tier >= 1 {
        tier1::instrument_tb(state, tb, meta_ptr);
    }

    // Tier 2: call tracking
    if state.config.tier >= 2 {
        tier2::instrument_tb(state, tb, meta_ptr);
    }

    // Tier 3: resource classification and MMIO tracking
    #[cfg(feature = "tier3")]
    if state.config.tier >= 3 {
        let coproc_flags = tier3::classify_tb(state, tb);
        let meta_mut = meta_ptr as *mut state::TbMeta;
        (*meta_mut).coproc_flags = coproc_flags;

        if state.sysemu {
            let start = qemu_plugin_tb_vaddr(tb);
            tier3::instrument_mem_cb(state, tb, start);
        }
    }
}

/// Code cache flush callback.
unsafe extern "C" fn flush_cb(_id: qemu_plugin_id_t) {
    // Scoreboards persist across flushes. TbMeta entries are looked
    // up by key on retranslation in tier0::instrument_tb.
}

/// Plugin exit callback — dump all collected data.
unsafe extern "C" fn plugin_exit(_id: qemu_plugin_id_t, _userdata: *mut c_void) {
    let state = plugin_state();
    let mut writer_guard = state.writer.lock().unwrap();
    let writer = match writer_guard.as_mut() {
        Some(w) => w,
        None => return,
    };

    // Write symbol table
    {
        let symbols = state.symbols.lock().unwrap();
        for (&id, name) in symbols.iter() {
            let _ = writer.write_symbol_entry(id, 0, name);
        }
    }

    // Write TB execution data (Tier 0+)
    {
        let tb_map = state.tb_map.lock().unwrap();
        let mut batch: Vec<(u64, u64, u16, u64, u64)> = Vec::new();

        for meta in tb_map.values() {
            let exec_u64 = qemu_plugin_u64::from_scoreboard(meta.exec_count);
            let total_exec = qemu_plugin_u64_sum(exec_u64);
            let first_seen = meta.first_seen_seq.load(Ordering::Relaxed);
            batch.push((meta.start_addr, meta.end_addr, meta.n_insns, total_exec, first_seen));
        }

        let _ = writer.write_tb_exec_batch(&batch);

        plugin_out(&format!("qemu-pgo: {} TBs profiled\n", batch.len()));
    }

    // Write edge data (Tier 1+)
    if state.config.tier >= 1 {
        // Drain and merge edges from all vCPUs (single-threaded at exit)
        let mut all_edges: Vec<(u64, u64, u64)> = Vec::new();
        unsafe {
            for acc in state.edge_accumulators.iter_mut() {
                all_edges.extend(acc.drain());
            }
        }

        let mut merged: std::collections::HashMap<(u64, u64), u64> =
            std::collections::HashMap::new();
        for (from, to, count) in &all_edges {
            *merged.entry((*from, *to)).or_insert(0) += count;
        }
        let edges: Vec<(u64, u64, u64)> = merged.into_iter().map(|((f, t), c)| (f, t, c)).collect();

        let _ = writer.write_branch_edge_batch(&edges);

        plugin_out(&format!(
            "qemu-pgo: {} branch edges recorded\n",
            edges.len()
        ));

        // Compute and write fall-through edges (merge streaming flush data)
        let flushed = state.flushed_branch_entries.lock().unwrap();
        let fall_throughs = tier1::compute_fall_throughs(state, &edges, &flushed);
        drop(flushed);
        let _ = writer.write_fall_through_batch(&fall_throughs);

        plugin_out(&format!(
            "qemu-pgo: {} fall-through edges computed\n",
            fall_throughs.len()
        ));
    }

    // Write call data (Tier 2+)
    if state.config.tier >= 2 {
        // Derive execution-weighted counts for direct calls by
        // looking up the containing TB's exec_count for each callsite.
        // Build a sorted index for O(log N) lookup per callsite.
        let dc = state.direct_calls.lock().unwrap();
        let tb_map = state.tb_map.lock().unwrap();

        // Build sorted vec of (start_addr, end_addr, exec_count)
        let mut tb_index: Vec<(u64, u64, u64)> = tb_map
            .values()
            .map(|meta| {
                let exec_u64 = qemu_plugin_u64::from_scoreboard(meta.exec_count);
                let exec = qemu_plugin_u64_sum(exec_u64);
                (meta.start_addr, meta.end_addr, exec)
            })
            .collect();
        tb_index.sort_unstable_by_key(|&(start, _, _)| start);

        let direct: Vec<(u64, u64, u64)> = dc
            .keys()
            .map(|&(cs, tgt)| {
                // Binary search: find the last TB with start_addr <= cs
                let idx = tb_index.partition_point(|&(start, _, _)| start <= cs);
                let count = if idx > 0 {
                    let (_, end_addr, exec) = tb_index[idx - 1];
                    if cs <= end_addr { exec } else { 0 }
                } else {
                    0
                };
                (cs, tgt, count)
            })
            .collect();
        drop(tb_map);
        drop(dc);
        let _ = writer.write_direct_call_batch(&direct);

        let mut indirect_merged: std::collections::HashMap<(u64, u64), u64> =
            std::collections::HashMap::new();
        // Single-threaded at exit — safe to iterate all vCPU slots
        unsafe {
            for vcpu_map in state.indirect_calls.iter_mut() {
                for (&callsite, targets) in vcpu_map.iter() {
                    for (&target, &count) in targets.iter() {
                        *indirect_merged.entry((callsite, target)).or_insert(0) += count;
                    }
                }
            }
        }
        let indirect: Vec<(u64, u64, u64)> = indirect_merged
            .into_iter()
            .map(|((cs, tgt), cnt)| (cs, tgt, cnt))
            .collect();
        let _ = writer.write_indirect_call_batch(&indirect);

        plugin_out(&format!(
            "qemu-pgo: {} direct, {} indirect call edges\n",
            direct.len(),
            indirect.len()
        ));
    }

    // Write resource data (Tier 3)
    #[cfg(feature = "tier3")]
    if state.config.tier >= 3 {
        let tb_map = state.tb_map.lock().unwrap();
        for meta in tb_map.values() {
            if meta.coproc_flags != 0 {
                let _ = writer.write_tb_resource(meta.start_addr, meta.coproc_flags, 0);
            }
        }
        drop(tb_map);

        let accesses = state.device_accesses.lock().unwrap();
        for (&tb_addr, devices) in accesses.iter() {
            for dev in devices {
                let _ = writer.write_device_access(tb_addr, dev);
            }
        }

        let events = state.discon_events.lock().unwrap();
        for (&(dtype, from, to), &count) in events.iter() {
            let _ = writer.write_discontinuity(dtype, from, to, count);
        }
    }

    let _ = writer.write_footer();
    plugin_out("qemu-pgo: profile written successfully\n");
}
