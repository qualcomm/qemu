// SPDX-License-Identifier: BSD-3-Clause-Clear
// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//
// BOLT pre-aggregated profile emitter.
//
// Format:
//   E cycles
//   B <from_offset_hex> <to_offset_hex> <count> <mispreds>
//   F <from_offset_hex> <to_offset_hex> <count>

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
    // headers. BOLT's pre-aggregated format expects addresses in the
    // binary's own (static) address space, so only the actual runtime
    // relocation delta should be removed -- for a non-PIE/static binary
    // the two addresses are equal and no shift is needed.
    let base = if profile.header.load_addr != 0 {
        profile.header.load_addr.wrapping_sub(elf.load_addr)
    } else {
        0
    };

    // Event type header
    writeln!(w, "E cycles").map_err(|e| format!("write: {}", e))?;

    // Branch edges → B records
    for edge in &profile.branch_edges {
        let from_off = edge.from.wrapping_sub(base);
        let to_off = edge.to.wrapping_sub(base);
        // Misprediction count = 0 (QEMU doesn't model prediction)
        writeln!(w, "B {:x} {:x} {} 0", from_off, to_off, edge.count)
            .map_err(|e| format!("write: {}", e))?;
    }

    // Fall-through edges → F records
    for edge in &profile.fall_through_edges {
        let from_off = edge.from.wrapping_sub(base);
        let to_off = edge.to.wrapping_sub(base);
        writeln!(w, "F {:x} {:x} {}", from_off, to_off, edge.count)
            .map_err(|e| format!("write: {}", e))?;
    }

    w.flush().map_err(|e| format!("flush: {}", e))?;

    let total = profile.branch_edges.len() + profile.fall_through_edges.len();
    eprintln!(
        "bolt: wrote {} records ({} B + {} F) to {}",
        total,
        profile.branch_edges.len(),
        profile.fall_through_edges.len(),
        output
    );

    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::native::{EdgeRecord, FileHeader, Profile};
    use std::collections::HashMap;
    use std::io::Read;

    fn make_profile(load_addr: u64, branch_edges: Vec<EdgeRecord>) -> Profile {
        Profile {
            header: FileHeader {
                format_version: 1,
                tier: 1,
                pointer_size: 8,
                flags: 0,
                load_addr,
                text_start: load_addr,
                text_end: load_addr + 0x8000,
                entry_addr: load_addr,
                build_id: [0; 16],
            },
            metadata: HashMap::new(),
            symbols: Vec::new(),
            tb_execs: Vec::new(),
            branch_edges,
            fall_through_edges: Vec::new(),
            direct_calls: Vec::new(),
            indirect_calls: Vec::new(),
            tb_resources: Vec::new(),
            discon_events: Vec::new(),
            footer: None,
        }
    }

    fn read_output(path: &str) -> String {
        let mut contents = String::new();
        std::fs::File::open(path)
            .unwrap()
            .read_to_string(&mut contents)
            .unwrap();
        contents
    }

    /// Non-PIE/static binary: the runtime .text address recorded by the
    /// plugin equals the ELF's own static .text address, so no shift
    /// should be applied -- addresses pass through unchanged, matching
    /// what BOLT's `-pa` reader expects (the binary's own vaddr space).
    #[test]
    fn no_shift_when_static() {
        let profile = make_profile(
            0x20140,
            vec![EdgeRecord {
                from: 0x2066c,
                to: 0x201e0,
                count: 5,
            }],
        );
        let elf = ElfIndex::empty(0x20140);

        let dir = std::env::temp_dir();
        let path = dir.join("test_bolt_no_shift.txt");
        let path_str = path.to_str().unwrap();

        emit(&profile, &elf, path_str).unwrap();
        let contents = read_output(path_str);
        let lines: Vec<&str> = contents.lines().collect();

        assert_eq!(lines[0], "E cycles");
        assert_eq!(lines[1], "B 2066c 201e0 5 0");

        std::fs::remove_file(path_str).ok();
    }

    /// PIE binary: the runtime .text address differs from the ELF's
    /// static .text address by the ASLR relocation amount. Only that
    /// delta should be removed, recovering the binary's static vaddrs.
    #[test]
    fn shift_by_aslr_delta_when_pie() {
        let profile = make_profile(
            0x25140, // runtime .text start (static 0x20140 + 0x5000 ASLR slide)
            vec![EdgeRecord {
                from: 0x2566c,
                to: 0x251e0,
                count: 5,
            }],
        );
        let elf = ElfIndex::empty(0x20140); // static .text start from the ELF

        let dir = std::env::temp_dir();
        let path = dir.join("test_bolt_shift_by_aslr_delta.txt");
        let path_str = path.to_str().unwrap();

        emit(&profile, &elf, path_str).unwrap();
        let contents = read_output(path_str);
        let lines: Vec<&str> = contents.lines().collect();

        assert_eq!(lines[0], "E cycles");
        assert_eq!(lines[1], "B 2066c 201e0 5 0");

        std::fs::remove_file(path_str).ok();
    }
}
