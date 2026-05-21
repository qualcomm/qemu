// SPDX-License-Identifier: BSD-3-Clause-Clear
// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//
// lld call graph ordering file emitter.
//
// Format:
//   caller_symbol callee_symbol weight
//
// Consumed by: ld.lld --call-graph-ordering-file=file

use std::collections::HashMap;
use std::fs::File;
use std::io::{BufWriter, Write};

use crate::elf_index::ElfIndex;
use crate::native::Profile;

pub fn emit(profile: &Profile, elf: &ElfIndex, output: &str) -> Result<(), String> {
    let file = File::create(output).map_err(|e| format!("create {}: {}", output, e))?;
    let mut w = BufWriter::new(file);

    // Build call graph from:
    // 1. Direct calls (tier 2+)
    // 2. Indirect calls (tier 2+)
    // 3. Branch edges that cross function boundaries

    let mut cg: HashMap<(String, String), u64> = HashMap::new();

    // Direct calls
    for call in &profile.direct_calls {
        let caller = elf.name_at(call.callsite).to_string();
        let callee = elf.name_at(call.target).to_string();
        if caller != "[unknown]" && callee != "[unknown]" {
            *cg.entry((caller, callee)).or_insert(0) += call.count;
        }
    }

    // Indirect calls
    for call in &profile.indirect_calls {
        if call.target == 0 {
            continue; // unresolved
        }
        let caller = elf.name_at(call.callsite).to_string();
        let callee = elf.name_at(call.target).to_string();
        if caller != "[unknown]" && callee != "[unknown]" {
            *cg.entry((caller, callee)).or_insert(0) += call.count;
        }
    }

    // Branch edges crossing function boundaries
    for edge in &profile.branch_edges {
        let from_sym = elf.symbol_at(edge.from);
        let to_sym = elf.symbol_at(edge.to);
        match (from_sym, to_sym) {
            (Some(f), Some(t)) if f.name != t.name => {
                *cg.entry((f.name.clone(), t.name.clone())).or_insert(0) += edge.count;
            }
            _ => {}
        }
    }

    // Sort by weight (descending) for deterministic output
    let mut entries: Vec<((String, String), u64)> = cg.into_iter().collect();
    entries.sort_by(|a, b| b.1.cmp(&a.1));

    for ((caller, callee), weight) in &entries {
        // The format is space-delimited, so sanitize symbol names
        // that contain spaces (rare but possible with demangled C++).
        let caller_safe = caller.replace(' ', "_");
        let callee_safe = callee.replace(' ', "_");
        writeln!(w, "{} {} {}", caller_safe, callee_safe, weight)
            .map_err(|e| format!("write: {}", e))?;
    }

    w.flush().map_err(|e| format!("flush: {}", e))?;

    eprintln!(
        "cgprof: wrote {} call graph edges to {}",
        entries.len(),
        output
    );

    Ok(())
}
