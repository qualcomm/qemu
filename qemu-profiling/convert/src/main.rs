// SPDX-License-Identifier: BSD-3-Clause-Clear
// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//
// tcg-prof-convert: Convert QEMU PGO plugin profiles to LLVM and GCC
// tool formats (BOLT, AutoFDO, GCC AutoFDO, lld CG sort, temporal traces).

use clap::{Parser, Subcommand};
use tcg_prof_convert::{elf_index, native};

/// Human-readable name for a tier number.
fn tier_name(tier: u8) -> &'static str {
    match tier {
        0 => "hotness",
        1 => "edges",
        2 => "calls",
        3 => "resources",
        _ => "unknown",
    }
}

/// Check that a profile meets the minimum tier requirement for a
/// conversion format. Returns Err with a descriptive message on
/// failure.
fn require_tier(profile: &native::Profile, min_tier: u8, format_name: &str) -> Result<(), String> {
    if profile.header.tier < min_tier {
        return Err(format!(
            "{} conversion requires at least tier {} ({}) but \
             profile was collected at tier {} ({}). \
             Re-run the plugin with tier={}.",
            format_name,
            min_tier,
            tier_name(min_tier),
            profile.header.tier,
            tier_name(profile.header.tier),
            tier_name(min_tier),
        ));
    }
    Ok(())
}

#[derive(Parser)]
#[command(name = "tcg-prof-convert")]
#[command(about = "Convert QEMU PGO profiles to LLVM tool formats")]
struct Cli {
    #[command(subcommand)]
    command: Commands,
}

#[derive(Subcommand)]
enum Commands {
    /// Convert to BOLT pre-aggregated profile (requires tier edges)
    Bolt {
        /// Input native profile path
        #[arg(short, long)]
        input: String,
        /// Guest ELF binary
        #[arg(short, long)]
        binary: String,
        /// Output BOLT profile path
        #[arg(short, long)]
        output: String,
    },
    /// Convert to llvm-profgen unsymbolized profile (requires tier
    /// edges)
    Autofdo {
        /// Input native profile path
        #[arg(short, long)]
        input: String,
        /// Guest ELF binary
        #[arg(short, long)]
        binary: String,
        /// Output unsymbolized profile path
        #[arg(short, long)]
        output: String,
    },
    /// Convert to lld call graph ordering file (requires tier edges;
    /// enriched with tier calls)
    Cgprof {
        /// Input native profile path
        #[arg(short, long)]
        input: String,
        /// Guest ELF binary
        #[arg(short, long)]
        binary: String,
        /// Output CG profile path
        #[arg(short, long)]
        output: String,
    },
    /// Convert to lld --call-graph-ordering-file (CGSort) format.
    /// Alias for cgprof. Requires tier edges; enriched with tier
    /// calls.
    Cgsort {
        /// Input native profile path
        #[arg(short, long)]
        input: String,
        /// Guest ELF binary
        #[arg(short, long)]
        binary: String,
        /// Output CG sort ordering file
        #[arg(short, long)]
        output: String,
    },
    /// Convert to GCC AutoFDO text profile for create_gcov
    /// (requires tier edges)
    GccAutofdo {
        /// Input native profile path
        #[arg(short, long)]
        input: String,
        /// Guest ELF binary
        #[arg(short, long)]
        binary: String,
        /// Output text profile path
        #[arg(short, long)]
        output: String,
    },
    /// Convert to temporal profile trace (requires tier hotness)
    Temporal {
        /// Input native profile path
        #[arg(short, long)]
        input: String,
        /// Guest ELF binary
        #[arg(short, long)]
        binary: String,
        /// Output temporal trace path
        #[arg(short, long)]
        output: String,
    },
    /// Dump native profile contents (for debugging)
    Dump {
        /// Input native profile path
        #[arg(short, long)]
        input: String,
    },
}

fn main() {
    let cli = Cli::parse();

    let result = match cli.command {
        Commands::Bolt {
            input,
            binary,
            output,
        } => run_bolt(&input, &binary, &output),
        Commands::Autofdo {
            input,
            binary,
            output,
        } => run_autofdo(&input, &binary, &output),
        Commands::GccAutofdo {
            input,
            binary,
            output,
        } => run_gcc_autofdo(&input, &binary, &output),
        Commands::Cgprof {
            input,
            binary,
            output,
        }
        | Commands::Cgsort {
            input,
            binary,
            output,
        } => run_cgprof(&input, &binary, &output),
        Commands::Temporal {
            input,
            binary,
            output,
        } => run_temporal(&input, &binary, &output),
        Commands::Dump { input } => run_dump(&input),
    };

    if let Err(e) = result {
        eprintln!("error: {}", e);
        std::process::exit(1);
    }
}

fn run_bolt(input: &str, binary: &str, output: &str) -> Result<(), String> {
    let profile = native::parse(input)?;
    require_tier(&profile, 1, "BOLT")?;
    let elf = elf_index::ElfIndex::from_file(binary)?;
    tcg_prof_convert::emit_bolt(&profile, &elf, output)
}

fn run_autofdo(input: &str, binary: &str, output: &str) -> Result<(), String> {
    let profile = native::parse(input)?;
    require_tier(&profile, 1, "AutoFDO")?;
    let elf = elf_index::ElfIndex::from_file(binary)?;
    tcg_prof_convert::emit_autofdo(&profile, &elf, output)
}

fn run_gcc_autofdo(input: &str, binary: &str, output: &str) -> Result<(), String> {
    let profile = native::parse(input)?;
    require_tier(&profile, 1, "GCC AutoFDO")?;
    let elf = elf_index::ElfIndex::from_file(binary)?;
    tcg_prof_convert::emit_gcc_autofdo(&profile, &elf, output)
}

fn run_cgprof(input: &str, binary: &str, output: &str) -> Result<(), String> {
    let profile = native::parse(input)?;
    require_tier(&profile, 1, "CGSort")?;
    if profile.header.tier < 2 {
        eprintln!(
            "warning: CGSort works best with tier calls (2); \
             profile has tier {} ({}). Direct/indirect call data \
             will be missing; only cross-function branch edges \
             will be used.",
            profile.header.tier,
            tier_name(profile.header.tier),
        );
    }
    let elf = elf_index::ElfIndex::from_file(binary)?;
    tcg_prof_convert::emit_cgprof(&profile, &elf, output)
}

fn run_temporal(input: &str, binary: &str, output: &str) -> Result<(), String> {
    let profile = native::parse(input)?;
    require_tier(&profile, 0, "Temporal")?;
    let elf = elf_index::ElfIndex::from_file(binary)?;
    tcg_prof_convert::emit_temporal(&profile, &elf, output)
}

fn run_dump(input: &str) -> Result<(), String> {
    let profile = native::parse(input)?;
    let h = &profile.header;

    println!("Header:");
    println!(
        "  version={} tier={} ({}) ptr_size={} flags=0x{:x}",
        h.format_version,
        h.tier,
        tier_name(h.tier),
        h.pointer_size,
        h.flags
    );
    println!(
        "  load_addr=0x{:x} text=[0x{:x}, 0x{:x}) entry=0x{:x}",
        h.load_addr, h.text_start, h.text_end, h.entry_addr
    );
    let sysemu = (h.flags & native::FLAG_SYSEMU) != 0;
    let streaming = (h.flags & native::FLAG_STREAMING) != 0;
    println!("  sysemu={} streaming={}", sysemu, streaming);

    println!("\nMetadata:");
    for (k, v) in &profile.metadata {
        println!("  {}={}", k, v);
    }

    println!("\nSymbols: {}", profile.symbols.len());
    for sym in &profile.symbols {
        println!("  [{}] 0x{:x} {}", sym.sym_id, sym.addr, sym.name);
    }

    println!("\nTB Execs: {}", profile.tb_execs.len());
    let mut sorted = profile.tb_execs.clone();
    sorted.sort_by(|a, b| b.exec_count.cmp(&a.exec_count));
    for (i, tb) in sorted.iter().take(20).enumerate() {
        println!(
            "  [{:3}] 0x{:x}-0x{:x} n_insns={} exec={} first_seen={}",
            i, tb.start_addr, tb.end_addr, tb.n_insns, tb.exec_count,
            tb.first_seen_seq
        );
    }
    if sorted.len() > 20 {
        println!("  ... and {} more", sorted.len() - 20);
    }

    println!("\nBranch Edges: {}", profile.branch_edges.len());
    let mut be = profile.branch_edges.clone();
    be.sort_by(|a, b| b.count.cmp(&a.count));
    for (i, e) in be.iter().take(20).enumerate() {
        println!(
            "  [{:3}] 0x{:x} -> 0x{:x} count={}",
            i, e.from, e.to, e.count
        );
    }
    if be.len() > 20 {
        println!("  ... and {} more", be.len() - 20);
    }

    println!("\nFall-Through Edges: {}", profile.fall_through_edges.len());

    if !profile.direct_calls.is_empty() {
        println!("\nDirect Calls: {}", profile.direct_calls.len());
    }
    if !profile.indirect_calls.is_empty() {
        println!("Indirect Calls: {}", profile.indirect_calls.len());
    }
    if !profile.tb_resources.is_empty() {
        println!("TB Resources: {}", profile.tb_resources.len());
    }
    if !profile.discon_events.is_empty() {
        println!("Discontinuities: {}", profile.discon_events.len());
    }

    if let Some(ref footer) = profile.footer {
        println!("\nFooter:");
        println!(
            "  clean_exit={} total_tbs={} total_edges={} \
             total_exec={} wall_time={:.3}s",
            footer.clean_exit,
            footer.total_tb_count,
            footer.total_edge_records,
            footer.total_exec_count,
            footer.wall_time_ns as f64 / 1e9
        );
    } else {
        println!("\nFooter: MISSING (crash or incomplete write)");
    }

    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn tier_name_mapping() {
        assert_eq!(tier_name(0), "hotness");
        assert_eq!(tier_name(1), "edges");
        assert_eq!(tier_name(2), "calls");
        assert_eq!(tier_name(3), "resources");
        assert_eq!(tier_name(99), "unknown");
    }

    #[test]
    fn require_tier_passes_when_sufficient() {
        let profile = native::Profile {
            header: native::FileHeader {
                format_version: 1,
                tier: 2,
                pointer_size: 8,
                flags: 0,
                load_addr: 0,
                text_start: 0,
                text_end: 0,
                entry_addr: 0,
                build_id: [0; 16],
            },
            metadata: std::collections::HashMap::new(),
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
        assert!(require_tier(&profile, 0, "Test").is_ok());
        assert!(require_tier(&profile, 1, "Test").is_ok());
        assert!(require_tier(&profile, 2, "Test").is_ok());
    }

    #[test]
    fn require_tier_fails_when_insufficient() {
        let profile = native::Profile {
            header: native::FileHeader {
                format_version: 1,
                tier: 0,
                pointer_size: 8,
                flags: 0,
                load_addr: 0,
                text_start: 0,
                text_end: 0,
                entry_addr: 0,
                build_id: [0; 16],
            },
            metadata: std::collections::HashMap::new(),
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
        let err = require_tier(&profile, 1, "BOLT").unwrap_err();
        assert!(err.contains("BOLT"));
        assert!(err.contains("tier 1 (edges)"));
        assert!(err.contains("tier 0 (hotness)"));
    }
}
