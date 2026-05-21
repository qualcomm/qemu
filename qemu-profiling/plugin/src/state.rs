// SPDX-License-Identifier: BSD-3-Clause-Clear
// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//
// Global plugin state and TB metadata definitions.

use std::collections::HashMap;
use std::sync::atomic::{AtomicBool, AtomicPtr, AtomicU64, AtomicUsize, Ordering};
use std::sync::Mutex;

use rustc_hash::FxHashMap;

use crate::decoder;
use crate::edge_storage::EdgeAccumulator;
use crate::ffi::*;
use crate::output::ProfileWriter;

/// Lock-free per-vCPU storage. Uses atomic pointers to heap-allocated
/// slots so the hot path (per-vCPU access by index) never acquires a
/// lock. Growth is serialized by the caller (ensure_vcpu_state).
///
/// # Safety contract
/// - Each index must be initialized (via `push`) before `get_mut`
/// - `get_mut(i)` may only be called from callbacks for vCPU `i`
///   (QEMU serializes per-vCPU callbacks)
pub struct PerVcpuStore<T> {
    ptrs: Box<[AtomicPtr<T>]>,
    len: AtomicUsize,
}

unsafe impl<T> Send for PerVcpuStore<T> {}
unsafe impl<T> Sync for PerVcpuStore<T> {}

const MAX_VCPUS: usize = 256;

impl<T> PerVcpuStore<T> {
    fn new() -> Self {
        let mut ptrs = Vec::with_capacity(MAX_VCPUS);
        for _ in 0..MAX_VCPUS {
            ptrs.push(AtomicPtr::new(std::ptr::null_mut()));
        }
        Self {
            ptrs: ptrs.into_boxed_slice(),
            len: AtomicUsize::new(0),
        }
    }

    fn push(&self, val: T) {
        // Use fetch_add to atomically claim a slot index, preventing
        // races when multiple vCPUs initialize concurrently.
        let idx = self.len.fetch_add(1, Ordering::AcqRel);
        assert!(idx < MAX_VCPUS, "too many vCPUs");
        let ptr = Box::into_raw(Box::new(val));
        self.ptrs[idx].store(ptr, Ordering::Release);
    }

    fn len(&self) -> usize {
        self.len.load(Ordering::Acquire)
    }

    /// Get a mutable reference to the per-vCPU slot. Lock-free.
    ///
    /// # Safety
    /// Caller must ensure exclusive access for this index (guaranteed
    /// by QEMU's per-vCPU callback serialization).
    pub unsafe fn get_mut(&self, idx: usize) -> Option<&mut T> {
        if idx >= self.len() {
            return None;
        }
        let ptr = self.ptrs[idx].load(Ordering::Acquire);
        if ptr.is_null() {
            None
        } else {
            Some(&mut *ptr)
        }
    }

    /// Iterate over all initialized slots mutably.
    ///
    /// # Safety
    /// Must only be called when no per-vCPU callbacks are running
    /// (e.g., at exit time).
    pub unsafe fn iter_mut(&self) -> impl Iterator<Item = &mut T> {
        let len = self.len();
        (0..len).filter_map(move |i| {
            let ptr = self.ptrs[i].load(Ordering::Acquire);
            if ptr.is_null() {
                None
            } else {
                Some(&mut *ptr)
            }
        })
    }
}

impl<T> Drop for PerVcpuStore<T> {
    fn drop(&mut self) {
        let len = self.len.load(Ordering::Acquire);
        for i in 0..len {
            let ptr = self.ptrs[i].load(Ordering::Acquire);
            if !ptr.is_null() {
                unsafe { drop(Box::from_raw(ptr)); }
            }
        }
    }
}

/// Per-TB metadata collected at translation time.
pub struct TbMeta {
    pub start_addr: u64,
    pub end_addr: u64,
    pub n_insns: u16,
    pub sym_id: u16,
    /// Scoreboard for per-vCPU exec counts (inline-updated).
    pub exec_count: *mut qemu_plugin_scoreboard,
    /// Set once on first execution via conditional callback.
    pub first_seen_seq: AtomicU64,
    /// Fall-through address (start of next sequential TB).
    pub fall_through_addr: u64,
    /// Whether this TB ends with an indirect call/branch (tier 2).
    pub ends_with_indirect: bool,
    /// Coprocessor flags bitmask (tier 3).
    pub coproc_flags: u32,
}

/// Key for looking up TBs — a TB is uniquely identified by its start
/// address and instruction count (QEMU may retranslate at different
/// sizes).
#[derive(Hash, Eq, PartialEq, Clone, Copy)]
pub struct TbKey {
    pub start_addr: u64,
    pub n_insns: u16,
}

/// Per-vCPU scoreboard state. Stored in a QEMU scoreboard so that
/// inline ops can update it directly without callbacks.
#[repr(C)]
pub struct VcpuState {
    /// End address of the last-executed TB.
    pub last_tb_end: u64,
    /// Expected next-sequential address after the last TB.
    pub fall_through_addr: u64,
    /// Callsite address of a pending indirect call/jump from the
    /// previous TB. Set by tier2 inline STORE, read + consumed by
    /// tier1 branch_taken_cb.
    pub pending_indirect_callsite: u64,
}

/// Human-readable tier name for a given tier number.
pub fn tier_name(tier: u8) -> &'static str {
    match tier {
        0 => "hotness",
        1 => "edges",
        2 => "calls",
        3 => "resources",
        _ => "unknown",
    }
}

/// Parse a tier name or number string into a tier number.
/// Returns None if the string is not recognized.
///
/// # Examples
/// ```
/// assert_eq!(parse_tier("hotness"), Some(0));
/// assert_eq!(parse_tier("edges"), Some(1));
/// assert_eq!(parse_tier("calls"), Some(2));
/// assert_eq!(parse_tier("resources"), Some(3));
/// assert_eq!(parse_tier("2"), Some(2));
/// assert_eq!(parse_tier("garbage"), None);
/// ```
pub fn parse_tier(s: &str) -> Option<u8> {
    match s {
        "hotness" | "0" => Some(0),
        "edges" | "1" => Some(1),
        "calls" | "2" => Some(2),
        "resources" | "3" => Some(3),
        _ => None,
    }
}

/// Configuration parsed from plugin arguments.
pub struct PluginConfig {
    pub tier: u8,
    pub output_path: String,
    pub top_n: usize,
    pub streaming: bool,
    pub binary_path: Option<String>,
}

impl Default for PluginConfig {
    fn default() -> Self {
        Self {
            tier: 1,
            output_path: "tcg_prof.prof".to_string(),
            top_n: 65536,
            streaming: false,
            binary_path: None,
        }
    }
}

/// Global plugin state. Accessed from callbacks via a static pointer.
pub struct PluginState {
    pub id: qemu_plugin_id_t,
    pub config: PluginConfig,
    pub sysemu: bool,
    pub api_version: i32,
    pub arch: String,
    /// Resolved target architecture for opcode-based classification.
    pub target_arch: decoder::Arch,

    /// Map of all known TBs. Protected by mutex since translation can
    /// happen concurrently with execution callbacks.
    pub tb_map: Mutex<HashMap<TbKey, Box<TbMeta>>>,

    /// Monotonic counter for first-seen sequence assignment.
    pub sequence_counter: AtomicU64,

    /// Symbol table: id → name.
    pub symbols: Mutex<HashMap<u16, String>>,
    /// Reverse symbol lookup: name → id.
    pub symbol_ids: Mutex<HashMap<String, u16>>,
    /// Next symbol ID to assign.
    pub next_sym_id: AtomicU64,

    /// Per-vCPU scoreboard for VcpuState (tier 1+).
    pub vcpu_scoreboard: *mut qemu_plugin_scoreboard,

    /// Per-vCPU edge accumulators (tier 1+). Lock-free access.
    pub edge_accumulators: PerVcpuStore<EdgeAccumulator>,

    /// Per-vCPU indirect call maps (tier 2+). Lock-free access.
    /// Maps callsite_addr → FxHashMap<target_addr, count>.
    pub indirect_calls: PerVcpuStore<FxHashMap<u64, FxHashMap<u64, u64>>>,

    /// Direct calls discovered at translation time (tier 2+).
    /// (callsite_addr, target_addr) → (). Actual count derived from
    /// containing TB's exec_count at exit time.
    pub direct_calls: Mutex<FxHashMap<(u64, u64), ()>>,

    /// Discontinuity events (tier 3).
    /// (type, from_pc, to_pc) → count.
    #[cfg(feature = "tier3")]
    pub discon_events: Mutex<HashMap<(u32, u64, u64), u64>>,

    /// Device accesses (tier 3). tb_addr → set of device names.
    #[cfg(feature = "tier3")]
    pub device_accesses: Mutex<HashMap<u64, Vec<String>>>,

    /// Accumulated branch-entry counts from streaming flushes (tier 1+).
    /// target_addr → total count across all flushed batches.
    pub flushed_branch_entries: Mutex<FxHashMap<u64, u64>>,

    /// Profile output writer.
    pub writer: Mutex<Option<ProfileWriter>>,

    /// Whether the writer has been initialized (deferred init).
    pub writer_initialized: AtomicBool,
}

unsafe impl Send for PluginState {}
unsafe impl Sync for PluginState {}

impl PluginState {
    pub fn new(
        id: qemu_plugin_id_t,
        config: PluginConfig,
        sysemu: bool,
        api_version: i32,
        arch: String,
    ) -> Self {
        let vcpu_scoreboard = if config.tier >= 1 {
            unsafe { qemu_plugin_scoreboard_new(std::mem::size_of::<VcpuState>()) }
        } else {
            std::ptr::null_mut()
        };

        let target_arch = decoder::Arch::from_name(&arch);

        Self {
            id,
            config,
            sysemu,
            api_version,
            arch,
            target_arch,
            tb_map: Mutex::new(HashMap::new()),
            sequence_counter: AtomicU64::new(1),
            symbols: Mutex::new(HashMap::new()),
            symbol_ids: Mutex::new(HashMap::new()),
            next_sym_id: AtomicU64::new(1),
            vcpu_scoreboard,
            edge_accumulators: PerVcpuStore::new(),
            indirect_calls: PerVcpuStore::new(),
            direct_calls: Mutex::new(FxHashMap::default()),
            #[cfg(feature = "tier3")]
            discon_events: Mutex::new(HashMap::new()),
            #[cfg(feature = "tier3")]
            device_accesses: Mutex::new(HashMap::new()),
            flushed_branch_entries: Mutex::new(FxHashMap::default()),
            writer: Mutex::new(None),
            writer_initialized: AtomicBool::new(false),
        }
    }

    /// Look up or insert a symbol, returning its ID.
    /// Returns 0 if the symbol table is full (> 65534 unique symbols).
    pub fn intern_symbol(&self, name: &str) -> u16 {
        let mut ids = self.symbol_ids.lock().unwrap();
        if let Some(&id) = ids.get(name) {
            return id;
        }
        let raw_id = self.next_sym_id.fetch_add(1, Ordering::Relaxed);
        if raw_id > u16::MAX as u64 {
            // Table full — return 0 (reserved "unknown" ID)
            return 0;
        }
        let id = raw_id as u16;
        ids.insert(name.to_string(), id);
        self.symbols.lock().unwrap().insert(id, name.to_string());
        id
    }

    /// Ensure edge accumulators exist for the given vCPU count.
    /// Safe to call concurrently — PerVcpuStore::push uses fetch_add
    /// so concurrent pushes claim distinct slots.
    pub fn ensure_vcpu_state(&self, vcpu_count: usize) {
        // Load once to minimize extra pushes from concurrent callers.
        // Extra entries are harmless (unused slots) but wasteful.
        if self.edge_accumulators.len() < vcpu_count {
            while self.edge_accumulators.len() < vcpu_count {
                self.edge_accumulators.push(EdgeAccumulator::new(
                    self.config.top_n,
                    self.config.streaming,
                ));
            }
        }

        if self.config.tier >= 2 && self.indirect_calls.len() < vcpu_count {
            while self.indirect_calls.len() < vcpu_count {
                self.indirect_calls.push(FxHashMap::default());
            }
        }
    }

    /// Record a branch edge for a given vCPU. Lock-free hot path.
    /// In streaming mode, triggers a flush at the top_n threshold.
    ///
    /// # Safety
    /// Must only be called from callbacks for the given vcpu_index.
    pub fn record_edge(&self, vcpu_index: u32, from: u64, to: u64) {
        let acc = unsafe { self.edge_accumulators.get_mut(vcpu_index as usize) };
        if let Some(acc) = acc {
            acc.record(from, to);

            if self.config.streaming && acc.should_flush(self.config.top_n) {
                let edges = acc.drain();
                self.flush_edges(vcpu_index, &edges);
            }
        }
    }

    /// Flush a batch of edges to the profile writer (streaming mode).
    /// Accumulates branch-entry counts for fall-through computation.
    fn flush_edges(&self, vcpu_index: u32, edges: &[(u64, u64, u64)]) {
        // Accumulate target → count into flushed_branch_entries
        {
            let mut flushed = self.flushed_branch_entries.lock().unwrap();
            for &(_, to, count) in edges {
                *flushed.entry(to).or_insert(0) += count;
            }
        }

        // Write to profile
        let mut writer_guard = self.writer.lock().unwrap();
        if let Some(ref mut writer) = *writer_guard {
            if let Err(e) = writer.write_branch_edge_batch(edges) {
                eprintln!("tcg_prof: flush edge batch error: {}", e);
            }
            if let Err(e) = writer.write_flush_marker(vcpu_index, edges.len() as u32) {
                eprintln!("tcg_prof: flush marker error: {}", e);
            }
        }
    }

    /// Record an indirect call target for a given vCPU. Lock-free.
    ///
    /// # Safety
    /// Must only be called from callbacks for the given vcpu_index.
    pub fn record_indirect_call(&self, vcpu_index: u32, callsite: u64, target: u64) {
        let vcpu_map = unsafe { self.indirect_calls.get_mut(vcpu_index as usize) };
        if let Some(vcpu_map) = vcpu_map {
            *vcpu_map
                .entry(callsite)
                .or_default()
                .entry(target)
                .or_insert(0) += 1;
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn tier_name_all_tiers() {
        assert_eq!(tier_name(0), "hotness");
        assert_eq!(tier_name(1), "edges");
        assert_eq!(tier_name(2), "calls");
        assert_eq!(tier_name(3), "resources");
        assert_eq!(tier_name(4), "unknown");
        assert_eq!(tier_name(255), "unknown");
    }

    #[test]
    fn parse_tier_by_name() {
        assert_eq!(parse_tier("hotness"), Some(0));
        assert_eq!(parse_tier("edges"), Some(1));
        assert_eq!(parse_tier("calls"), Some(2));
        assert_eq!(parse_tier("resources"), Some(3));
    }

    #[test]
    fn parse_tier_by_number() {
        assert_eq!(parse_tier("0"), Some(0));
        assert_eq!(parse_tier("1"), Some(1));
        assert_eq!(parse_tier("2"), Some(2));
        assert_eq!(parse_tier("3"), Some(3));
    }

    #[test]
    fn parse_tier_invalid() {
        assert_eq!(parse_tier(""), None);
        assert_eq!(parse_tier("4"), None);
        assert_eq!(parse_tier("garbage"), None);
        assert_eq!(parse_tier("EDGES"), None);
    }

    #[test]
    fn plugin_config_defaults() {
        let cfg = PluginConfig::default();
        assert_eq!(cfg.tier, 1);
        assert_eq!(cfg.output_path, "tcg_prof.prof");
        assert_eq!(cfg.top_n, 65536);
        assert!(!cfg.streaming);
        assert!(cfg.binary_path.is_none());
    }
}
