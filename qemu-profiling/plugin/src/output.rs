// SPDX-License-Identifier: BSD-3-Clause-Clear
// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//
// Append-only binary log writer for the native PGO profile format.
// Uses TLV (Tag-Length-Value) encoding for extensibility and crash
// tolerance.

use std::fs::File;
use std::io::{BufWriter, Write};
use std::time::Instant;

/// Magic number: "QEMU_PGO" in ASCII.
pub const FILE_MAGIC: u64 = 0x4F47505F554D4551;

/// Current format version.
pub const FORMAT_VERSION: u16 = 1;

/// File header flags.
pub const FLAG_SYSEMU: u32 = 0x1;
pub const FLAG_STREAMING: u32 = 0x2;

/// Record tag values.
/// Tag ranges:
///   0x00-0x0F: metadata
///   0x10-0x1F: tier 0 (hotness)
///   0x20-0x2F: tier 1 (edges)
///   0x30-0x3F: tier 2 (calls)
///   0x40-0x4F: tier 3 (resources)
///   0xF0-0xFF: control
#[repr(u8)]
#[derive(Copy, Clone, Debug, PartialEq, Eq)]
pub enum RecordTag {
    // Metadata
    StringMeta = 0x01,
    SymbolEntry = 0x02,

    // Tier 0: Hotness
    TbExecBatch = 0x13,
    FirstSeenOrder = 0x14,

    // Tier 1: Edges
    BranchEdgeBatch = 0x21,
    FallThroughBatch = 0x23,
    FlushMarker = 0x2F,

    // Tier 2: Calls
    DirectCallBatch = 0x32,
    IndirectCallBatch = 0x33,

    // Tier 3: Resources
    TbResource = 0x40,
    DeviceAccess = 0x41,
    Discontinuity = 0x42,

    // Control
    Footer = 0xFF,
}

/// Fixed 64-byte file header.
#[repr(C, packed)]
pub struct FileHeader {
    pub magic: u64,
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

/// Writer for the native PGO profile format.
pub struct ProfileWriter {
    writer: BufWriter<File>,
    start_time: Instant,
    total_tb_count: u64,
    total_edge_records: u64,
    total_exec_count: u64,
}

impl ProfileWriter {
    /// Create a new profile writer and write the file header.
    pub fn new(
        path: &str,
        tier: u8,
        sysemu: bool,
        streaming: bool,
        load_addr: u64,
        text_start: u64,
        text_end: u64,
        entry_addr: u64,
    ) -> std::io::Result<Self> {
        let file = File::create(path)?;
        let mut writer = BufWriter::with_capacity(64 * 1024, file);

        let mut flags: u32 = 0;
        if sysemu {
            flags |= FLAG_SYSEMU;
        }
        if streaming {
            flags |= FLAG_STREAMING;
        }

        let header = FileHeader {
            magic: FILE_MAGIC,
            format_version: FORMAT_VERSION,
            tier,
            pointer_size: 8,
            flags,
            load_addr,
            text_start,
            text_end,
            entry_addr,
            build_id: [0u8; 16],
        };

        // Safety: FileHeader is packed C repr, writing raw bytes is fine.
        let header_bytes: &[u8] = unsafe {
            std::slice::from_raw_parts(
                &header as *const FileHeader as *const u8,
                std::mem::size_of::<FileHeader>(),
            )
        };
        writer.write_all(header_bytes)?;

        Ok(Self {
            writer,
            start_time: Instant::now(),
            total_tb_count: 0,
            total_edge_records: 0,
            total_exec_count: 0,
        })
    }

    /// Maximum payload length for a single TLV record (24-bit field).
    const MAX_TLV_PAYLOAD: u32 = (1 << 24) - 1;

    /// Write a TLV record header: [tag: u8][len: u24 LE].
    fn write_tlv_header(&mut self, tag: RecordTag, payload_len: u32) -> std::io::Result<()> {
        if payload_len > Self::MAX_TLV_PAYLOAD {
            return Err(std::io::Error::new(
                std::io::ErrorKind::InvalidInput,
                format!(
                    "TLV payload {} exceeds 24-bit max {}",
                    payload_len,
                    Self::MAX_TLV_PAYLOAD
                ),
            ));
        }
        let tag_byte = tag as u8;
        let len_bytes = payload_len.to_le_bytes();
        // tag(1) + len(3) = 4 bytes
        self.writer
            .write_all(&[tag_byte, len_bytes[0], len_bytes[1], len_bytes[2]])
    }

    /// Write a string metadata record (key-value pair).
    pub fn write_string_meta(&mut self, key: &str, value: &str) -> std::io::Result<()> {
        let key_bytes = key.as_bytes();
        let val_bytes = value.as_bytes();
        let key_len = key_bytes.len().min(255) as u8;
        let val_len = val_bytes.len().min(65535) as u16;
        let payload_len = 1 + key_len as u32 + 2 + val_len as u32;

        self.write_tlv_header(RecordTag::StringMeta, payload_len)?;
        self.writer.write_all(&[key_len])?;
        self.writer.write_all(&key_bytes[..key_len as usize])?;
        self.writer.write_all(&val_len.to_le_bytes())?;
        self.writer.write_all(&val_bytes[..val_len as usize])
    }

    /// Write a symbol entry record.
    pub fn write_symbol_entry(
        &mut self,
        sym_id: u16,
        addr: u64,
        name: &str,
    ) -> std::io::Result<()> {
        let name_bytes = name.as_bytes();
        let name_len = name_bytes.len().min(65535) as u16;
        // sym_id(2) + addr(8) + name_len(2) + name
        let payload_len = 2 + 8 + 2 + name_len as u32;

        self.write_tlv_header(RecordTag::SymbolEntry, payload_len)?;
        self.writer.write_all(&sym_id.to_le_bytes())?;
        self.writer.write_all(&addr.to_le_bytes())?;
        self.writer.write_all(&name_len.to_le_bytes())?;
        self.writer.write_all(&name_bytes[..name_len as usize])
    }

    /// Maximum entries per TLV record for TB exec batches (40 bytes each).
    /// (MAX_TLV_PAYLOAD - 4 bytes for count field) / 40 bytes per entry.
    const MAX_TB_EXEC_PER_RECORD: usize = ((1 << 24) - 1 - 4) / 40;

    /// Write a batch of TB execution counts, splitting into multiple
    /// TLV records if needed to stay within the 24-bit length limit.
    /// Each entry: start_addr(8) + end_addr(8) + n_insns(2) + pad(6) +
    /// exec_count(8) + first_seen_seq(8) = 40 bytes.
    pub fn write_tb_exec_batch(
        &mut self,
        entries: &[(u64, u64, u16, u64, u64)], // (start_addr, end_addr, n_insns, exec_count, first_seen)
    ) -> std::io::Result<()> {
        for chunk in entries.chunks(Self::MAX_TB_EXEC_PER_RECORD) {
            let count = chunk.len() as u32;
            let payload_len = 4 + count * 40;

            self.write_tlv_header(RecordTag::TbExecBatch, payload_len)?;
            self.writer.write_all(&count.to_le_bytes())?;
            for &(start_addr, end_addr, n_insns, exec_count, first_seen) in chunk {
                self.writer.write_all(&start_addr.to_le_bytes())?;
                self.writer.write_all(&end_addr.to_le_bytes())?;
                self.writer.write_all(&n_insns.to_le_bytes())?;
                self.writer.write_all(&[0u8; 6])?; // padding
                self.writer.write_all(&exec_count.to_le_bytes())?;
                self.writer.write_all(&first_seen.to_le_bytes())?;
                self.total_exec_count += exec_count;
            }
            self.total_tb_count += count as u64;
        }
        Ok(())
    }

    /// Maximum entries per TLV record for edge batches (24 bytes each).
    const MAX_EDGES_PER_RECORD: usize = ((1 << 24) - 1 - 4) / 24;

    /// Write a batch of branch edges, splitting into multiple TLV
    /// records if needed to stay within the 24-bit length limit.
    /// Each entry: from_addr(8) + to_addr(8) + count(8) = 24 bytes.
    pub fn write_branch_edge_batch(
        &mut self,
        edges: &[(u64, u64, u64)], // (from, to, count)
    ) -> std::io::Result<()> {
        for chunk in edges.chunks(Self::MAX_EDGES_PER_RECORD) {
            let count = chunk.len() as u32;
            let payload_len = 4 + count * 24;

            self.write_tlv_header(RecordTag::BranchEdgeBatch, payload_len)?;
            self.writer.write_all(&count.to_le_bytes())?;
            for &(from, to, cnt) in chunk {
                self.writer.write_all(&from.to_le_bytes())?;
                self.writer.write_all(&to.to_le_bytes())?;
                self.writer.write_all(&cnt.to_le_bytes())?;
            }
            self.total_edge_records += count as u64;
        }
        Ok(())
    }

    /// Write a batch of fall-through edges.
    pub fn write_fall_through_batch(&mut self, edges: &[(u64, u64, u64)]) -> std::io::Result<()> {
        for chunk in edges.chunks(Self::MAX_EDGES_PER_RECORD) {
            let count = chunk.len() as u32;
            let payload_len = 4 + count * 24;

            self.write_tlv_header(RecordTag::FallThroughBatch, payload_len)?;
            self.writer.write_all(&count.to_le_bytes())?;
            for &(from, to, cnt) in chunk {
                self.writer.write_all(&from.to_le_bytes())?;
                self.writer.write_all(&to.to_le_bytes())?;
                self.writer.write_all(&cnt.to_le_bytes())?;
            }
            self.total_edge_records += count as u64;
        }
        Ok(())
    }

    /// Write a streaming flush marker.
    pub fn write_flush_marker(&mut self, vcpu_index: u32, num_edges: u32) -> std::io::Result<()> {
        let elapsed = self.start_time.elapsed().as_nanos() as u64;
        let payload_len = 8 + 4 + 4;

        self.write_tlv_header(RecordTag::FlushMarker, payload_len)?;
        self.writer.write_all(&elapsed.to_le_bytes())?;
        self.writer.write_all(&vcpu_index.to_le_bytes())?;
        self.writer.write_all(&num_edges.to_le_bytes())?;
        self.writer.flush()
    }

    /// Write a batch of direct call records.
    /// Each entry: callsite(8) + target(8) + count(8) = 24 bytes.
    pub fn write_direct_call_batch(&mut self, calls: &[(u64, u64, u64)]) -> std::io::Result<()> {
        for chunk in calls.chunks(Self::MAX_EDGES_PER_RECORD) {
            let count = chunk.len() as u32;
            let payload_len = 4 + count * 24;

            self.write_tlv_header(RecordTag::DirectCallBatch, payload_len)?;
            self.writer.write_all(&count.to_le_bytes())?;
            for &(callsite, target, cnt) in chunk {
                self.writer.write_all(&callsite.to_le_bytes())?;
                self.writer.write_all(&target.to_le_bytes())?;
                self.writer.write_all(&cnt.to_le_bytes())?;
            }
        }
        Ok(())
    }

    /// Write a batch of indirect call records.
    pub fn write_indirect_call_batch(&mut self, calls: &[(u64, u64, u64)]) -> std::io::Result<()> {
        for chunk in calls.chunks(Self::MAX_EDGES_PER_RECORD) {
            let count = chunk.len() as u32;
            let payload_len = 4 + count * 24;

            self.write_tlv_header(RecordTag::IndirectCallBatch, payload_len)?;
            self.writer.write_all(&count.to_le_bytes())?;
            for &(callsite, target, cnt) in chunk {
                self.writer.write_all(&callsite.to_le_bytes())?;
                self.writer.write_all(&target.to_le_bytes())?;
                self.writer.write_all(&cnt.to_le_bytes())?;
            }
        }
        Ok(())
    }

    /// Write a TB resource annotation (tier 3).
    pub fn write_tb_resource(
        &mut self,
        tb_addr: u64,
        coproc_flags: u32,
        vcpu_mask: u64,
    ) -> std::io::Result<()> {
        // tb_addr(8) + coproc_flags(4) + vcpu_mask(8) = 20
        let payload_len = 20u32;

        self.write_tlv_header(RecordTag::TbResource, payload_len)?;
        self.writer.write_all(&tb_addr.to_le_bytes())?;
        self.writer.write_all(&coproc_flags.to_le_bytes())?;
        self.writer.write_all(&vcpu_mask.to_le_bytes())
    }

    /// Write a device access record (tier 3).
    pub fn write_device_access(&mut self, tb_addr: u64, device_name: &str) -> std::io::Result<()> {
        let name_bytes = device_name.as_bytes();
        let name_len = name_bytes.len().min(255) as u8;
        let payload_len = 8 + 1 + name_len as u32;

        self.write_tlv_header(RecordTag::DeviceAccess, payload_len)?;
        self.writer.write_all(&tb_addr.to_le_bytes())?;
        self.writer.write_all(&[name_len])?;
        self.writer.write_all(&name_bytes[..name_len as usize])
    }

    /// Write a discontinuity event record (tier 3).
    pub fn write_discontinuity(
        &mut self,
        discon_type: u32,
        from_pc: u64,
        to_pc: u64,
        count: u64,
    ) -> std::io::Result<()> {
        // type(4) + from(8) + to(8) + count(8) = 28
        let payload_len = 28u32;

        self.write_tlv_header(RecordTag::Discontinuity, payload_len)?;
        self.writer.write_all(&discon_type.to_le_bytes())?;
        self.writer.write_all(&from_pc.to_le_bytes())?;
        self.writer.write_all(&to_pc.to_le_bytes())?;
        self.writer.write_all(&count.to_le_bytes())
    }

    /// Write the footer and flush. Call at clean exit.
    pub fn write_footer(&mut self) -> std::io::Result<()> {
        let wall_time_ns = self.start_time.elapsed().as_nanos() as u64;
        // total_tb_count(8) + total_edge_records(8) + total_exec_count(8)
        // + wall_time_ns(8) + flags(4) + reserved(4) = 40
        let payload_len = 40u32;

        self.write_tlv_header(RecordTag::Footer, payload_len)?;
        self.writer.write_all(&self.total_tb_count.to_le_bytes())?;
        self.writer
            .write_all(&self.total_edge_records.to_le_bytes())?;
        self.writer
            .write_all(&self.total_exec_count.to_le_bytes())?;
        self.writer.write_all(&wall_time_ns.to_le_bytes())?;
        let flags: u32 = 0x1; // clean exit
        self.writer.write_all(&flags.to_le_bytes())?;
        self.writer.write_all(&[0u8; 4])?; // reserved
        self.writer.flush()
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::io::Read;

    /// Write a minimal profile and verify the file header.
    #[test]
    fn file_header_round_trip() {
        let dir = std::env::temp_dir();
        let path = dir.join("test_header.pgo");
        let path_str = path.to_str().unwrap();

        {
            let mut w =
                ProfileWriter::new(path_str, 1, false, false, 0x1000, 0x1000, 0x2000, 0x1000)
                    .unwrap();
            w.write_footer().unwrap();
        }

        let mut f = std::fs::File::open(path_str).unwrap();
        let mut buf = [0u8; 64];
        f.read_exact(&mut buf).unwrap();

        // Verify magic
        let magic = u64::from_le_bytes(buf[0..8].try_into().unwrap());
        assert_eq!(magic, FILE_MAGIC);

        // Verify version
        let version = u16::from_le_bytes([buf[8], buf[9]]);
        assert_eq!(version, FORMAT_VERSION);

        // Verify tier
        assert_eq!(buf[10], 1);

        // Verify pointer size
        assert_eq!(buf[11], 8);

        // Verify load_addr
        let load_addr = u64::from_le_bytes(buf[16..24].try_into().unwrap());
        assert_eq!(load_addr, 0x1000);

        std::fs::remove_file(path_str).ok();
    }

    /// Verify TLV header encoding.
    #[test]
    fn tlv_header_encoding() {
        let dir = std::env::temp_dir();
        let path = dir.join("test_tlv.pgo");
        let path_str = path.to_str().unwrap();

        {
            let mut w = ProfileWriter::new(path_str, 0, false, false, 0, 0, 0, 0).unwrap();
            w.write_string_meta("k", "v").unwrap();
            w.write_footer().unwrap();
        }

        let data = std::fs::read(path_str).unwrap();
        // After 64-byte header, first record should be StringMeta
        assert_eq!(data[64], RecordTag::StringMeta as u8);
        // len = 1 (key_len) + 1 (key) + 2 (val_len) + 1 (val) = 5
        let len = u32::from_le_bytes([data[65], data[66], data[67], 0]);
        assert_eq!(len, 5);

        std::fs::remove_file(path_str).ok();
    }

    /// Write TB exec batch and verify record count tracking.
    #[test]
    fn tb_exec_batch_tracking() {
        let dir = std::env::temp_dir();
        let path = dir.join("test_tb_batch.pgo");
        let path_str = path.to_str().unwrap();

        {
            let mut w = ProfileWriter::new(path_str, 0, false, false, 0, 0, 0, 0).unwrap();
            let entries = vec![
                (0x1000u64, 0x100cu64, 4u16, 100u64, 1u64),
                (0x2000, 0x201c, 8, 50, 2),
            ];
            w.write_tb_exec_batch(&entries).unwrap();
            w.write_footer().unwrap();
        }

        let data = std::fs::read(path_str).unwrap();
        // Find footer: last record. Read its total_exec_count.
        // The footer tag is 0xFF, preceded by the TLV header.
        let footer_pos = data.windows(1).rposition(|w| w[0] == 0xFF).unwrap();
        let payload = &data[footer_pos + 4..];
        let total_exec = u64::from_le_bytes(payload[16..24].try_into().unwrap());
        assert_eq!(total_exec, 150); // 100 + 50

        std::fs::remove_file(path_str).ok();
    }

    /// Write branch edge batch and verify edge count tracking.
    #[test]
    fn branch_edge_batch_tracking() {
        let dir = std::env::temp_dir();
        let path = dir.join("test_edge_batch.pgo");
        let path_str = path.to_str().unwrap();

        {
            let mut w = ProfileWriter::new(path_str, 1, false, false, 0, 0, 0, 0).unwrap();
            let edges = vec![(0x1000u64, 0x2000u64, 42u64), (0x3000, 0x4000, 7)];
            w.write_branch_edge_batch(&edges).unwrap();
            w.write_footer().unwrap();
        }

        let data = std::fs::read(path_str).unwrap();
        let footer_pos = data.windows(1).rposition(|w| w[0] == 0xFF).unwrap();
        let payload = &data[footer_pos + 4..];
        let total_edges = u64::from_le_bytes(payload[8..16].try_into().unwrap());
        assert_eq!(total_edges, 2);

        std::fs::remove_file(path_str).ok();
    }
}
