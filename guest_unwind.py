#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later

"""
guest_unwind.py - GDB command for unwinding AArch64 guest stack frames in QEMU.

Usage (from GDB prompt):
    source guest_unwind.py
    guest_unwind

The command walks the AArch64 frame-pointer chain stored in guest memory,
reading each frame record via the QEMU helper debug_read_64().

AArch64 frame record layout (each slot is 8 bytes / 2 × 32-bit words):
    [FP + 0]  : previous FP  (low  32 bits, then high 32 bits)
    [FP + 8]  : saved LR     (low  32 bits, then high 32 bits)

The frame-pointer register on AArch64 is X29; the link register is X30.
cpu_env(current_cpu)->xregs[29] is the frame pointer (FP).
cpu_env(current_cpu)->xregs[30] is the link register (LR) for the
innermost frame.

Note: guest CPU state is accessed as ((CPUARMState*)(current_cpu + 1)).
"""

import gdb
import os
import re
import struct
import subprocess

try:
    import capstone
    _CS_ENGINES: dict[str, capstone.Cs] = {
        "aarch64": capstone.Cs(capstone.CS_ARCH_ARM64, capstone.CS_MODE_ARM),
        "x86_64":  capstone.Cs(capstone.CS_ARCH_X86,   capstone.CS_MODE_64),
    }
    for _cs in _CS_ENGINES.values():
        _cs.detail = False
    _CAPSTONE_AVAILABLE = True
except ImportError:
    _CAPSTONE_AVAILABLE = False
    print("[guest_unwind] warning: capstone not available, disassembly will be skipped")


# ---------------------------------------------------------------------------
# Low-level helpers
# ---------------------------------------------------------------------------

MAX_FRAMES = 256          # Guard against corrupt stacks


def _read_u64(addr: int) -> int:
    """Read a 64-bit word from guest memory via QEMU helper."""
    result = gdb.parse_and_eval(f"debug_read_64(current_cpu, (uint64_t){addr:#x})")
    return int(result) & 0xFFFF_FFFF_FFFF_FFFF


def _read_arm64_reg(field: str) -> int:
    """Read a named field from CPUARMState."""
    val = gdb.parse_and_eval(f"((CPUARMState*)(current_cpu + 1))->{field}")
    return int(val) & 0xFFFF_FFFF_FFFF_FFFF


def _read_x86_reg(field: str) -> int:
    """Read a named field from CPUX86State."""
    val = gdb.parse_and_eval(f"((CPUX86State*)(current_cpu + 1))->{field}")
    return int(val) & 0xFFFF_FFFF_FFFF_FFFF


# ---------------------------------------------------------------------------
# Frame-pointer unwinders
# ---------------------------------------------------------------------------

def _canonicalize(addr: int) -> int:
    """
    Reconstruct a canonical 64-bit virtual address.
    Keep only the lower 48 bits; if bit 47 is set, sign-extend by filling
    bits [63:48] with 0xffff, otherwise leave them as 0x0000.
    """
    addr &= 0xFFFF_FFFF_FFFF  # keep lower 48 bits
    if addr & (1 << 47):      # bit 47 set → kernel address
        addr |= 0xFFFF_0000_0000_0000
    return addr


def _unwind_frames_aarch64():
    """
    Yield (frame_number, pc) tuples by walking the AArch64 FP (X29) chain.

    Frame record layout written by the function prologue
    (stp x29, x30, [sp, #-N]!  /  mov x29, sp):
        [FP + 0x00]  previous FP   (8 bytes)
        [FP + 0x08]  saved LR      (8 bytes)
    """
    pc = _read_arm64_reg("pc")       # current PC → frame #0
    fp = _read_arm64_reg("xregs[29]")  # X29 = frame pointer
    lr = _read_arm64_reg("xregs[30]")  # X30 = link register

    yield 0, _canonicalize(pc)
    yield 1, _canonicalize(lr)

    frame_num = 2
    visited = set()

    while frame_num < MAX_FRAMES:
        if fp == 0:
            break
        if fp & 0x7:
            print(f"  [!] FP {fp:#018x} is misaligned — aborting unwind")
            break
        if fp in visited:
            print(f"  [!] FP cycle detected at {fp:#018x} — aborting unwind")
            break
        visited.add(fp)

        try:
            prev_fp  = _read_u64(fp)
            saved_lr = _read_u64(fp + 8)
        except gdb.error as exc:
            print(f"  [!] Memory read failed at FP {fp:#018x}: {exc}")
            break

        yield frame_num, _canonicalize(saved_lr)
        fp = prev_fp
        frame_num += 1

    if frame_num == MAX_FRAMES:
        print(f"  [!] Reached frame limit ({MAX_FRAMES}) — unwind truncated")


def _unwind_frames_x86_64():
    """
    Yield (frame_number, pc) tuples by walking the x86-64 RBP chain.

    Frame record layout written by the function prologue
    (push rbp  /  mov rbp, rsp):
        [RBP + 0x00]  previous RBP  (8 bytes)
        [RBP + 0x08]  saved RIP     (8 bytes)
    """
    pc = _read_x86_reg("eip")   # RIP (stored as eip in QEMU's CPUX86State)
    fp = _read_x86_reg("regs[5]")  # RBP = regs[R_EBP = 5]

    yield 0, _canonicalize(pc)

    frame_num = 1
    visited = set()

    while frame_num < MAX_FRAMES:
        if fp == 0:
            break
        if fp & 0x7:
            print(f"  [!] RBP {fp:#018x} is misaligned — aborting unwind")
            break
        if fp in visited:
            print(f"  [!] RBP cycle detected at {fp:#018x} — aborting unwind")
            break
        visited.add(fp)

        try:
            prev_fp  = _read_u64(fp)
            saved_rip = _read_u64(fp + 8)
        except gdb.error as exc:
            print(f"  [!] Memory read failed at RBP {fp:#018x}: {exc}")
            break

        yield frame_num, _canonicalize(saved_rip)
        fp = prev_fp
        frame_num += 1

    if frame_num == MAX_FRAMES:
        print(f"  [!] Reached frame limit ({MAX_FRAMES}) — unwind truncated")


def _unwind_frames(target: str):
    """Dispatch to the correct unwinder for the detected target."""
    if target in ("aarch64", "aarch64_be"):
        yield from _unwind_frames_aarch64()
    elif target == "x86_64":
        yield from _unwind_frames_x86_64()


# ---------------------------------------------------------------------------
# Symbol file discovery from .gdbinit
# ---------------------------------------------------------------------------

def _load_symbol_files(gdbinit_path: str = "gdbinit") -> list[tuple[str, int]]:
    """
    Parse a .gdbinit file and extract (binary_path, offset) pairs from every
    'add-symbol-file' line.

    Syntax handled:
        add-symbol-file PATH
        add-symbol-file PATH OFFSET
        add-symbol-file PATH -o OFFSET   (newer GDB style)

    Only files that exist on disk are returned.
    Offset defaults to 0 when not specified.
    """
    entries: list[tuple[str, int]] = []
    try:
        with open(gdbinit_path, "r", errors="replace") as fh:
            for raw in fh:
                line = raw.strip()
                if not line.startswith("add-symbol-file"):
                    continue
                # Drop the command word itself
                rest = line[len("add-symbol-file"):].strip()
                # Tokenise (handles quoted paths too, but simple split suffices
                # for the typical QEMU / kernel workflow)
                tokens = rest.split()
                if not tokens:
                    continue
                path = tokens[0]
                offset = 0
                if len(tokens) >= 2:
                    # Support both:  PATH OFFSET  and  PATH -o OFFSET
                    if tokens[1] == "-o" and len(tokens) >= 3:
                        offset_str = tokens[2]
                    else:
                        offset_str = tokens[1]
                    try:
                        offset = int(offset_str, 0)   # 0x… or decimal
                    except ValueError:
                        offset = 0
                if os.path.isfile(path):
                    entries.append((path, offset))
    except OSError:
        pass
    return entries


# Cache so we only parse once per GDB session
_SYMBOL_FILES: list[tuple[str, int]] | None = None

def _get_symbol_files() -> list[tuple[str, int]]:
    global _SYMBOL_FILES
    if _SYMBOL_FILES is None:
        _SYMBOL_FILES = _load_symbol_files()
    return _SYMBOL_FILES


# ---------------------------------------------------------------------------
# Symbol resolution (best-effort)
# ---------------------------------------------------------------------------

def _addr2line(binary: str, addr: int) -> tuple[str, str, int]:
    """
    Run addr2line for a single (binary, address) pair.
    Returns (func, filepath, lineno) or ('', '', 0) on failure / no match.
    '??' in either field is treated as no match.
    """
    try:
        result = subprocess.run(
            ["addr2line", "--exe", binary,
             "--functions", "--demangle", f"{addr:#x}"],
            capture_output=True, text=True, timeout=5
        )
        if result.returncode != 0:
            return "", "", 0
        lines = result.stdout.strip().splitlines()
        if len(lines) < 2:
            return "", "", 0
        func = lines[0].strip()
        location = lines[1].strip()
        if "??" in func or "??" in location:
            return "", "", 0
        filepath, _, lineno_str = location.rpartition(":")
        try:
            lineno = int(lineno_str)
        except ValueError:
            lineno = 0
        return func, filepath, lineno
    except (FileNotFoundError, subprocess.TimeoutExpired, OSError):
        return "", "", 0


def _resolve_symbol(pc: int) -> tuple[str, str, int]:
    """
    Try every symbol file from .gdbinit in order.
    When an offset is present the address is adjusted before querying:
        effective_addr = pc - offset
    Returns the first (func, filepath, lineno) that is not '??', or
    ('', '', 0) when nothing matches.
    """
    for binary, offset in _get_symbol_files():
        func, filepath, lineno = _addr2line(binary, pc - offset)
        if func:
            return func, filepath, lineno
    return "", "", 0


def _read_source_snippet(filepath: str, lineno: int, func: str,
                         context: int = 1) -> list[str]:
    """
    Return up to (2 * context + 1) lines centred on lineno from filepath,
    prefixed with line numbers and a '>' marker on the target line.
    """
    if not filepath or lineno <= 0:
        return []
    try:
        with open(filepath, "r", errors="replace") as fh:
            all_lines = fh.readlines()
        start = max(0, lineno - 1 - context)
        end = min(len(all_lines), lineno + context)
        snippet = []
        hunk_header = f"@@ {filepath}:{start + 1} @@ {func}"
        snippet.append(f"    {hunk_header}")
        for i, src in enumerate(all_lines[start:end], start=start + 1):
            marker = ">" if i == lineno else " "
            snippet.append(f"    {marker} {i:5d}  {src.rstrip()}")
        return snippet
    except OSError:
        return []


# ---------------------------------------------------------------------------
# Disassembly
# ---------------------------------------------------------------------------

def _disassemble(pc: int, target: str) -> str:
    """
    Read the instruction at pc from guest memory and disassemble it with
    capstone using the engine matching target.
    AArch64 instructions are always 4 bytes; x86-64 instructions are
    variable-length (read up to 15 bytes).
    Returns a formatted string or '' when unavailable.
    """
    if not _CAPSTONE_AVAILABLE:
        return ""
    cs_key = "aarch64" if target in ("aarch64", "aarch64_be") else target
    cs = _CS_ENGINES.get(cs_key)
    if cs is None:
        return ""
    try:
        if cs_key == "aarch64":
            raw = _read_u64(pc) & 0xFFFF_FFFF   # 32-bit fixed-width instruction
            insn_bytes = struct.pack("<I", raw)
        else:
            # x86-64: instructions are 1-15 bytes; read 16 to cover worst case
            lo = _read_u64(pc)
            hi = _read_u64(pc + 8)
            insn_bytes = struct.pack("<QQ", lo, hi)
        insns = list(cs.disasm(insn_bytes, pc, count=1))
        if insns:
            insn = insns[0]
            return f"    => {pc:#018x}:  {insn.mnemonic}  {insn.op_str}".rstrip()
    except (gdb.error, Exception):
        pass
    return ""


# ---------------------------------------------------------------------------
# Architecture detection
# ---------------------------------------------------------------------------

# Map QEMU target_name() strings to a human-readable label.
# Only architectures supported by this unwinder are listed.
# Keys are exact strings returned by target_name().
_SUPPORTED_TARGETS: dict[str, str] = {
    "aarch64":     "AArch64",
    "aarch64_be":  "AArch64 (big-endian)",
    "x86_64":      "x86-64",
}


def _detect_target() -> str:
    """
    Call QEMU's target_name() via GDB and return the raw string.
    GDB returns the result in the form:  0x5555563ec073 "aarch64"
    We extract the string between the double quotes.
    Returns '' on failure.
    """
    try:
        val = gdb.parse_and_eval("target_name()")
        m = re.search(r'"([^"]+)"', str(val))
        return m.group(1) if m else ""
    except gdb.error:
        return ""


# ---------------------------------------------------------------------------
# GDB command class
# ---------------------------------------------------------------------------

class GuestUnwindCommand(gdb.Command):
    """
    guest_unwind — print an AArch64 guest backtrace via frame-pointer unwinding.

    Reads guest memory through QEMU's debug_read_64() helper and walks the
    X29 (FP) chain to reconstruct the call stack.

    Usage:
        guest_unwind

    The command prints one line per frame:
        #N  0x<pc>  [<symbol> [file:line]]
    """

    def __init__(self):
        super().__init__("guest_unwind", gdb.COMMAND_USER)

    # ------------------------------------------------------------------
    def invoke(self, _args: str, _from_tty: bool) -> None:
        # Architecture check -----------------------------------------------
        target = _detect_target()
        if target not in _SUPPORTED_TARGETS:
            if target:
                all_supported = ", ".join(sorted(_SUPPORTED_TARGETS))
                print(f"[!] guest_unwind: unsupported QEMU target '{target}'.")
                print(f"    Supported targets: {all_supported}")
            else:
                print("[!] guest_unwind: could not determine QEMU target "
                      "(is target_name() available?).")
            return

        print(f"Guest {_SUPPORTED_TARGETS[target]} stack unwind  "
              f"[target: {target}]")
        print("=" * 60)

        try:
            frames = list(_unwind_frames(target))
        except gdb.error as exc:
            print(f"[!] Failed to start unwind: {exc}")
            return

        if not frames:
            print("  (no frames found)")
            return

        for frame_num, pc in frames:
            func, filepath, lineno = _resolve_symbol(pc)
            if func:
                location = f"{filepath}:{lineno}" if filepath and lineno else ""
                sym_str = f"  {func}" + (f"  [{location}]" if location else "")
            else:
                sym_str = ""
            print(f"  #{frame_num:<4d}  {pc:#018x}{sym_str}")
            for src_line in _read_source_snippet(filepath, lineno, func):
                print(src_line)
            disasm = _disassemble(pc, target)
            if disasm:
                print(disasm)

        print("=" * 60)
        print(f"  {len(frames)} frame(s) unwound")


# ---------------------------------------------------------------------------
# Register the command
# ---------------------------------------------------------------------------

GuestUnwindCommand()
print("[guest_unwind] command registered — type 'guest_unwind' to use it")
