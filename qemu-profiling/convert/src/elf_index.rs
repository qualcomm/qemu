// SPDX-License-Identifier: BSD-3-Clause-Clear
// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//
// ELF symbol and section resolver using goblin.

use std::collections::BTreeMap;
use std::fs;

/// Symbol information from ELF.
#[derive(Debug, Clone)]
pub struct Symbol {
    pub name: String,
    pub addr: u64,
    pub size: u64,
}

/// Index of ELF symbols for address-to-symbol lookups.
pub struct ElfIndex {
    /// Symbols sorted by address for efficient lookup.
    symbols: BTreeMap<u64, Symbol>,
    /// Load address of the text segment.
    pub load_addr: u64,
    /// Build ID, if present.
    pub build_id: Option<Vec<u8>>,
}

impl ElfIndex {
    /// Parse an ELF binary and build the symbol index.
    pub fn from_file(path: &str) -> Result<Self, String> {
        let data = fs::read(path).map_err(|e| format!("read {}: {}", path, e))?;

        let elf = goblin::elf::Elf::parse(&data).map_err(|e| format!("parse ELF: {}", e))?;

        let mut symbols = BTreeMap::new();

        // Index both .symtab and .dynsym. When two symbols share
        // the same address, keep the one with the longer name
        // (heuristic: prefer non-alias names).
        for sym in elf.syms.iter().chain(elf.dynsyms.iter()) {
            if sym.st_type() != goblin::elf::sym::STT_FUNC {
                continue;
            }
            if sym.st_value == 0 {
                continue;
            }
            let name = elf.strtab.get_at(sym.st_name).unwrap_or("").to_string();
            if name.is_empty() {
                continue;
            }
            // For duplicate addresses: keep the entry with the longer
            // name (R8 fix), or the one with non-zero size.
            if let Some(existing) = symbols.get(&sym.st_value) {
                let existing: &Symbol = existing;
                // Prefer entry with size > 0, or longer name
                if existing.size > 0 && sym.st_size == 0 {
                    continue;
                }
                if existing.size == 0 && sym.st_size > 0 {
                    // Replace: new one has size
                } else if name.len() <= existing.name.len() {
                    continue;
                }
            }
            symbols.insert(
                sym.st_value,
                Symbol {
                    name,
                    addr: sym.st_value,
                    size: sym.st_size,
                },
            );
        }

        // For symbols with st_size == 0 (R7): infer size from the gap
        // to the next symbol. Collect addresses first, then patch.
        let addrs: Vec<u64> = symbols.keys().copied().collect();
        for i in 0..addrs.len() {
            let addr = addrs[i];
            if let Some(sym) = symbols.get_mut(&addr) {
                if sym.size == 0 {
                    // Infer size from next symbol's address, capped at 64KB
                    let next_addr = if i + 1 < addrs.len() {
                        addrs[i + 1]
                    } else {
                        addr + 0x1000 // fallback: assume 4KB
                    };
                    sym.size = (next_addr - addr).min(0x10000);
                }
            }
        }

        // Find load address (lowest LOAD segment with exec flag)
        let mut load_addr = 0u64;
        for phdr in &elf.program_headers {
            if phdr.p_type == goblin::elf::program_header::PT_LOAD
                && (phdr.p_flags & goblin::elf::program_header::PF_X) != 0
            {
                load_addr = phdr.p_vaddr;
                break;
            }
        }

        // Extract build ID from .note.gnu.build-id.
        // Note fields use the ELF's byte order (e_ident[EI_DATA]:
        // 1 = little-endian, 2 = big-endian).
        let is_le = elf.header.e_ident[goblin::elf::header::EI_DATA]
            != goblin::elf::header::ELFDATA2MSB;
        let read_note_u32 = |bytes: &[u8]| -> Option<u32> {
            let arr: [u8; 4] = bytes.try_into().ok()?;
            Some(if is_le {
                u32::from_le_bytes(arr)
            } else {
                u32::from_be_bytes(arr)
            })
        };

        let build_id = elf
            .section_headers
            .iter()
            .find(|sh| elf.shdr_strtab.get_at(sh.sh_name).unwrap_or("") == ".note.gnu.build-id")
            .and_then(|sh| {
                let offset = sh.sh_offset as usize;
                let size = sh.sh_size as usize;
                if offset + size <= data.len() && size > 16 {
                    // Note format: namesz(4) + descsz(4) + type(4)
                    // + name("GNU\0") + desc(build-id bytes)
                    let namesz = read_note_u32(&data[offset..offset + 4])? as usize;
                    let descsz = read_note_u32(&data[offset + 4..offset + 8])? as usize;
                    let desc_offset = offset + 12 + ((namesz + 3) & !3);
                    if desc_offset + descsz <= data.len() {
                        Some(data[desc_offset..desc_offset + descsz].to_vec())
                    } else {
                        None
                    }
                } else {
                    None
                }
            });

        Ok(Self {
            symbols,
            load_addr,
            build_id,
        })
    }

    /// Create an ElfIndex with no symbols, for testing.
    #[cfg(test)]
    pub fn empty(load_addr: u64) -> Self {
        Self {
            symbols: BTreeMap::new(),
            load_addr,
            build_id: None,
        }
    }

    /// Look up the symbol containing the given address.
    pub fn symbol_at(&self, addr: u64) -> Option<&Symbol> {
        // Find the largest symbol address <= addr
        self.symbols
            .range(..=addr)
            .next_back()
            .map(|(_, sym)| sym)
            .filter(|sym| addr < sym.addr + sym.size)
    }

    /// Look up the symbol name for an address, returning "unknown"
    /// if not found.
    pub fn name_at(&self, addr: u64) -> &str {
        self.symbol_at(addr)
            .map(|s| s.name.as_str())
            .unwrap_or("[unknown]")
    }

    /// Convert a virtual address to a file offset relative to the
    /// load address.
    pub fn addr_to_offset(&self, addr: u64) -> u64 {
        addr.wrapping_sub(self.load_addr)
    }
}
