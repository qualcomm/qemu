// SPDX-License-Identifier: BSD-3-Clause-Clear
// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//
// Opcode-based instruction classifier for tier 2 call graph tracking
// and tier 3 resource/coprocessor classification.
// Uses bitmask tables on raw instruction bytes instead of disassembly
// text, eliminating string allocation and false-positive matching.

/// Target architecture for opcode classification.
#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum Arch {
    Hexagon,
    Aarch64,
    Riscv,
    X86,
    Unknown,
}

impl Arch {
    /// Resolve architecture from QEMU's target name string.
    pub fn from_name(name: &str) -> Self {
        match name {
            "hexagon" => Arch::Hexagon,
            "aarch64" | "aarch64_be" => Arch::Aarch64,
            "riscv32" | "riscv64" => Arch::Riscv,
            "x86_64" | "i386" => Arch::X86,
            _ => Arch::Unknown,
        }
    }
}

/// Instruction classification for tier 2 call/branch tracking.
#[derive(Debug, PartialEq, Eq)]
pub enum InsnClass {
    /// Direct call or unconditional jump with a computable target.
    Direct { target: u64 },
    /// Indirect call, jump, or return via register.
    Indirect,
    /// Not relevant to tier 2.
    Other,
}

/// Classify a guest instruction from raw bytes.
///
/// `bytes`: raw instruction bytes from `qemu_plugin_insn_data()`
/// `pc`: virtual address of the instruction
/// `insn_size`: total instruction size from `qemu_plugin_insn_size()`
pub fn classify(arch: Arch, bytes: &[u8], pc: u64, insn_size: usize) -> InsnClass {
    match arch {
        Arch::Hexagon => {
            if bytes.len() < 4 {
                return InsnClass::Other;
            }
            let insn = u32::from_le_bytes([bytes[0], bytes[1], bytes[2], bytes[3]]);
            classify_hexagon(insn, pc)
        }
        Arch::Aarch64 => {
            if bytes.len() < 4 {
                return InsnClass::Other;
            }
            let insn = u32::from_le_bytes([bytes[0], bytes[1], bytes[2], bytes[3]]);
            classify_aarch64(insn, pc)
        }
        Arch::Riscv => classify_riscv(bytes, pc),
        Arch::X86 => classify_x86(bytes, pc, insn_size),
        Arch::Unknown => InsnClass::Other,
    }
}

// --- Hexagon (32-bit fixed width, little-endian) ---
//
// Masks intentionally ignore parse bits [15:14] and endloop bits [1:0]
// so that instructions match regardless of packet position.

fn classify_hexagon(insn: u32, pc: u64) -> InsnClass {
    // Parse bits [15:14] == 00 indicates a duplex (two sub-instructions
    // packed into one 32-bit word).  Duplex encoding uses bits [31:29]
    // as the duplex class, not a regular opcode, so the masks below
    // would produce false matches (e.g. duplex class 2 overlaps with
    // J2_call/J2_jump bit patterns).  Duplex sub-instructions are
    // limited to simple ALU, loads, stores, and returns — no calls
    // or unconditional jumps that tier 2 needs to track.
    if (insn >> 14) & 3 == 0 {
        return InsnClass::Other;
    }
    // All J-class register-indirect branch/call instructions live in
    // the JUMPR_MISC subclass: bits[31:28]=ICLASS_J (0101) and
    // bits[27:26]=00.  This covers all 15 variants:
    //   0000: J2_callr, J2_callrh
    //   0001: J2_callrt, J2_callrf
    //   0010: J2_jumpr, J2_jumprh, J4_hintjumpr
    //   0011: J2_jumprt/f, jumprt/fpt, jumprtnew/fnew, jumprtnewpt/fnewpt
    // Subclasses 01xx (trap, pause, icinva, isync) are not branches.
    if (insn & 0xFC000000) == 0x50000000 {
        return InsnClass::Indirect;
    }
    // J2_call: direct call (r22:2 immediate)
    if (insn & 0xFE000000) == 0x5A000000 {
        return InsnClass::Direct {
            target: hex_r22_target(insn, pc),
        };
    }
    // J2_jump: unconditional direct jump (r22:2 immediate)
    if (insn & 0xFE000000) == 0x58000000 {
        return InsnClass::Direct {
            target: hex_r22_target(insn, pc),
        };
    }
    // J2_callt/callf: conditional direct call (r15:2 immediate)
    if (insn & 0xFF000000) == 0x5D000000 {
        return InsnClass::Direct {
            target: hex_r15_target(insn, pc),
        };
    }
    InsnClass::Other
}

/// Extract target from Hexagon r22:2 immediate (J2_call, J2_jump).
/// 22 bits scattered at insn[24, 23:16, 13:8, 7:1], sign-extended,
/// shifted left 2, added to PC.
fn hex_r22_target(insn: u32, pc: u64) -> u64 {
    let bit24 = (insn >> 24) & 1;
    let bits23_16 = (insn >> 16) & 0xFF;
    let bits13_8 = (insn >> 8) & 0x3F;
    let bits7_1 = (insn >> 1) & 0x7F;
    let imm22 = (bit24 << 21) | (bits23_16 << 13) | (bits13_8 << 7) | bits7_1;
    // Sign-extend from 22 bits
    let offset = (((imm22 as i32) << 10) >> 10) as i64;
    (pc as i64).wrapping_add(offset << 2) as u64
}

/// Extract target from Hexagon r15:2 immediate (J2_callt/callf).
/// 15 bits scattered at insn[23:22, 20:16, 13, 7:1], sign-extended,
/// shifted left 2, added to PC.
fn hex_r15_target(insn: u32, pc: u64) -> u64 {
    let bits23_22 = (insn >> 22) & 0x3;
    let bits20_16 = (insn >> 16) & 0x1F;
    let bit13 = (insn >> 13) & 1;
    let bits7_1 = (insn >> 1) & 0x7F;
    let imm15 = (bits23_22 << 13) | (bits20_16 << 8) | (bit13 << 7) | bits7_1;
    // Sign-extend from 15 bits
    let offset = (((imm15 as i32) << 17) >> 17) as i64;
    (pc as i64).wrapping_add(offset << 2) as u64
}

// --- AArch64 (32-bit fixed width, little-endian) ---

fn classify_aarch64(insn: u32, pc: u64) -> InsnClass {
    // BL imm26 (call)
    if (insn & 0xFC000000) == 0x94000000 {
        return InsnClass::Direct {
            target: aarch64_imm26_target(insn, pc),
        };
    }
    // B imm26 (unconditional branch)
    if (insn & 0xFC000000) == 0x14000000 {
        return InsnClass::Direct {
            target: aarch64_imm26_target(insn, pc),
        };
    }
    // BLR Xn (indirect call)
    if (insn & 0xFFFFFC1F) == 0xD63F0000 {
        return InsnClass::Indirect;
    }
    // BR Xn (indirect branch)
    if (insn & 0xFFFFFC1F) == 0xD61F0000 {
        return InsnClass::Indirect;
    }
    // RET
    if (insn & 0xFFFFFC1F) == 0xD65F0000 {
        return InsnClass::Indirect;
    }
    InsnClass::Other
}

/// Extract target from AArch64 imm26 field (B/BL).
/// pc + sign_extend(imm26) << 2
fn aarch64_imm26_target(insn: u32, pc: u64) -> u64 {
    let imm26 = insn & 0x03FFFFFF;
    let offset = (((imm26 as i32) << 6) >> 6) as i64;
    (pc as i64).wrapping_add(offset << 2) as u64
}

// --- RISC-V (variable: 32-bit base + 16-bit compressed) ---

fn classify_riscv(bytes: &[u8], pc: u64) -> InsnClass {
    if bytes.len() < 2 {
        return InsnClass::Other;
    }
    // 32-bit instruction: bits[1:0] == 0b11
    if (bytes[0] & 0x03) == 0x03 {
        if bytes.len() < 4 {
            return InsnClass::Other;
        }
        let insn = u32::from_le_bytes([bytes[0], bytes[1], bytes[2], bytes[3]]);
        classify_riscv32(insn, pc)
    } else {
        let insn = u16::from_le_bytes([bytes[0], bytes[1]]);
        classify_riscv16(insn, pc)
    }
}

fn classify_riscv32(insn: u32, pc: u64) -> InsnClass {
    let opcode = insn & 0x7F;
    // JAL (opcode 0x6F) — direct jump/call
    if opcode == 0x6F {
        return InsnClass::Direct {
            target: riscv_jal_target(insn, pc),
        };
    }
    // JALR (opcode 0x67) — indirect jump/call/return
    if opcode == 0x67 {
        return InsnClass::Indirect;
    }
    InsnClass::Other
}

fn classify_riscv16(insn: u16, pc: u64) -> InsnClass {
    // C.J: funct3=101, op=01
    if (insn & 0xE003) == 0xA001 {
        return InsnClass::Direct {
            target: riscv_cj_target(insn, pc),
        };
    }
    // C.JAL (RV32 only): funct3=001, op=01
    if (insn & 0xE003) == 0x2001 {
        return InsnClass::Direct {
            target: riscv_cj_target(insn, pc),
        };
    }
    // C.JR: funct4=1000, rs2=0, op=10
    if (insn & 0xF07F) == 0x8002 {
        return InsnClass::Indirect;
    }
    // C.JALR: funct4=1001, rs2=0, op=10
    if (insn & 0xF07F) == 0x9002 {
        return InsnClass::Indirect;
    }
    InsnClass::Other
}

/// Extract target from RISC-V J-type immediate (JAL).
/// imm[20|10:1|11|19:12] from insn[31|30:21|20|19:12]
fn riscv_jal_target(insn: u32, pc: u64) -> u64 {
    let imm20 = ((insn >> 31) & 1) << 20;
    let imm10_1 = ((insn >> 21) & 0x3FF) << 1;
    let imm11 = ((insn >> 20) & 1) << 11;
    let imm19_12 = ((insn >> 12) & 0xFF) << 12;
    let imm = imm20 | imm19_12 | imm11 | imm10_1;
    // Sign-extend from 21 bits (bit 20 is sign)
    let offset = (((imm as i32) << 11) >> 11) as i64;
    (pc as i64).wrapping_add(offset) as u64
}

/// Extract target from RISC-V compressed J-type immediate (C.J, C.JAL).
/// offset[11|4|9:8|10|6|7|3:1|5] from insn[12:2]
fn riscv_cj_target(insn: u16, pc: u64) -> u64 {
    let w = insn as u32;
    let offset = (((w >> 12) & 1) << 11)
        | (((w >> 11) & 1) << 4)
        | (((w >> 9) & 3) << 8)
        | (((w >> 8) & 1) << 10)
        | (((w >> 7) & 1) << 6)
        | (((w >> 6) & 1) << 7)
        | (((w >> 3) & 7) << 1)
        | (((w >> 2) & 1) << 5);
    // Sign-extend from 12 bits (bit 11 is sign)
    let offset = (((offset as i32) << 20) >> 20) as i64;
    (pc as i64).wrapping_add(offset) as u64
}

// --- x86/x86-64 (variable width) ---

fn classify_x86(bytes: &[u8], pc: u64, insn_size: usize) -> InsnClass {
    let mut i = 0;
    // Skip legacy prefixes and REX
    while i < bytes.len() {
        match bytes[i] {
            0x66 | 0x67 | 0x26 | 0x2E | 0x36 | 0x3E | 0x64 | 0x65 | 0xF0 | 0xF2 | 0xF3 => {
                i += 1
            }
            0x40..=0x4F => i += 1, // REX prefix
            _ => break,
        }
    }
    if i >= bytes.len() {
        return InsnClass::Other;
    }

    let next_pc = pc + insn_size as u64;

    match bytes[i] {
        // CALL rel32
        0xE8 if i + 5 <= bytes.len() => {
            let rel = i32::from_le_bytes([
                bytes[i + 1],
                bytes[i + 2],
                bytes[i + 3],
                bytes[i + 4],
            ]);
            InsnClass::Direct {
                target: next_pc.wrapping_add(rel as i64 as u64),
            }
        }
        // JMP rel32
        0xE9 if i + 5 <= bytes.len() => {
            let rel = i32::from_le_bytes([
                bytes[i + 1],
                bytes[i + 2],
                bytes[i + 3],
                bytes[i + 4],
            ]);
            InsnClass::Direct {
                target: next_pc.wrapping_add(rel as i64 as u64),
            }
        }
        // JMP rel8
        0xEB if i + 2 <= bytes.len() => {
            let rel = bytes[i + 1] as i8;
            InsnClass::Direct {
                target: next_pc.wrapping_add(rel as i64 as u64),
            }
        }
        // FF /2 = CALL r/m, FF /4 = JMP r/m
        0xFF if i + 2 <= bytes.len() => {
            let reg = (bytes[i + 1] >> 3) & 7;
            match reg {
                2 | 4 => InsnClass::Indirect,
                _ => InsnClass::Other,
            }
        }
        _ => InsnClass::Other,
    }
}

// --- Tier 3: Coprocessor resource classification ---

/// Coprocessor flag bitmask values for tier 3 resource tracking.
pub const COPROC_HVX: u32 = 0x1;
pub const COPROC_HMX: u32 = 0x2;
pub const COPROC_FP: u32 = 0x4;
pub const COPROC_ATOMIC: u32 = 0x8;
pub const COPROC_SIMD: u32 = 0x10;

/// Classify a guest instruction's coprocessor resource usage from raw bytes.
///
/// Returns a bitmask of `COPROC_*` flags indicating which coprocessor
/// resources the instruction uses.
pub fn classify_resources(arch: Arch, bytes: &[u8]) -> u32 {
    match arch {
        Arch::Hexagon => {
            if bytes.len() < 4 {
                return 0;
            }
            let insn = u32::from_le_bytes([bytes[0], bytes[1], bytes[2], bytes[3]]);
            classify_resources_hexagon(insn)
        }
        Arch::Aarch64 => {
            if bytes.len() < 4 {
                return 0;
            }
            let insn = u32::from_le_bytes([bytes[0], bytes[1], bytes[2], bytes[3]]);
            classify_resources_aarch64(insn)
        }
        Arch::Riscv => classify_resources_riscv(bytes),
        Arch::X86 => classify_resources_x86(bytes),
        Arch::Unknown => 0,
    }
}

// --- Hexagon resource classification ---

fn classify_resources_hexagon(insn: u32) -> u32 {
    // Duplex (PP=00, bits[15:14]=00) — skip, no coprocessor usage.
    if (insn >> 14) & 3 == 0 {
        return 0;
    }

    let mut flags = 0u32;

    // HVX: ICLASS_CJ with bit[27]=1 (vector ALU)
    if (insn & 0xF8000000) == 0x18000000 {
        flags |= COPROC_HVX;
    }
    // HVX: ICLASS_NCJ with bit[27]=1 (vector memory)
    if (insn & 0xF8000000) == 0x28000000 {
        flags |= COPROC_HVX;
    }

    // HMX group 1: bits[31:21]=10010010000
    if (insn & 0xFFE00000) == 0x92000000 {
        flags |= COPROC_HMX;
    }
    // HMX group 2: bits[31:21]=10100110111
    if (insn & 0xFFE00000) == 0xA6E00000 {
        flags |= COPROC_HMX;
    }

    // FP: ICLASS_M (bits[31:28]=1110)
    // SF basic: 0xEB______
    if (insn & 0xFF000000) == 0xEB000000 {
        flags |= COPROC_FP;
    }
    // DF basic: VMIN2=11 (bits[6:5]) + SHFT=0 (bit[23])
    if (insn & 0xFF800060) == 0xE8000060 {
        flags |= COPROC_FP;
    }
    // DF mul parts: VMIN2=11 (bits[6:5]) + SHFT=0 (bit[23])
    if (insn & 0xFF800060) == 0xEA000060 {
        flags |= COPROC_FP;
    }
    // SF FMA: sffma, sffms, sffma_lib, sffms_lib, sffma_sc
    if (insn & 0xFF800000) == 0xEF800000 {
        flags |= COPROC_FP;
    }

    // FP: ICLASS_S2op (bits[31:28]=1000)
    // DF<->D conversions
    if (insn & 0xFFE00000) == 0x80E00000 {
        flags |= COPROC_FP;
    }
    // SF<->DF conversions
    if (insn & 0xFF800000) == 0x84800000 {
        flags |= COPROC_FP;
    }
    // DF->SF/W conversions
    if (insn & 0xFF0000E0) == 0x88000020 {
        flags |= COPROC_FP;
    }
    // SF<->W conversions, sfinvsqrta
    if (insn & 0xFF000000) == 0x8B000000 {
        flags |= COPROC_FP;
    }
    // sfclass
    if (insn & 0xFFE00000) == 0x85E00000 {
        flags |= COPROC_FP;
    }

    // FP: ICLASS_ALU64 (bits[31:28]=1101)
    // sfimm_p, sfimm_n
    if (insn & 0xFF000000) == 0xD6000000 {
        flags |= COPROC_FP;
    }
    // dfimm_p, dfimm_n
    if (insn & 0xFF000000) == 0xD9000000 {
        flags |= COPROC_FP;
    }
    // dfclass
    if (insn & 0xFF000000) == 0xDC000000 {
        flags |= COPROC_FP;
    }

    flags
}

// --- AArch64 resource classification ---

fn classify_resources_aarch64(insn: u32) -> u32 {
    let mut flags = 0u32;

    // Advanced SIMD + scalar FP: bits[27:25]=111
    if (insn & 0x0E000000) == 0x0E000000 {
        flags |= COPROC_SIMD | COPROC_FP;
    }

    // SVE/SME: bits[28:25]=0010
    if (insn & 0x1E000000) == 0x04000000 {
        flags |= COPROC_SIMD;
    }

    flags
}

// --- RISC-V resource classification ---

fn classify_resources_riscv(bytes: &[u8]) -> u32 {
    if bytes.len() < 2 {
        return 0;
    }

    // 32-bit instruction: bits[1:0] == 0b11
    if (bytes[0] & 0x03) == 0x03 {
        if bytes.len() < 4 {
            return 0;
        }
        let insn = u32::from_le_bytes([bytes[0], bytes[1], bytes[2], bytes[3]]);
        let opcode = insn & 0x7F;
        let mut flags = 0u32;

        // OP-V: vector ALU
        if opcode == 0x57 {
            flags |= COPROC_SIMD;
        }
        // LOAD-FP (0x07): scalar FP loads + vector loads
        if opcode == 0x07 {
            flags |= COPROC_FP;
            // Vector loads: width (bits[14:12]) >= 5
            let width = (insn >> 12) & 0x7;
            if width >= 5 {
                flags |= COPROC_SIMD;
            }
        }
        // STORE-FP (0x27): scalar FP stores + vector stores
        if opcode == 0x27 {
            flags |= COPROC_FP;
            // Vector stores: width (bits[14:12]) >= 5
            let width = (insn >> 12) & 0x7;
            if width >= 5 {
                flags |= COPROC_SIMD;
            }
        }
        // OP-FP: scalar FP ALU
        if opcode == 0x53 {
            flags |= COPROC_FP;
        }
        // FMADD
        if opcode == 0x43 {
            flags |= COPROC_FP;
        }
        // FMSUB
        if opcode == 0x47 {
            flags |= COPROC_FP;
        }
        // FNMSUB
        if opcode == 0x4B {
            flags |= COPROC_FP;
        }
        // FNMADD
        if opcode == 0x4F {
            flags |= COPROC_FP;
        }

        flags
    } else {
        // 16-bit compressed: skip — rare for coprocessor classification
        0
    }
}

// --- x86 resource classification ---

fn classify_resources_x86(bytes: &[u8]) -> u32 {
    let mut i = 0;
    // Skip legacy prefixes and REX (same as classify_x86)
    while i < bytes.len() {
        match bytes[i] {
            0x66 | 0x67 | 0x26 | 0x2E | 0x36 | 0x3E | 0x64 | 0x65
            | 0xF0 | 0xF2 | 0xF3 => i += 1,
            0x40..=0x4F => i += 1, // REX prefix
            _ => break,
        }
    }
    if i >= bytes.len() {
        return 0;
    }

    match bytes[i] {
        // VEX 2-byte prefix: AVX
        0xC5 => COPROC_SIMD,
        // VEX 3-byte prefix: AVX
        0xC4 => COPROC_SIMD,
        // EVEX prefix: AVX-512
        0x62 => COPROC_SIMD,
        // x87 FP: D8-DF
        0xD8..=0xDF => COPROC_FP,
        _ => 0,
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    // --- Arch ---

    #[test]
    fn arch_from_name() {
        assert_eq!(Arch::from_name("hexagon"), Arch::Hexagon);
        assert_eq!(Arch::from_name("aarch64"), Arch::Aarch64);
        assert_eq!(Arch::from_name("aarch64_be"), Arch::Aarch64);
        assert_eq!(Arch::from_name("riscv32"), Arch::Riscv);
        assert_eq!(Arch::from_name("riscv64"), Arch::Riscv);
        assert_eq!(Arch::from_name("x86_64"), Arch::X86);
        assert_eq!(Arch::from_name("i386"), Arch::X86);
        assert_eq!(Arch::from_name("mips"), Arch::Unknown);
    }

    #[test]
    fn unknown_arch_returns_other() {
        let bytes = [0u8; 4];
        assert_eq!(classify(Arch::Unknown, &bytes, 0, 4), InsnClass::Other);
    }

    // --- Hexagon ---

    // All regular Hexagon instruction tests use PP=11 (bits [15:14] = 11,
    // set via | 0xC000) to indicate a valid non-duplex instruction word.
    // Parse bits [15:14] are not part of any immediate field.

    #[test]
    fn hexagon_j2_call_forward() {
        // J2_call PC+4: imm22=1, offset=4
        // bit24=0, bits23:16=0, bits13:8=0, bits7:1=1
        let bytes = (0x5A000002u32 | 0xC000).to_le_bytes();
        assert_eq!(
            classify(Arch::Hexagon, &bytes, 0x1000, 4),
            InsnClass::Direct { target: 0x1004 }
        );
    }

    #[test]
    fn hexagon_j2_call_backward() {
        // J2_call PC-8: imm22 = -2 (offset -8 >> 2)
        // bit24=1, bits23:16=0xFF, bits13:8=0x3F, bits7:1=0x7E
        //   insn[31:24] = 0x5B (J2_call + bit24=1)
        //   insn[23:16] = 0xFF
        //   insn[15:14] = 11 (PP, non-duplex)
        //   insn[13:8]  = 0x3F
        //   insn[7:1]   = 0x7E (1111110)
        //   insn[0]     = 0
        let insn: u32 = 0x5BFF3FFC | 0xC000;
        let bytes = insn.to_le_bytes();
        assert_eq!(
            classify(Arch::Hexagon, &bytes, 0x1000, 4),
            InsnClass::Direct { target: 0x0FF8 }
        );
    }

    #[test]
    fn hexagon_j2_jump() {
        // J2_jump PC+4: same immediate encoding as call
        let bytes = (0x58000002u32 | 0xC000).to_le_bytes();
        assert_eq!(
            classify(Arch::Hexagon, &bytes, 0x1000, 4),
            InsnClass::Direct { target: 0x1004 }
        );
    }

    #[test]
    fn hexagon_indirect_all_subclasses() {
        // All J-class JUMPR_MISC subclasses (bits[27:24] = 0000..0011)
        // must be classified as Indirect.  One representative per
        // subclass, with PP=11.
        let cases: &[(u32, &str)] = &[
            (0x50A00000, "J2_callr"),    // 0000: unconditional indirect call
            (0x50C00000, "J2_callrh"),   // 0000: hinted indirect call
            (0x51000000, "J2_callrt"),   // 0001: predicated indirect call (true)
            (0x51200000, "J2_callrf"),   // 0001: predicated indirect call (false)
            (0x52800000, "J2_jumpr"),    // 0010: unconditional indirect jump
            (0x52C00000, "J2_jumprh"),   // 0010: hinted indirect jump
            (0x52A00000, "J4_hintjumpr"),// 0010: hint indirect jump
            (0x53400000, "J2_jumprt"),   // 0011: predicated indirect jump (true)
            (0x53600000, "J2_jumprf"),   // 0011: predicated indirect jump (false)
        ];
        for &(base, name) in cases {
            let bytes = (base | 0xC000).to_le_bytes();
            assert_eq!(
                classify(Arch::Hexagon, &bytes, 0x1000, 4),
                InsnClass::Indirect,
                "{} not classified as Indirect",
                name
            );
        }
    }

    #[test]
    fn hexagon_trap_not_indirect() {
        // J2_trap0 is in ICLASS_J subclass 0100 (bits[27:26]=01),
        // must NOT be classified as Indirect.
        let bytes = (0x54000000u32 | 0xC000).to_le_bytes();
        assert_eq!(
            classify(Arch::Hexagon, &bytes, 0x1000, 4),
            InsnClass::Other
        );
    }

    #[test]
    fn hexagon_j2_callt() {
        // J2_callt PC+8: r15:2 imm, offset=8 -> imm15=2
        // bits23:22=0, bits20:16=0, bit13=0, bits7:1=2
        let bytes = (0x5D000004u32 | 0xC000).to_le_bytes();
        assert_eq!(
            classify(Arch::Hexagon, &bytes, 0x1000, 4),
            InsnClass::Direct { target: 0x1008 }
        );
    }

    #[test]
    fn hexagon_non_branch() {
        // An instruction that doesn't match any call/jump pattern
        let bytes = (0x70000000u32 | 0xC000).to_le_bytes();
        assert_eq!(
            classify(Arch::Hexagon, &bytes, 0x1000, 4),
            InsnClass::Other
        );
    }

    #[test]
    fn hexagon_duplex_not_misclassified() {
        // A duplex (PP=00) whose upper bits match J2_call.
        // bits [31:25] = 0101101 (same as J2_call), but PP=00
        // makes this a duplex with completely different encoding.
        let bytes = 0x5A000000u32.to_le_bytes();
        assert_eq!(
            classify(Arch::Hexagon, &bytes, 0x1000, 4),
            InsnClass::Other
        );
    }

    #[test]
    fn hexagon_duplex_jump_pattern_rejected() {
        // Duplex whose upper bits match J2_jump (0x58xxxxxx), PP=00.
        let bytes = 0x58000000u32.to_le_bytes();
        assert_eq!(
            classify(Arch::Hexagon, &bytes, 0x1000, 4),
            InsnClass::Other
        );
    }

    #[test]
    fn hexagon_j2_call_all_parse_bits() {
        // J2_call must be detected at every non-duplex packet position.
        // PP=01 (mid-packet), PP=10 (mid-packet), PP=11 (end-of-packet).
        let base: u32 = 0x5A000002; // J2_call PC+4, imm22=1
        for &pp in &[0x4000u32, 0x8000, 0xC000] {
            let bytes = (base | pp).to_le_bytes();
            assert_eq!(
                classify(Arch::Hexagon, &bytes, 0x1000, 4),
                InsnClass::Direct { target: 0x1004 },
                "J2_call not detected with PP={:#06x}",
                pp
            );
        }
    }

    #[test]
    fn hexagon_j2_jump_all_parse_bits() {
        // J2_jump must be detected at every non-duplex packet position.
        let base: u32 = 0x58000002; // J2_jump PC+4
        for &pp in &[0x4000u32, 0x8000, 0xC000] {
            let bytes = (base | pp).to_le_bytes();
            assert_eq!(
                classify(Arch::Hexagon, &bytes, 0x1000, 4),
                InsnClass::Direct { target: 0x1004 },
                "J2_jump not detected with PP={:#06x}",
                pp
            );
        }
    }

    #[test]
    fn hexagon_indirect_all_parse_bits() {
        // Indirect instructions must be detected at every non-duplex
        // packet position.  Test J2_callr and J2_jumprt as representatives.
        for &base in &[0x50A00000u32, 0x53400000] {
            for &pp in &[0x4000u32, 0x8000, 0xC000] {
                let bytes = (base | pp).to_le_bytes();
                assert_eq!(
                    classify(Arch::Hexagon, &bytes, 0x1000, 4),
                    InsnClass::Indirect,
                    "indirect not detected: base={:#010x} PP={:#06x}",
                    base,
                    pp
                );
            }
        }
    }

    // --- AArch64 ---

    #[test]
    fn aarch64_bl() {
        // BL #0x100: imm26 = 0x100>>2 = 0x40
        // insn = 0x94000040
        let bytes = 0x94000040u32.to_le_bytes();
        assert_eq!(
            classify(Arch::Aarch64, &bytes, 0x1000, 4),
            InsnClass::Direct { target: 0x1100 }
        );
    }

    #[test]
    fn aarch64_b() {
        // B #0x100: imm26 = 0x40
        // insn = 0x14000040
        let bytes = 0x14000040u32.to_le_bytes();
        assert_eq!(
            classify(Arch::Aarch64, &bytes, 0x1000, 4),
            InsnClass::Direct { target: 0x1100 }
        );
    }

    #[test]
    fn aarch64_bl_backward() {
        // BL #-0x100: imm26 = sign_extend(-0x40) in 26 bits
        // -0x40 in 26 bits = 0x3FFFFC0
        // insn = 0x97FFFFC0
        let bytes = 0x97FFFFC0u32.to_le_bytes();
        assert_eq!(
            classify(Arch::Aarch64, &bytes, 0x1000, 4),
            InsnClass::Direct { target: 0x0F00 }
        );
    }

    #[test]
    fn aarch64_blr() {
        // BLR X0: insn = 0xD63F0000
        let bytes = 0xD63F0000u32.to_le_bytes();
        assert_eq!(
            classify(Arch::Aarch64, &bytes, 0x1000, 4),
            InsnClass::Indirect
        );
    }

    #[test]
    fn aarch64_br() {
        // BR X0: insn = 0xD61F0000
        let bytes = 0xD61F0000u32.to_le_bytes();
        assert_eq!(
            classify(Arch::Aarch64, &bytes, 0x1000, 4),
            InsnClass::Indirect
        );
    }

    #[test]
    fn aarch64_ret() {
        // RET (X30 default): insn = 0xD65F03C0
        // mask 0xFFFFFC1F: 0xD65F03C0 & 0xFFFFFC1F = 0xD65F0000
        let bytes = 0xD65F03C0u32.to_le_bytes();
        assert_eq!(
            classify(Arch::Aarch64, &bytes, 0x1000, 4),
            InsnClass::Indirect
        );
    }

    #[test]
    fn aarch64_non_branch() {
        // MOV X0, #0: insn = 0xD2800000
        let bytes = 0xD2800000u32.to_le_bytes();
        assert_eq!(
            classify(Arch::Aarch64, &bytes, 0x1000, 4),
            InsnClass::Other
        );
    }

    // --- RISC-V ---

    #[test]
    fn riscv_jal() {
        // JAL ra, +0x100
        // opcode=0x6F, rd=x1(ra)
        // J-type imm for offset 0x100:
        //   bit 8 of offset → insn[28]
        // insn = 0x100000EF
        let bytes = 0x100000EFu32.to_le_bytes();
        assert_eq!(
            classify(Arch::Riscv, &bytes, 0x1000, 4),
            InsnClass::Direct { target: 0x1100 }
        );
    }

    #[test]
    fn riscv_jal_backward() {
        // JAL ra, -4
        // offset = -4: imm[20:1] with sign bit
        // -4 in 21 bits = 0x1FFFFC
        // insn[31]=1, insn[30:21]=imm[10:1]=111111_1110,
        // insn[20]=imm[11]=1, insn[19:12]=imm[19:12]=1111_1111
        // insn = 0xFFDFF0EF
        let bytes = 0xFFDFF0EFu32.to_le_bytes();
        assert_eq!(
            classify(Arch::Riscv, &bytes, 0x1000, 4),
            InsnClass::Direct { target: 0x0FFC }
        );
    }

    #[test]
    fn riscv_jalr() {
        // JALR ra, rs1, 0: opcode=0x67
        // insn = 0x000080E7 (JALR ra, x1, 0)
        let bytes = 0x000080E7u32.to_le_bytes();
        assert_eq!(
            classify(Arch::Riscv, &bytes, 0x1000, 4),
            InsnClass::Indirect
        );
    }

    #[test]
    fn riscv_c_j() {
        // C.J offset +0: insn = 0xA001
        // funct3=101, op=01, all offset bits zero
        let bytes = 0xA001u16.to_le_bytes();
        assert_eq!(
            classify(Arch::Riscv, &bytes, 0x1000, 2),
            InsnClass::Direct { target: 0x1000 }
        );
    }

    #[test]
    fn riscv_c_jr() {
        // C.JR rs1=x1: funct4=1000, rs1=00001, rs2=0, op=10
        // insn[15:12]=1000, insn[11:7]=00001, insn[6:2]=00000, insn[1:0]=10
        // = 0x8082
        let bytes = 0x8082u16.to_le_bytes();
        assert_eq!(
            classify(Arch::Riscv, &bytes, 0x1000, 2),
            InsnClass::Indirect
        );
    }

    #[test]
    fn riscv_c_jalr() {
        // C.JALR rs1=x1: funct4=1001, rs1=00001, rs2=0, op=10
        // = 0x9082
        let bytes = 0x9082u16.to_le_bytes();
        assert_eq!(
            classify(Arch::Riscv, &bytes, 0x1000, 2),
            InsnClass::Indirect
        );
    }

    #[test]
    fn riscv_non_branch() {
        // ADDI x0, x0, 0 (NOP): insn = 0x00000013
        let bytes = 0x00000013u32.to_le_bytes();
        assert_eq!(
            classify(Arch::Riscv, &bytes, 0x1000, 4),
            InsnClass::Other
        );
    }

    // --- x86 ---

    #[test]
    fn x86_call_rel32() {
        // CALL rel32: E8 FB 00 00 00
        // pc=0x400000, insn_size=5, rel32=0xFB
        // target = 0x400005 + 0xFB = 0x400100
        let bytes = [0xE8, 0xFB, 0x00, 0x00, 0x00];
        assert_eq!(
            classify(Arch::X86, &bytes, 0x400000, 5),
            InsnClass::Direct {
                target: 0x400100
            }
        );
    }

    #[test]
    fn x86_jmp_rel32() {
        // JMP rel32: E9 FB 00 00 00
        let bytes = [0xE9, 0xFB, 0x00, 0x00, 0x00];
        assert_eq!(
            classify(Arch::X86, &bytes, 0x400000, 5),
            InsnClass::Direct {
                target: 0x400100
            }
        );
    }

    #[test]
    fn x86_jmp_rel8() {
        // JMP rel8: EB 0E
        // pc=0x400000, insn_size=2
        // target = 0x400002 + 0x0E = 0x400010
        let bytes = [0xEB, 0x0E];
        assert_eq!(
            classify(Arch::X86, &bytes, 0x400000, 2),
            InsnClass::Direct {
                target: 0x400010
            }
        );
    }

    #[test]
    fn x86_call_indirect() {
        // CALL rax: FF D0 (ModRM: mod=11, reg=2, rm=0)
        let bytes = [0xFF, 0xD0];
        assert_eq!(
            classify(Arch::X86, &bytes, 0x400000, 2),
            InsnClass::Indirect
        );
    }

    #[test]
    fn x86_jmp_indirect() {
        // JMP rax: FF E0 (ModRM: mod=11, reg=4, rm=0)
        let bytes = [0xFF, 0xE0];
        assert_eq!(
            classify(Arch::X86, &bytes, 0x400000, 2),
            InsnClass::Indirect
        );
    }

    #[test]
    fn x86_call_with_rex() {
        // REX.W CALL rel32: 48 E8 F5 00 00 00
        // pc=0x400000, insn_size=6
        // target = 0x400006 + 0xF5 = 0x4000FB
        let bytes = [0x48, 0xE8, 0xF5, 0x00, 0x00, 0x00];
        assert_eq!(
            classify(Arch::X86, &bytes, 0x400000, 6),
            InsnClass::Direct {
                target: 0x4000FB
            }
        );
    }

    #[test]
    fn x86_call_rel32_backward() {
        // CALL rel32 backward: E8 F6 FF FF FF
        // pc=0x400000, insn_size=5, rel32=-10 (0xFFFFFFF6)
        // target = 0x400005 + (-10) = 0x3FFFFB
        let bytes = [0xE8, 0xF6, 0xFF, 0xFF, 0xFF];
        assert_eq!(
            classify(Arch::X86, &bytes, 0x400000, 5),
            InsnClass::Direct {
                target: 0x3FFFFB
            }
        );
    }

    #[test]
    fn x86_non_branch() {
        // NOP: 90
        let bytes = [0x90];
        assert_eq!(
            classify(Arch::X86, &bytes, 0x400000, 1),
            InsnClass::Other
        );
    }

    #[test]
    fn too_short_bytes() {
        // Less than minimum for each arch
        assert_eq!(
            classify(Arch::Hexagon, &[0, 0], 0, 2),
            InsnClass::Other
        );
        assert_eq!(
            classify(Arch::Aarch64, &[0, 0], 0, 2),
            InsnClass::Other
        );
        assert_eq!(classify(Arch::Riscv, &[0], 0, 1), InsnClass::Other);
        assert_eq!(classify(Arch::X86, &[], 0, 0), InsnClass::Other);
    }

    // --- Resource classification: Hexagon ---

    #[test]
    fn hex_res_hvx_alu() {
        // HVX vector ALU: ICLASS_CJ(0001) + bit[27]=1 → 0x18______
        // PP=11 (non-duplex)
        let bytes = (0x1800C000u32).to_le_bytes();
        assert_eq!(classify_resources(Arch::Hexagon, &bytes), COPROC_HVX);
    }

    #[test]
    fn hex_res_hvx_vmem() {
        // HVX vector memory: ICLASS_NCJ(0010) + bit[27]=1 → 0x28______
        let bytes = (0x2800C000u32).to_le_bytes();
        assert_eq!(classify_resources(Arch::Hexagon, &bytes), COPROC_HVX);
    }

    #[test]
    fn hex_res_hmx_group1() {
        // HMX group 1: bits[31:21]=10010010000 → 0x92000000
        let bytes = (0x9200C000u32).to_le_bytes();
        assert_eq!(classify_resources(Arch::Hexagon, &bytes), COPROC_HMX);
    }

    #[test]
    fn hex_res_hmx_group2() {
        // HMX group 2: bits[31:21]=10100110111 → 0xA6E00000
        let bytes = (0xA6E0C000u32).to_le_bytes();
        assert_eq!(classify_resources(Arch::Hexagon, &bytes), COPROC_HMX);
    }

    #[test]
    fn hex_res_fp_sf_basic() {
        // SF basic: 0xEB______ (ICLASS_M)
        let bytes = (0xEB00C000u32).to_le_bytes();
        assert_eq!(classify_resources(Arch::Hexagon, &bytes), COPROC_FP);
    }

    #[test]
    fn hex_res_fp_df_basic() {
        // DF basic: mask 0xFF800060, value 0xE8000060
        // bits[6:5]=11 (VMIN2), bit[23]=0 (SHFT)
        let bytes = (0xE800C060u32).to_le_bytes();
        assert_eq!(classify_resources(Arch::Hexagon, &bytes), COPROC_FP);
    }

    #[test]
    fn hex_res_fp_sf_fma() {
        // SF FMA: mask 0xFF800000, value 0xEF800000
        let bytes = (0xEF80C000u32).to_le_bytes();
        assert_eq!(classify_resources(Arch::Hexagon, &bytes), COPROC_FP);
    }

    #[test]
    fn hex_res_fp_sfclass() {
        // sfclass: mask 0xFFE00000, value 0x85E00000
        let bytes = (0x85E0C000u32).to_le_bytes();
        assert_eq!(classify_resources(Arch::Hexagon, &bytes), COPROC_FP);
    }

    #[test]
    fn hex_res_fp_dfclass() {
        // dfclass: mask 0xFF000000, value 0xDC000000
        let bytes = (0xDC00C000u32).to_le_bytes();
        assert_eq!(classify_resources(Arch::Hexagon, &bytes), COPROC_FP);
    }

    #[test]
    fn hex_res_fp_sfimm() {
        // sfimm_p/sfimm_n: mask 0xFF000000, value 0xD6000000
        let bytes = (0xD600C000u32).to_le_bytes();
        assert_eq!(classify_resources(Arch::Hexagon, &bytes), COPROC_FP);
    }

    #[test]
    fn hex_res_fp_conv_df2d() {
        // DF<->D conversions: mask 0xFFE00000, value 0x80E00000
        let bytes = (0x80E0C000u32).to_le_bytes();
        assert_eq!(classify_resources(Arch::Hexagon, &bytes), COPROC_FP);
    }

    #[test]
    fn hex_res_fp_conv_sf2df() {
        // SF<->DF conversions: mask 0xFF800000, value 0x84800000
        let bytes = (0x8480C000u32).to_le_bytes();
        assert_eq!(classify_resources(Arch::Hexagon, &bytes), COPROC_FP);
    }

    #[test]
    fn hex_res_fp_conv_sf_w() {
        // SF<->W conversions: mask 0xFF000000, value 0x8B000000
        let bytes = (0x8B00C000u32).to_le_bytes();
        assert_eq!(classify_resources(Arch::Hexagon, &bytes), COPROC_FP);
    }

    #[test]
    fn hex_res_duplex_skipped() {
        // Duplex (PP=00) should return 0 even if upper bits match HVX
        let bytes = (0x18000000u32).to_le_bytes(); // PP=00
        assert_eq!(classify_resources(Arch::Hexagon, &bytes), 0);
    }

    #[test]
    fn hex_res_scalar_no_flags() {
        // A scalar instruction (ICLASS_ALU32, 0x7_______) with PP=11
        let bytes = (0x7000C000u32).to_le_bytes();
        assert_eq!(classify_resources(Arch::Hexagon, &bytes), 0);
    }

    // --- Resource classification: AArch64 ---

    #[test]
    fn aarch64_res_simd_fp() {
        // Advanced SIMD + scalar FP: bits[27:25]=111
        // e.g. FADD S0, S1, S2 = 0x1E212820
        let bytes = (0x1E212820u32).to_le_bytes();
        assert_eq!(
            classify_resources(Arch::Aarch64, &bytes),
            COPROC_SIMD | COPROC_FP,
        );
    }

    #[test]
    fn aarch64_res_sve() {
        // SVE: bits[28:25]=0010
        // e.g. SVE ADD (vectors) = 0x04200000
        let bytes = (0x04200000u32).to_le_bytes();
        assert_eq!(classify_resources(Arch::Aarch64, &bytes), COPROC_SIMD);
    }

    #[test]
    fn aarch64_res_integer_no_flags() {
        // Integer: MOV X0, #0 = 0xD2800000
        // bits[27:25] = 101, not 111 → no SIMD/FP
        let bytes = (0xD2800000u32).to_le_bytes();
        assert_eq!(classify_resources(Arch::Aarch64, &bytes), 0);
    }

    // --- Resource classification: RISC-V ---

    #[test]
    fn riscv_res_vector_alu() {
        // OP-V (opcode 0x57): vector ALU
        let bytes = (0x00000057u32).to_le_bytes();
        assert_eq!(classify_resources(Arch::Riscv, &bytes), COPROC_SIMD);
    }

    #[test]
    fn riscv_res_fp_alu() {
        // OP-FP (opcode 0x53): scalar FP ALU
        let bytes = (0x00000053u32).to_le_bytes();
        assert_eq!(classify_resources(Arch::Riscv, &bytes), COPROC_FP);
    }

    #[test]
    fn riscv_res_fp_load() {
        // LOAD-FP (opcode 0x07) with scalar width (width=2, bits[14:12]=010)
        let bytes = (0x00002007u32).to_le_bytes();
        assert_eq!(classify_resources(Arch::Riscv, &bytes), COPROC_FP);
    }

    #[test]
    fn riscv_res_vector_load() {
        // LOAD-FP (opcode 0x07) with vector width (width=6, bits[14:12]=110)
        let bytes = (0x00006007u32).to_le_bytes();
        assert_eq!(
            classify_resources(Arch::Riscv, &bytes),
            COPROC_FP | COPROC_SIMD,
        );
    }

    #[test]
    fn riscv_res_fmadd() {
        // FMADD (opcode 0x43)
        let bytes = (0x00000043u32).to_le_bytes();
        assert_eq!(classify_resources(Arch::Riscv, &bytes), COPROC_FP);
    }

    #[test]
    fn riscv_res_integer_no_flags() {
        // ADDI x0, x0, 0 (opcode 0x13): no coprocessor
        let bytes = (0x00000013u32).to_le_bytes();
        assert_eq!(classify_resources(Arch::Riscv, &bytes), 0);
    }

    #[test]
    fn riscv_res_compressed_skipped() {
        // 16-bit compressed instruction (bits[1:0] != 11)
        let bytes = 0xA001u16.to_le_bytes();
        assert_eq!(classify_resources(Arch::Riscv, &bytes), 0);
    }

    // --- Resource classification: x86 ---

    #[test]
    fn x86_res_vex2() {
        // VEX 2-byte: C5 F8 ... (VMOVAPS)
        let bytes = [0xC5, 0xF8, 0x28, 0xC0];
        assert_eq!(classify_resources(Arch::X86, &bytes), COPROC_SIMD);
    }

    #[test]
    fn x86_res_vex3() {
        // VEX 3-byte: C4 E1 ...
        let bytes = [0xC4, 0xE1, 0x79, 0x28, 0xC0];
        assert_eq!(classify_resources(Arch::X86, &bytes), COPROC_SIMD);
    }

    #[test]
    fn x86_res_evex() {
        // EVEX: 62 ...
        let bytes = [0x62, 0xF1, 0x7C, 0x48, 0x28, 0xC0];
        assert_eq!(classify_resources(Arch::X86, &bytes), COPROC_SIMD);
    }

    #[test]
    fn x86_res_x87() {
        // x87: D8 C0 (FADD ST(0), ST(0))
        let bytes = [0xD8, 0xC0];
        assert_eq!(classify_resources(Arch::X86, &bytes), COPROC_FP);
    }

    #[test]
    fn x86_res_x87_with_prefix() {
        // x87 with legacy prefix: 66 D9 E1 (FABS with operand-size prefix)
        let bytes = [0x66, 0xD9, 0xE1];
        assert_eq!(classify_resources(Arch::X86, &bytes), COPROC_FP);
    }

    #[test]
    fn x86_res_integer_no_flags() {
        // NOP: 90
        let bytes = [0x90];
        assert_eq!(classify_resources(Arch::X86, &bytes), 0);
    }

    #[test]
    fn x86_res_vex_after_rex() {
        // REX prefix (0x48) followed by VEX (0xC5) — REX is consumed,
        // VEX is the opcode
        let bytes = [0x48, 0xC5, 0xF8, 0x28, 0xC0];
        assert_eq!(classify_resources(Arch::X86, &bytes), COPROC_SIMD);
    }

    // --- Resource classification: edge cases ---

    #[test]
    fn res_too_short_bytes() {
        assert_eq!(classify_resources(Arch::Hexagon, &[0, 0]), 0);
        assert_eq!(classify_resources(Arch::Aarch64, &[0, 0]), 0);
        assert_eq!(classify_resources(Arch::Riscv, &[0]), 0);
        assert_eq!(classify_resources(Arch::X86, &[]), 0);
    }

    #[test]
    fn res_unknown_arch() {
        let bytes = [0xFF; 4];
        assert_eq!(classify_resources(Arch::Unknown, &bytes), 0);
    }
}
