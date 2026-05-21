// SPDX-License-Identifier: BSD-3-Clause-Clear
// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//
// GCC AutoFDO text profile emitter for create_gcov --profiler=text.
//
// Format (three sections, each preceded by an entry count):
//   NUM_RANGE_ENTRIES
//   <from_hex>-<to_hex>:<count>
//   ...
//   NUM_ADDRESS_ENTRIES
//   <addr_hex>:<count>
//   ...
//   NUM_BRANCH_ENTRIES
//   <src_hex>-><dst_hex>:<count>
//   ...
//
// Consumed by:
//   create_gcov --binary=elf --profile=file --profiler=text \
//               --gcov=output.afdo -gcov_version=1
//   gcc -O2 -fauto-profile=output.afdo source.c

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
    let mut ranges: Vec<(u64, u64, u64)> = Vec::new();
    for tb in &profile.tb_execs {
        if tb.exec_count == 0 {
            continue;
        }
        let from = tb.start_addr.wrapping_sub(base);
        let to = tb.end_addr.wrapping_sub(base);
        ranges.push((from, to, tb.exec_count));
    }

    // Branch entries: taken branches only.
    let mut branches: Vec<(u64, u64, u64)> = Vec::new();
    for edge in &profile.branch_edges {
        let src = edge.from.wrapping_sub(base);
        let dst = edge.to.wrapping_sub(base);
        branches.push((src, dst, edge.count));
    }

    // Write range entries
    writeln!(w, "{}", ranges.len()).map_err(|e| format!("write: {}", e))?;
    for (from, to, count) in &ranges {
        writeln!(w, "{:x}-{:x}:{}", from, to, count)
            .map_err(|e| format!("write: {}", e))?;
    }

    // Address entries: QEMU profiles at TB granularity, not
    // per-instruction, so emit an empty section.  create_gcov
    // reconstructs block frequencies from ranges + branches.
    writeln!(w, "0").map_err(|e| format!("write: {}", e))?;

    // Write branch entries
    writeln!(w, "{}", branches.len()).map_err(|e| format!("write: {}", e))?;
    for (src, dst, count) in &branches {
        writeln!(w, "{:x}->{:x}:{}", src, dst, count)
            .map_err(|e| format!("write: {}", e))?;
    }

    w.flush().map_err(|e| format!("flush: {}", e))?;

    eprintln!(
        "gcc-autofdo: wrote {} ranges + {} branches to {}",
        ranges.len(),
        branches.len(),
        output
    );

    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::native::{EdgeRecord, FileHeader, Profile, TbExec};
    use std::collections::HashMap;
    use std::io::Read;

    fn make_profile(
        tb_execs: Vec<TbExec>,
        branch_edges: Vec<EdgeRecord>,
    ) -> Profile {
        Profile {
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
            tb_execs,
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

    /// Verify the three-section structure required by
    /// create_gcov --profiler=text.
    #[test]
    fn three_section_structure() {
        let profile = make_profile(
            vec![TbExec {
                start_addr: 0x1000,
                end_addr: 0x100c,
                n_insns: 4,
                exec_count: 50,
                first_seen_seq: 1,
            }],
            vec![EdgeRecord {
                from: 0x100c,
                to: 0x2000,
                count: 30,
            }],
        );
        let elf = ElfIndex::empty(0x1000);

        let dir = std::env::temp_dir();
        let path = dir.join("test_gcc_autofdo_structure.txt");
        let path_str = path.to_str().unwrap();

        emit(&profile, &elf, path_str).unwrap();
        let contents = read_output(path_str);
        let lines: Vec<&str> = contents.lines().collect();

        // Section 1: ranges
        assert_eq!(lines[0], "1", "range count");
        assert_eq!(lines[1], "1000-100c:50", "range entry");
        // Section 2: address (empty)
        assert_eq!(lines[2], "0", "address count");
        // Section 3: branches
        assert_eq!(lines[3], "1", "branch count");
        assert_eq!(lines[4], "100c->2000:30", "branch entry");
        // No extra lines
        assert_eq!(lines.len(), 5);

        std::fs::remove_file(path_str).ok();
    }

    /// Empty profile produces three zero-count sections.
    #[test]
    fn empty_profile() {
        let profile = make_profile(Vec::new(), Vec::new());
        let elf = ElfIndex::empty(0x1000);

        let dir = std::env::temp_dir();
        let path = dir.join("test_gcc_autofdo_empty.txt");
        let path_str = path.to_str().unwrap();

        emit(&profile, &elf, path_str).unwrap();
        let contents = read_output(path_str);
        let lines: Vec<&str> = contents.lines().collect();

        assert_eq!(lines[0], "0", "range count");
        assert_eq!(lines[1], "0", "address count");
        assert_eq!(lines[2], "0", "branch count");
        assert_eq!(lines.len(), 3);

        std::fs::remove_file(path_str).ok();
    }

    /// Multiple TBs and edges produce correct offsets and counts.
    #[test]
    fn multiple_tbs_and_edges() {
        let profile = make_profile(
            vec![
                TbExec {
                    start_addr: 0x1000,
                    end_addr: 0x100c,
                    n_insns: 4,
                    exec_count: 100,
                    first_seen_seq: 1,
                },
                TbExec {
                    start_addr: 0x2000,
                    end_addr: 0x2008,
                    n_insns: 3,
                    exec_count: 75,
                    first_seen_seq: 2,
                },
            ],
            vec![
                EdgeRecord {
                    from: 0x100c,
                    to: 0x2000,
                    count: 60,
                },
                EdgeRecord {
                    from: 0x2008,
                    to: 0x1000,
                    count: 40,
                },
            ],
        );
        let elf = ElfIndex::empty(0x1000);

        let dir = std::env::temp_dir();
        let path = dir.join("test_gcc_autofdo_multi.txt");
        let path_str = path.to_str().unwrap();

        emit(&profile, &elf, path_str).unwrap();
        let contents = read_output(path_str);
        let lines: Vec<&str> = contents.lines().collect();

        // 2 ranges, 0 addresses, 2 branches
        assert_eq!(lines[0], "2");
        assert_eq!(lines[1], "1000-100c:100");
        assert_eq!(lines[2], "2000-2008:75");
        assert_eq!(lines[3], "0"); // address section
        assert_eq!(lines[4], "2");
        assert_eq!(lines[5], "100c->2000:60");
        assert_eq!(lines[6], "2008->1000:40");

        std::fs::remove_file(path_str).ok();
    }

    /// Zero-exec TBs are skipped.
    #[test]
    fn zero_exec_skipped() {
        let profile = make_profile(
            vec![
                TbExec {
                    start_addr: 0x1000,
                    end_addr: 0x1004,
                    n_insns: 2,
                    exec_count: 0,
                    first_seen_seq: 1,
                },
                TbExec {
                    start_addr: 0x2000,
                    end_addr: 0x2004,
                    n_insns: 2,
                    exec_count: 5,
                    first_seen_seq: 2,
                },
            ],
            Vec::new(),
        );
        let elf = ElfIndex::empty(0x1000);

        let dir = std::env::temp_dir();
        let path = dir.join("test_gcc_autofdo_zero.txt");
        let path_str = path.to_str().unwrap();

        emit(&profile, &elf, path_str).unwrap();
        let contents = read_output(path_str);
        let lines: Vec<&str> = contents.lines().collect();

        assert_eq!(lines[0], "1", "only non-zero TB emitted");
        assert_eq!(lines[1], "2000-2004:5");

        std::fs::remove_file(path_str).ok();
    }
}
