// SPDX-License-Identifier: BSD-3-Clause-Clear
// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//
// llvm-profgen unsymbolized profile emitter.
//
// Format:
//   NUM_RANGE_ENTRIES
//   <from_hex>-<to_hex>:<count>
//   ...
//   NUM_BRANCH_ENTRIES
//   <src_hex>-><dst_hex>:<count>
//   ...
//
// Consumed by: llvm-profgen --unsymbolized-profile=file --binary=elf

use std::fs::File;
use std::io::{BufWriter, Write};

use crate::elf_index::ElfIndex;
use crate::native::Profile;

pub fn emit(profile: &Profile, elf: &ElfIndex, output: &str) -> Result<(), String> {
    let file = File::create(output).map_err(|e| format!("create {}: {}", output, e))?;
    let mut w = BufWriter::new(file);

    // profile.header.load_addr is the runtime address where .text was
    // mapped (possibly shifted by ASLR for a PIE binary); elf.load_addr
    // is the same location as recorded statically in the ELF's program
    // headers. Only the actual runtime relocation delta should be
    // removed -- for a non-PIE/static binary the two addresses are
    // equal and no shift is needed.
    let base = if profile.header.load_addr != 0 {
        profile.header.load_addr.wrapping_sub(elf.load_addr)
    } else {
        0
    };

    // Range entries: one per TB, weighted by exec_count.
    // Each range covers [start_addr, end_addr] of the TB.
    let mut ranges: Vec<(u64, u64, u64)> = Vec::new();
    for tb in &profile.tb_execs {
        if tb.exec_count == 0 {
            continue;
        }
        let from = tb.start_addr.wrapping_sub(base);
        let to = tb.end_addr.wrapping_sub(base);
        ranges.push((from, to, tb.exec_count));
    }

    // Branch entries: taken branches only (fall-throughs are
    // implicit in the range entries above; llvm-profgen infers
    // sequential execution between branch targets).
    let mut branches: Vec<(u64, u64, u64)> = Vec::new();
    for edge in &profile.branch_edges {
        let src = edge.from.wrapping_sub(base);
        let dst = edge.to.wrapping_sub(base);
        branches.push((src, dst, edge.count));
    }

    // Write range entries
    writeln!(w, "{}", ranges.len()).map_err(|e| format!("write: {}", e))?;
    for (from, to, count) in &ranges {
        writeln!(w, "{:x}-{:x}:{}", from, to, count).map_err(|e| format!("write: {}", e))?;
    }

    // Write branch entries
    writeln!(w, "{}", branches.len()).map_err(|e| format!("write: {}", e))?;
    for (src, dst, count) in &branches {
        writeln!(w, "{:x}->{:x}:{}", src, dst, count).map_err(|e| format!("write: {}", e))?;
    }

    w.flush().map_err(|e| format!("flush: {}", e))?;

    eprintln!(
        "autofdo: wrote {} ranges + {} branches to {}",
        ranges.len(),
        branches.len(),
        output
    );

    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::native::{FileHeader, Profile, TbExec};
    use std::collections::HashMap;
    use std::io::Read;

    /// Verify that the emitter uses the exact end_addr from the
    /// profile rather than approximating it as
    /// start + (n_insns - 1) * 4.  This matters for variable-width
    /// ISAs (x86, ARM Thumb, compressed RISC-V).
    #[test]
    fn end_addr_not_approximated() {
        // Simulate compressed RISC-V: 4 insns of 2 bytes each.
        // start=0x1000, end_addr=0x1006 (last insn at 0x1006).
        // The old 4-byte approximation would give 0x100c.
        let profile = Profile {
            header: FileHeader {
                format_version: 1,
                tier: 1,
                pointer_size: 8,
                flags: 0,
                load_addr: 0x1000,
                text_start: 0x1000,
                text_end: 0x9000,
                entry_addr: 0x1000,
                build_id: [0; 16],
            },
            metadata: HashMap::new(),
            symbols: Vec::new(),
            tb_execs: vec![TbExec {
                start_addr: 0x1000,
                end_addr: 0x1006,
                n_insns: 4,
                exec_count: 10,
                first_seen_seq: 1,
            }],
            branch_edges: Vec::new(),
            fall_through_edges: Vec::new(),
            direct_calls: Vec::new(),
            indirect_calls: Vec::new(),
            tb_resources: Vec::new(),
            discon_events: Vec::new(),
            footer: None,
        };

        let elf = ElfIndex::empty(0x1000);

        let dir = std::env::temp_dir();
        let path = dir.join("test_autofdo_end_addr.txt");
        let path_str = path.to_str().unwrap();

        emit(&profile, &elf, path_str).unwrap();

        let mut contents = String::new();
        std::fs::File::open(path_str)
            .unwrap()
            .read_to_string(&mut contents)
            .unwrap();

        // The range line should use the exact end_addr 0x1006,
        // not 0x100c (which the old (n_insns-1)*4 approximation
        // would produce). load_addr == elf.load_addr here (no ASLR
        // relocation), so addresses pass through unshifted.
        let lines: Vec<&str> = contents.lines().collect();
        // First line is the range count, second is the range entry
        assert_eq!(lines[0], "1");
        assert_eq!(lines[1], "1000-1006:10");

        std::fs::remove_file(path_str).ok();
    }
}
