// SPDX-License-Identifier: BSD-3-Clause-Clear
// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//
// Temporal profile trace emitter for lld startup ordering.
//
// Format (embedded in instrprof text format):
//   :temporal_prof_traces
//   # Num Temporal Profile Traces:
//   1
//   # Temporal Profile Trace Stream Size:
//   1
//   # Weight:
//   1
//   func_a,func_b,func_c,
//
// Consumed by: llvm-profdata merge → lld --irpgo-profile
//              with --bp-startup-sort=function

use std::collections::HashMap;
use std::fs::File;
use std::io::{BufWriter, Write};

use crate::elf_index::ElfIndex;
use crate::native::Profile;

pub fn emit(profile: &Profile, elf: &ElfIndex, output: &str) -> Result<(), String> {
    let file = File::create(output).map_err(|e| format!("create {}: {}", output, e))?;
    let mut w = BufWriter::new(file);

    // Group TBs by function (symbol), tracking the minimum
    // first_seen_seq per function for temporal ordering.
    let mut func_first_seen: HashMap<String, u64> = HashMap::new();

    for tb in &profile.tb_execs {
        if tb.exec_count == 0 || tb.first_seen_seq == 0 {
            continue;
        }
        let name = elf.name_at(tb.start_addr);
        if name == "[unknown]" {
            continue;
        }
        let entry = func_first_seen.entry(name.to_string()).or_insert(u64::MAX);
        if tb.first_seen_seq < *entry {
            *entry = tb.first_seen_seq;
        }
    }

    // Sort functions by first-seen order
    let mut funcs: Vec<(String, u64)> = func_first_seen.into_iter().collect();
    funcs.sort_by_key(|&(_, seq)| seq);

    // Emit temporal profile trace format.
    // If no functions were found, emit 0 traces to avoid malformed output (R14).
    let num_traces = if funcs.is_empty() { 0 } else { 1 };

    writeln!(w, ":temporal_prof_traces").map_err(|e| format!("write: {}", e))?;
    writeln!(w, "# Num Temporal Profile Traces:").map_err(|e| format!("write: {}", e))?;
    writeln!(w, "{}", num_traces).map_err(|e| format!("write: {}", e))?;
    writeln!(w, "# Temporal Profile Trace Stream Size:").map_err(|e| format!("write: {}", e))?;
    writeln!(w, "{}", num_traces).map_err(|e| format!("write: {}", e))?;

    if !funcs.is_empty() {
        writeln!(w, "# Weight:").map_err(|e| format!("write: {}", e))?;
        writeln!(w, "1").map_err(|e| format!("write: {}", e))?;

        // Function list, comma-separated, trailing comma.
        // Sanitize names that contain commas (R13) since the format
        // uses comma as delimiter.
        let names: Vec<String> = funcs
            .iter()
            .map(|(n, _)| n.replace(',', "_"))
            .collect();
        let trace_line = names.join(",") + ",";
        writeln!(w, "{}", trace_line).map_err(|e| format!("write: {}", e))?;
    }

    w.flush().map_err(|e| format!("flush: {}", e))?;

    eprintln!(
        "temporal: wrote {} functions in execution order to {}",
        funcs.len(),
        output
    );

    Ok(())
}
