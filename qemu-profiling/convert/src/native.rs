// SPDX-License-Identifier: BSD-3-Clause-Clear
// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//
// Native PGO profile format parser. Reads the TLV append-only log.

use std::collections::HashMap;
use std::fs::File;
use std::io::{BufReader, Read};

/// Magic number: "QEMU_PGO" in ASCII.
pub const FILE_MAGIC: u64 = 0x4F47505F554D4551;

pub const FLAG_SYSEMU: u32 = 0x1;
pub const FLAG_STREAMING: u32 = 0x2;

/// Parsed file header.
#[derive(Debug)]
pub struct FileHeader {
    pub format_version: u16,
    pub tier: u8,
    pub pointer_size: u8,
    pub flags: u32,
    pub load_addr: u64,
    pub text_start: u64,
    pub text_end: u64,
    pub entry_addr: u64,
    pub build_id: [u8; 16],
}

/// TB execution data.
#[derive(Debug, Clone)]
pub struct TbExec {
    pub start_addr: u64,
    pub end_addr: u64,
    pub n_insns: u16,
    pub exec_count: u64,
    pub first_seen_seq: u64,
}

/// Edge record (branch or fall-through).
#[derive(Debug, Clone)]
pub struct EdgeRecord {
    pub from: u64,
    pub to: u64,
    pub count: u64,
}

/// Call record (direct or indirect).
#[derive(Debug, Clone)]
pub struct CallRecord {
    pub callsite: u64,
    pub target: u64,
    pub count: u64,
}

/// Symbol entry.
#[derive(Debug, Clone)]
pub struct SymbolRecord {
    pub sym_id: u16,
    pub addr: u64,
    pub name: String,
}

/// TB resource annotation.
#[derive(Debug, Clone)]
pub struct TbResourceRecord {
    pub tb_addr: u64,
    pub coproc_flags: u32,
    pub vcpu_mask: u64,
}

/// Discontinuity event.
#[derive(Debug, Clone)]
pub struct DisconRecord {
    pub discon_type: u32,
    pub from_pc: u64,
    pub to_pc: u64,
    pub count: u64,
}

/// Footer statistics.
#[derive(Debug)]
pub struct Footer {
    pub total_tb_count: u64,
    pub total_edge_records: u64,
    pub total_exec_count: u64,
    pub wall_time_ns: u64,
    pub clean_exit: bool,
}

/// Complete parsed profile.
#[derive(Debug)]
pub struct Profile {
    pub header: FileHeader,
    pub metadata: HashMap<String, String>,
    pub symbols: Vec<SymbolRecord>,
    pub tb_execs: Vec<TbExec>,
    pub branch_edges: Vec<EdgeRecord>,
    pub fall_through_edges: Vec<EdgeRecord>,
    pub direct_calls: Vec<CallRecord>,
    pub indirect_calls: Vec<CallRecord>,
    pub tb_resources: Vec<TbResourceRecord>,
    pub discon_events: Vec<DisconRecord>,
    pub footer: Option<Footer>,
}

impl Profile {
    /// Get all edges (branch + fall-through), merging duplicates.
    pub fn merged_edges(&self) -> HashMap<(u64, u64), u64> {
        let mut merged = HashMap::new();
        for e in &self.branch_edges {
            *merged.entry((e.from, e.to)).or_insert(0) += e.count;
        }
        for e in &self.fall_through_edges {
            *merged.entry((e.from, e.to)).or_insert(0) += e.count;
        }
        merged
    }

    /// Find symbol name for an address.
    pub fn symbol_for_id(&self, sym_id: u16) -> Option<&str> {
        self.symbols
            .iter()
            .find(|s| s.sym_id == sym_id)
            .map(|s| s.name.as_str())
    }
}

/// Read helper: read exactly n bytes.
fn read_exact(reader: &mut BufReader<File>, n: usize) -> Option<Vec<u8>> {
    let mut buf = vec![0u8; n];
    reader.read_exact(&mut buf).ok()?;
    Some(buf)
}

fn read_u8(reader: &mut BufReader<File>) -> Option<u8> {
    let buf = read_exact(reader, 1)?;
    Some(buf[0])
}

fn read_u16_le(reader: &mut BufReader<File>) -> Option<u16> {
    let buf = read_exact(reader, 2)?;
    Some(u16::from_le_bytes([buf[0], buf[1]]))
}

fn read_u32_le(reader: &mut BufReader<File>) -> Option<u32> {
    let buf = read_exact(reader, 4)?;
    Some(u32::from_le_bytes([buf[0], buf[1], buf[2], buf[3]]))
}

fn read_u64_le(reader: &mut BufReader<File>) -> Option<u64> {
    let buf = read_exact(reader, 8)?;
    Some(u64::from_le_bytes(buf.try_into().unwrap()))
}

/// Parse the native profile format from a file.
pub fn parse(path: &str) -> Result<Profile, String> {
    let file = File::open(path).map_err(|e| format!("open: {}", e))?;
    let mut reader = BufReader::new(file);

    // Read file header (64 bytes)
    let magic = read_u64_le(&mut reader).ok_or("truncated header: magic")?;
    if magic != FILE_MAGIC {
        return Err(format!(
            "bad magic: expected 0x{:016X}, got 0x{:016X}",
            FILE_MAGIC, magic
        ));
    }

    let format_version = read_u16_le(&mut reader).ok_or("truncated header: version")?;
    if format_version != 1 {
        return Err(format!(
            "unsupported format version: {} (expected 1)",
            format_version
        ));
    }
    let tier = read_u8(&mut reader).ok_or("truncated header: tier")?;
    let pointer_size = read_u8(&mut reader).ok_or("truncated header: ptr_size")?;
    if pointer_size != 4 && pointer_size != 8 {
        return Err(format!(
            "invalid pointer_size: {} (expected 4 or 8)",
            pointer_size
        ));
    }
    let flags = read_u32_le(&mut reader).ok_or("truncated header: flags")?;
    let load_addr = read_u64_le(&mut reader).ok_or("truncated header: load_addr")?;
    let text_start = read_u64_le(&mut reader).ok_or("truncated header: text_start")?;
    let text_end = read_u64_le(&mut reader).ok_or("truncated header: text_end")?;
    let entry_addr = read_u64_le(&mut reader).ok_or("truncated header: entry")?;
    let build_id_bytes = read_exact(&mut reader, 16).ok_or("truncated header: build_id")?;
    let mut build_id = [0u8; 16];
    build_id.copy_from_slice(&build_id_bytes);

    let header = FileHeader {
        format_version,
        tier,
        pointer_size,
        flags,
        load_addr,
        text_start,
        text_end,
        entry_addr,
        build_id,
    };

    let mut profile = Profile {
        header,
        metadata: HashMap::new(),
        symbols: Vec::new(),
        tb_execs: Vec::new(),
        branch_edges: Vec::new(),
        fall_through_edges: Vec::new(),
        direct_calls: Vec::new(),
        indirect_calls: Vec::new(),
        tb_resources: Vec::new(),
        discon_events: Vec::new(),
        footer: None,
    };

    // Read TLV records until EOF
    while let Some(tag) = read_u8(&mut reader) {
        let len_bytes = match read_exact(&mut reader, 3) {
            Some(b) => b,
            None => break, // truncated
        };
        let payload_len =
            u32::from_le_bytes([len_bytes[0], len_bytes[1], len_bytes[2], 0]) as usize;

        let payload = match read_exact(&mut reader, payload_len) {
            Some(p) => p,
            None => break, // truncated record
        };

        match tag {
            0x01 => parse_string_meta(&payload, &mut profile),
            0x02 => parse_symbol_entry(&payload, &mut profile),
            0x13 => parse_tb_exec_batch(&payload, &mut profile),
            0x21 => parse_branch_edge_batch(&payload, &mut profile),
            0x23 => parse_fall_through_batch(&payload, &mut profile),
            0x32 => parse_direct_call_batch(&payload, &mut profile),
            0x33 => parse_indirect_call_batch(&payload, &mut profile),
            0x40 => parse_tb_resource(&payload, &mut profile),
            0x42 => parse_discontinuity(&payload, &mut profile),
            0xFF => parse_footer(&payload, &mut profile),
            _ => {} // skip unknown tags
        }
    }

    // Merge duplicate edges that can arise from streaming mode
    // flushes writing the same edge across multiple TLV records.
    merge_edges(&mut profile.branch_edges);
    merge_edges(&mut profile.fall_through_edges);

    Ok(profile)
}

/// Merge duplicate edges by summing their counts.
fn merge_edges(edges: &mut Vec<EdgeRecord>) {
    if edges.len() <= 1 {
        return;
    }
    let mut merged: HashMap<(u64, u64), u64> = HashMap::new();
    for e in edges.iter() {
        *merged.entry((e.from, e.to)).or_insert(0) += e.count;
    }
    if merged.len() == edges.len() {
        return; // no duplicates
    }
    *edges = merged
        .into_iter()
        .map(|((from, to), count)| EdgeRecord { from, to, count })
        .collect();
}

fn parse_string_meta(payload: &[u8], profile: &mut Profile) {
    if payload.is_empty() {
        return;
    }
    let key_len = payload[0] as usize;
    if payload.len() < 1 + key_len + 2 {
        return;
    }
    let key = String::from_utf8_lossy(&payload[1..1 + key_len]).to_string();
    let val_len = u16::from_le_bytes([payload[1 + key_len], payload[2 + key_len]]) as usize;
    let val_start = 3 + key_len;
    if payload.len() < val_start + val_len {
        return;
    }
    let val = String::from_utf8_lossy(&payload[val_start..val_start + val_len]).to_string();
    profile.metadata.insert(key, val);
}

fn parse_symbol_entry(payload: &[u8], profile: &mut Profile) {
    if payload.len() < 12 {
        return;
    }
    let sym_id = u16::from_le_bytes([payload[0], payload[1]]);
    let addr = u64::from_le_bytes(payload[2..10].try_into().unwrap());
    let name_len = u16::from_le_bytes([payload[10], payload[11]]) as usize;
    if payload.len() < 12 + name_len {
        return;
    }
    let name = String::from_utf8_lossy(&payload[12..12 + name_len]).to_string();
    profile.symbols.push(SymbolRecord { sym_id, addr, name });
}

fn parse_tb_exec_batch(payload: &[u8], profile: &mut Profile) {
    if payload.len() < 4 {
        return;
    }
    let count = u32::from_le_bytes(payload[0..4].try_into().unwrap()) as usize;
    let mut off = 4;
    for _ in 0..count {
        if off + 40 > payload.len() {
            break;
        }
        let start_addr = u64::from_le_bytes(payload[off..off + 8].try_into().unwrap());
        let end_addr = u64::from_le_bytes(payload[off + 8..off + 16].try_into().unwrap());
        let n_insns = u16::from_le_bytes([payload[off + 16], payload[off + 17]]);
        // Skip 6 bytes of padding
        let exec_count = u64::from_le_bytes(payload[off + 24..off + 32].try_into().unwrap());
        let first_seen_seq = u64::from_le_bytes(payload[off + 32..off + 40].try_into().unwrap());
        profile.tb_execs.push(TbExec {
            start_addr,
            end_addr,
            n_insns,
            exec_count,
            first_seen_seq,
        });
        off += 40;
    }
}

fn parse_edge_batch(payload: &[u8]) -> Vec<EdgeRecord> {
    let mut edges = Vec::new();
    if payload.len() < 4 {
        return edges;
    }
    let count = u32::from_le_bytes(payload[0..4].try_into().unwrap()) as usize;
    let mut off = 4;
    for _ in 0..count {
        if off + 24 > payload.len() {
            break;
        }
        let from = u64::from_le_bytes(payload[off..off + 8].try_into().unwrap());
        let to = u64::from_le_bytes(payload[off + 8..off + 16].try_into().unwrap());
        let cnt = u64::from_le_bytes(payload[off + 16..off + 24].try_into().unwrap());
        edges.push(EdgeRecord {
            from,
            to,
            count: cnt,
        });
        off += 24;
    }
    edges
}

fn parse_branch_edge_batch(payload: &[u8], profile: &mut Profile) {
    profile.branch_edges.extend(parse_edge_batch(payload));
}

fn parse_fall_through_batch(payload: &[u8], profile: &mut Profile) {
    profile.fall_through_edges.extend(parse_edge_batch(payload));
}

fn parse_call_batch(payload: &[u8]) -> Vec<CallRecord> {
    let mut calls = Vec::new();
    if payload.len() < 4 {
        return calls;
    }
    let count = u32::from_le_bytes(payload[0..4].try_into().unwrap()) as usize;
    let mut off = 4;
    for _ in 0..count {
        if off + 24 > payload.len() {
            break;
        }
        let callsite = u64::from_le_bytes(payload[off..off + 8].try_into().unwrap());
        let target = u64::from_le_bytes(payload[off + 8..off + 16].try_into().unwrap());
        let cnt = u64::from_le_bytes(payload[off + 16..off + 24].try_into().unwrap());
        calls.push(CallRecord {
            callsite,
            target,
            count: cnt,
        });
        off += 24;
    }
    calls
}

fn parse_direct_call_batch(payload: &[u8], profile: &mut Profile) {
    profile.direct_calls.extend(parse_call_batch(payload));
}

fn parse_indirect_call_batch(payload: &[u8], profile: &mut Profile) {
    profile.indirect_calls.extend(parse_call_batch(payload));
}

fn parse_tb_resource(payload: &[u8], profile: &mut Profile) {
    if payload.len() < 20 {
        return;
    }
    let tb_addr = u64::from_le_bytes(payload[0..8].try_into().unwrap());
    let coproc_flags = u32::from_le_bytes(payload[8..12].try_into().unwrap());
    let vcpu_mask = u64::from_le_bytes(payload[12..20].try_into().unwrap());
    profile.tb_resources.push(TbResourceRecord {
        tb_addr,
        coproc_flags,
        vcpu_mask,
    });
}

fn parse_discontinuity(payload: &[u8], profile: &mut Profile) {
    if payload.len() < 28 {
        return;
    }
    let discon_type = u32::from_le_bytes(payload[0..4].try_into().unwrap());
    let from_pc = u64::from_le_bytes(payload[4..12].try_into().unwrap());
    let to_pc = u64::from_le_bytes(payload[12..20].try_into().unwrap());
    let count = u64::from_le_bytes(payload[20..28].try_into().unwrap());
    profile.discon_events.push(DisconRecord {
        discon_type,
        from_pc,
        to_pc,
        count,
    });
}

fn parse_footer(payload: &[u8], profile: &mut Profile) {
    if payload.len() < 40 {
        return;
    }
    let total_tb_count = u64::from_le_bytes(payload[0..8].try_into().unwrap());
    let total_edge_records = u64::from_le_bytes(payload[8..16].try_into().unwrap());
    let total_exec_count = u64::from_le_bytes(payload[16..24].try_into().unwrap());
    let wall_time_ns = u64::from_le_bytes(payload[24..32].try_into().unwrap());
    let flags = u32::from_le_bytes(payload[32..36].try_into().unwrap());
    profile.footer = Some(Footer {
        total_tb_count,
        total_edge_records,
        total_exec_count,
        wall_time_ns,
        clean_exit: (flags & 0x1) != 0,
    });
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::io::Write;

    /// Build a minimal valid profile file in memory and write it to
    /// a temp file. Returns the path.
    fn write_test_profile(
        tier: u8,
        tb_execs: &[(u64, u64, u16, u64, u64)],
        branch_edges: &[(u64, u64, u64)],
        fall_throughs: &[(u64, u64, u64)],
    ) -> String {
        let dir = std::env::temp_dir();
        let path = dir
            .join(format!(
                "test_native_{}.pgo",
                std::time::SystemTime::now()
                    .duration_since(std::time::UNIX_EPOCH)
                    .unwrap()
                    .as_nanos()
            ))
            .to_string_lossy()
            .to_string();

        let mut f = std::fs::File::create(&path).expect("create temp file");

        // File header (64 bytes)
        f.write_all(&FILE_MAGIC.to_le_bytes()).unwrap();
        f.write_all(&1u16.to_le_bytes()).unwrap(); // version
        f.write_all(&[tier]).unwrap(); // tier
        f.write_all(&[8u8]).unwrap(); // pointer_size
        f.write_all(&0u32.to_le_bytes()).unwrap(); // flags
        f.write_all(&0x1000u64.to_le_bytes()).unwrap(); // load_addr
        f.write_all(&0x1000u64.to_le_bytes()).unwrap(); // text_start
        f.write_all(&0x9000u64.to_le_bytes()).unwrap(); // text_end
        f.write_all(&0x1000u64.to_le_bytes()).unwrap(); // entry
        f.write_all(&[0u8; 16]).unwrap(); // build_id

        // String metadata record
        write_tlv(&mut f, 0x01, &build_string_meta("arch", "hexagon"));

        // TB exec batch
        if !tb_execs.is_empty() {
            let mut payload = Vec::new();
            payload.extend_from_slice(&(tb_execs.len() as u32).to_le_bytes());
            for &(addr, end_addr, n_insns, exec, seq) in tb_execs {
                payload.extend_from_slice(&addr.to_le_bytes());
                payload.extend_from_slice(&end_addr.to_le_bytes());
                payload.extend_from_slice(&n_insns.to_le_bytes());
                payload.extend_from_slice(&[0u8; 6]); // pad
                payload.extend_from_slice(&exec.to_le_bytes());
                payload.extend_from_slice(&seq.to_le_bytes());
            }
            write_tlv(&mut f, 0x13, &payload);
        }

        // Branch edge batch
        if !branch_edges.is_empty() {
            let mut payload = Vec::new();
            payload.extend_from_slice(&(branch_edges.len() as u32).to_le_bytes());
            for &(from, to, count) in branch_edges {
                payload.extend_from_slice(&from.to_le_bytes());
                payload.extend_from_slice(&to.to_le_bytes());
                payload.extend_from_slice(&count.to_le_bytes());
            }
            write_tlv(&mut f, 0x21, &payload);
        }

        // Fall-through batch
        if !fall_throughs.is_empty() {
            let mut payload = Vec::new();
            payload.extend_from_slice(&(fall_throughs.len() as u32).to_le_bytes());
            for &(from, to, count) in fall_throughs {
                payload.extend_from_slice(&from.to_le_bytes());
                payload.extend_from_slice(&to.to_le_bytes());
                payload.extend_from_slice(&count.to_le_bytes());
            }
            write_tlv(&mut f, 0x23, &payload);
        }

        // Footer
        let mut footer_payload = Vec::new();
        footer_payload.extend_from_slice(&(tb_execs.len() as u64).to_le_bytes());
        let total_edges = (branch_edges.len() + fall_throughs.len()) as u64;
        footer_payload.extend_from_slice(&total_edges.to_le_bytes());
        let total_exec: u64 = tb_execs.iter().map(|t| t.3).sum();
        footer_payload.extend_from_slice(&total_exec.to_le_bytes());
        footer_payload.extend_from_slice(&1000000u64.to_le_bytes()); // wall_time
        footer_payload.extend_from_slice(&1u32.to_le_bytes()); // clean exit
        footer_payload.extend_from_slice(&[0u8; 4]); // reserved
        write_tlv(&mut f, 0xFF, &footer_payload);

        path
    }

    fn write_tlv(f: &mut std::fs::File, tag: u8, payload: &[u8]) {
        let len = payload.len() as u32;
        let len_bytes = len.to_le_bytes();
        f.write_all(&[tag, len_bytes[0], len_bytes[1], len_bytes[2]])
            .unwrap();
        f.write_all(payload).unwrap();
    }

    fn build_string_meta(key: &str, val: &str) -> Vec<u8> {
        let mut payload = Vec::new();
        payload.push(key.len() as u8);
        payload.extend_from_slice(key.as_bytes());
        payload.extend_from_slice(&(val.len() as u16).to_le_bytes());
        payload.extend_from_slice(val.as_bytes());
        payload
    }

    #[test]
    fn parse_minimal_profile() {
        let path = write_test_profile(1, &[], &[], &[]);
        let profile = parse(&path).unwrap();
        assert_eq!(profile.header.format_version, 1);
        assert_eq!(profile.header.tier, 1);
        assert_eq!(profile.header.load_addr, 0x1000);
        assert_eq!(
            profile.metadata.get("arch").map(|s| s.as_str()),
            Some("hexagon")
        );
        assert!(profile.footer.is_some());
        assert!(profile.footer.unwrap().clean_exit);
        std::fs::remove_file(&path).ok();
    }

    #[test]
    fn parse_tb_exec_data() {
        let tb_execs = vec![
            (0x1000u64, 0x100cu64, 4u16, 100u64, 1u64),
            (0x2000, 0x201c, 8, 50, 2),
            (0x3000, 0x3004, 2, 200, 3),
        ];
        let path = write_test_profile(0, &tb_execs, &[], &[]);
        let profile = parse(&path).unwrap();

        assert_eq!(profile.tb_execs.len(), 3);
        assert_eq!(profile.tb_execs[0].start_addr, 0x1000);
        assert_eq!(profile.tb_execs[0].end_addr, 0x100c);
        assert_eq!(profile.tb_execs[0].n_insns, 4);
        assert_eq!(profile.tb_execs[0].exec_count, 100);
        assert_eq!(profile.tb_execs[0].first_seen_seq, 1);
        assert_eq!(profile.tb_execs[2].exec_count, 200);

        let footer = profile.footer.unwrap();
        assert_eq!(footer.total_tb_count, 3);
        assert_eq!(footer.total_exec_count, 350);
        std::fs::remove_file(&path).ok();
    }

    #[test]
    fn parse_branch_and_fall_through_edges() {
        let branches = vec![(0x100cu64, 0x2000u64, 42u64), (0x200cu64, 0x3000u64, 7u64)];
        let fall_throughs = vec![(0x100cu64, 0x1010u64, 58u64)];
        let path = write_test_profile(1, &[], &branches, &fall_throughs);
        let profile = parse(&path).unwrap();

        assert_eq!(profile.branch_edges.len(), 2);
        assert_eq!(profile.branch_edges[0].from, 0x100c);
        assert_eq!(profile.branch_edges[0].to, 0x2000);
        assert_eq!(profile.branch_edges[0].count, 42);

        assert_eq!(profile.fall_through_edges.len(), 1);
        assert_eq!(profile.fall_through_edges[0].count, 58);
        std::fs::remove_file(&path).ok();
    }

    #[test]
    fn parse_bad_magic_returns_error() {
        let dir = std::env::temp_dir();
        let path = dir.join("test_bad_magic.pgo");
        let path_str = path.to_string_lossy().to_string();
        {
            let mut f = std::fs::File::create(&path_str).unwrap();
            f.write_all(&0xDEADBEEFu64.to_le_bytes()).unwrap();
            f.write_all(&[0u8; 56]).unwrap(); // fill rest of header
        }
        let result = parse(&path_str);
        assert!(result.is_err());
        assert!(result.unwrap_err().contains("bad magic"));
        std::fs::remove_file(&path_str).ok();
    }

    #[test]
    fn parse_header_only_no_records() {
        // A file with just a 64-byte header and no records should
        // parse successfully (no footer).
        let dir = std::env::temp_dir();
        let path = dir.join("test_header_only.pgo");
        let path_str = path.to_string_lossy().to_string();
        {
            let mut f = std::fs::File::create(&path_str).unwrap();
            f.write_all(&FILE_MAGIC.to_le_bytes()).unwrap();
            f.write_all(&1u16.to_le_bytes()).unwrap(); // version
            f.write_all(&[1u8]).unwrap(); // tier
            f.write_all(&[8u8]).unwrap(); // ptr_size
            f.write_all(&0u32.to_le_bytes()).unwrap(); // flags
            f.write_all(&0u64.to_le_bytes()).unwrap(); // load_addr
            f.write_all(&0u64.to_le_bytes()).unwrap(); // text_start
            f.write_all(&0u64.to_le_bytes()).unwrap(); // text_end
            f.write_all(&0u64.to_le_bytes()).unwrap(); // entry_addr
            f.write_all(&[0u8; 16]).unwrap(); // build_id
        }
        let profile = parse(&path_str).unwrap();
        assert!(profile.footer.is_none());
        assert!(profile.tb_execs.is_empty());
        std::fs::remove_file(&path_str).ok();
    }

    #[test]
    fn merged_edges_combines_branch_and_fall_through() {
        let path = write_test_profile(1, &[], &[(0x100c, 0x2000, 10)], &[(0x100c, 0x2000, 5)]);
        let profile = parse(&path).unwrap();
        let merged = profile.merged_edges();
        assert_eq!(*merged.get(&(0x100c, 0x2000)).unwrap(), 15);
        std::fs::remove_file(&path).ok();
    }

    #[test]
    fn unknown_tags_are_skipped() {
        let dir = std::env::temp_dir();
        let path = dir.join("test_unknown_tag.pgo");
        let path_str = path.to_string_lossy().to_string();
        {
            let mut f = std::fs::File::create(&path_str).unwrap();
            // Header
            f.write_all(&FILE_MAGIC.to_le_bytes()).unwrap();
            f.write_all(&1u16.to_le_bytes()).unwrap();
            f.write_all(&[1u8, 8u8]).unwrap();
            f.write_all(&0u32.to_le_bytes()).unwrap();
            f.write_all(&[0u8; 40]).unwrap();
            // Unknown tag 0xAA with 4 bytes of payload
            write_tlv(&mut f, 0xAA, &[1, 2, 3, 4]);
            // Valid footer
            let mut footer = Vec::new();
            footer.extend_from_slice(&0u64.to_le_bytes()); // tb count
            footer.extend_from_slice(&0u64.to_le_bytes()); // edge count
            footer.extend_from_slice(&0u64.to_le_bytes()); // exec count
            footer.extend_from_slice(&0u64.to_le_bytes()); // wall time
            footer.extend_from_slice(&1u32.to_le_bytes()); // flags
            footer.extend_from_slice(&[0u8; 4]);
            write_tlv(&mut f, 0xFF, &footer);
        }
        let profile = parse(&path_str).unwrap();
        assert!(profile.footer.is_some());
        assert!(profile.footer.unwrap().clean_exit);
        std::fs::remove_file(&path_str).ok();
    }

    /// Duplicate edges from multiple streaming flushes are merged.
    #[test]
    fn streaming_duplicate_edges_merged() {
        let dir = std::env::temp_dir();
        let path = dir.join("test_streaming_merge.pgo");
        let path_str = path.to_string_lossy().to_string();
        {
            let mut f = std::fs::File::create(&path_str).unwrap();
            // Header (64 bytes)
            f.write_all(&FILE_MAGIC.to_le_bytes()).unwrap();
            f.write_all(&1u16.to_le_bytes()).unwrap(); // version
            f.write_all(&[1u8]).unwrap(); // tier
            f.write_all(&[8u8]).unwrap(); // pointer_size
            f.write_all(&0u32.to_le_bytes()).unwrap(); // flags
            f.write_all(&0u64.to_le_bytes()).unwrap(); // load_addr
            f.write_all(&0u64.to_le_bytes()).unwrap(); // text_start
            f.write_all(&0u64.to_le_bytes()).unwrap(); // text_end
            f.write_all(&0u64.to_le_bytes()).unwrap(); // entry_addr
            f.write_all(&[0u8; 16]).unwrap(); // build_id

            // First batch: edge A->B count=10, edge C->D count=5
            let mut payload1 = Vec::new();
            payload1.extend_from_slice(&2u32.to_le_bytes());
            for &(from, to, count) in &[(0x1000u64, 0x2000u64, 10u64),
                                         (0x3000u64, 0x4000u64, 5u64)] {
                payload1.extend_from_slice(&from.to_le_bytes());
                payload1.extend_from_slice(&to.to_le_bytes());
                payload1.extend_from_slice(&count.to_le_bytes());
            }
            write_tlv(&mut f, 0x21, &payload1);

            // Second batch: edge A->B count=20 (duplicate)
            let mut payload2 = Vec::new();
            payload2.extend_from_slice(&1u32.to_le_bytes());
            payload2.extend_from_slice(&0x1000u64.to_le_bytes());
            payload2.extend_from_slice(&0x2000u64.to_le_bytes());
            payload2.extend_from_slice(&20u64.to_le_bytes());
            write_tlv(&mut f, 0x21, &payload2);

            // Footer
            let mut footer = Vec::new();
            footer.extend_from_slice(&0u64.to_le_bytes());
            footer.extend_from_slice(&3u64.to_le_bytes());
            footer.extend_from_slice(&0u64.to_le_bytes());
            footer.extend_from_slice(&0u64.to_le_bytes());
            footer.extend_from_slice(&1u32.to_le_bytes());
            footer.extend_from_slice(&[0u8; 4]);
            write_tlv(&mut f, 0xFF, &footer);
        }
        let profile = parse(&path_str).unwrap();

        // A->B should be merged: 10 + 20 = 30
        assert_eq!(profile.branch_edges.len(), 2);
        let ab = profile.branch_edges.iter()
            .find(|e| e.from == 0x1000 && e.to == 0x2000)
            .expect("A->B edge missing");
        assert_eq!(ab.count, 30);
        let cd = profile.branch_edges.iter()
            .find(|e| e.from == 0x3000 && e.to == 0x4000)
            .expect("C->D edge missing");
        assert_eq!(cd.count, 5);

        std::fs::remove_file(&path_str).ok();
    }
}
