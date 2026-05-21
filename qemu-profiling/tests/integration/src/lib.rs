// SPDX-License-Identifier: BSD-3-Clause-Clear
// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//
// Shared test harness for PGO plugin integration tests.

use std::path::{Path, PathBuf};
use std::process::Command;

use tcg_prof_convert::native::{self, Profile};
use tempfile::NamedTempFile;

/// Result of running a profiled binary under QEMU.
pub struct ProfileRun {
    pub profile: Profile,
    pub stderr: String,
    pub exit_code: i32,
    _tmpfile: NamedTempFile,
}

/// Resolve the workspace root (two levels up from this crate).
fn workspace_root() -> PathBuf {
    let manifest = PathBuf::from(env!("CARGO_MANIFEST_DIR"));
    manifest
        .parent()
        .and_then(|p| p.parent())
        .expect("cannot find workspace root")
        .to_path_buf()
}

/// Get the path to the PGO plugin shared library.
pub fn plugin_path() -> Option<PathBuf> {
    let from_env = std::env::var("PGO_PLUGIN").ok().map(PathBuf::from);
    let default = workspace_root().join("target/release/libtcg_prof.so");
    let path = from_env.unwrap_or(default);
    if path.exists() {
        Some(path)
    } else {
        None
    }
}

/// Get the QEMU binary path for a given architecture.
pub fn get_qemu(arch: &str) -> Option<PathBuf> {
    let (env_var, default_rel) = match arch {
        "hexagon" => ("QEMU_HEXAGON", "build_claude/qemu-hexagon"),
        "riscv64" => ("QEMU_RISCV64", "build_multiarch/qemu-riscv64"),
        "aarch64" => ("QEMU_AARCH64", "build_multiarch/qemu-aarch64"),
        _ => return None,
    };

    let from_env = std::env::var(env_var).ok().map(PathBuf::from);
    // Default is relative to the top-level QEMU source tree (3 levels up
    // from CARGO_MANIFEST_DIR = qemu-profiling/tests/integration).
    let qemu_root = workspace_root()
        .parent()
        .expect("cannot find QEMU root")
        .to_path_buf();
    let default = qemu_root.join(default_rel);
    let path = from_env.unwrap_or(default);
    if path.exists() {
        Some(path)
    } else {
        None
    }
}

/// Get the fixture directory for a given architecture.
/// Returns None if the architecture was skipped at build time.
pub fn fixture_dir(arch: &str) -> Option<PathBuf> {
    let env_key = format!("FIXTURE_DIR_{}", arch.to_uppercase());
    option_env_runtime(&env_key).map(PathBuf::from)
}

/// Runtime lookup of compile-time env vars set by build.rs.
/// We use a macro approach since env! is compile-time only.
fn option_env_runtime(key: &str) -> Option<&'static str> {
    match key {
        "FIXTURE_DIR_HEXAGON" => option_env!("FIXTURE_DIR_HEXAGON"),
        "FIXTURE_DIR_RISCV64" => option_env!("FIXTURE_DIR_RISCV64"),
        "FIXTURE_DIR_AARCH64" => option_env!("FIXTURE_DIR_AARCH64"),
        _ => None,
    }
}

/// Get path to a specific fixture binary for an architecture.
pub fn fixture_binary(arch: &str, name: &str) -> Option<PathBuf> {
    let dir = fixture_dir(arch)?;
    let path = Path::new(&dir).join(name);
    if path.exists() {
        Some(path)
    } else {
        None
    }
}

/// Tier name string for plugin argument.
pub fn tier_arg(tier: u8) -> &'static str {
    match tier {
        0 => "hotness",
        1 => "edges",
        2 => "calls",
        3 => "resources",
        _ => "edges",
    }
}

/// Run a binary under QEMU with the PGO plugin and parse the profile.
pub fn run_profile(qemu: &Path, binary: &Path, tier: u8) -> ProfileRun {
    let plugin = plugin_path().expect("PGO plugin not found");
    let tmpfile = NamedTempFile::new().expect("failed to create temp file");
    let profile_path = tmpfile.path().to_str().unwrap().to_string();

    let plugin_arg = format!(
        "{},tier={},output={}",
        plugin.display(),
        tier_arg(tier),
        profile_path
    );

    let output = Command::new(qemu)
        .arg("-plugin")
        .arg(&plugin_arg)
        .arg(binary)
        .output()
        .unwrap_or_else(|e| {
            panic!(
                "failed to run QEMU ({} {}): {}",
                qemu.display(),
                binary.display(),
                e
            )
        });

    let exit_code = output.status.code().unwrap_or(-1);
    let stderr = String::from_utf8_lossy(&output.stderr).to_string();

    let profile = native::parse(&profile_path).unwrap_or_else(|e| {
        panic!(
            "failed to parse profile at {}: {}\nQEMU stderr:\n{}",
            profile_path, e, stderr
        )
    });

    ProfileRun {
        profile,
        stderr,
        exit_code,
        _tmpfile: tmpfile,
    }
}

// ---- Assertion helpers ----

/// Assert the program exited cleanly with a valid footer.
pub fn assert_clean_exit(run: &ProfileRun) {
    assert_eq!(run.exit_code, 0, "QEMU exited with code {}", run.exit_code);
    let footer = run
        .profile
        .footer
        .as_ref()
        .expect("profile has no footer");
    assert!(footer.clean_exit, "profile footer: clean_exit=false");
}

/// Assert at least `n` as the max branch edge count.
pub fn assert_min_edge_count(profile: &Profile, n: u64) {
    let max = max_branch_edge_count(profile);
    assert!(
        max >= n,
        "max branch edge count {} < expected minimum {}",
        max,
        n
    );
}

/// Assert at least `n` total TBs in profile.
pub fn assert_min_tb_count(profile: &Profile, n: usize) {
    assert!(
        profile.tb_execs.len() >= n,
        "total TBs {} < expected minimum {}",
        profile.tb_execs.len(),
        n
    );
}

/// Assert the profile metadata arch matches.
pub fn assert_arch(profile: &Profile, expected: &str) {
    let arch = profile
        .metadata
        .get("arch")
        .expect("profile missing 'arch' metadata");
    assert!(
        arch.contains(expected),
        "arch metadata '{}' does not contain '{}'",
        arch,
        expected
    );
}

/// Assert the profile has branch edges.
pub fn assert_has_edges(profile: &Profile) {
    assert!(
        !profile.branch_edges.is_empty(),
        "profile has no branch edges"
    );
}

/// Assert the profile has direct calls.
pub fn assert_has_calls(profile: &Profile) {
    assert!(
        !profile.direct_calls.is_empty(),
        "profile has no direct calls"
    );
}

/// Get the maximum branch edge count in the profile.
pub fn max_branch_edge_count(profile: &Profile) -> u64 {
    profile
        .branch_edges
        .iter()
        .map(|e| e.count)
        .max()
        .unwrap_or(0)
}

/// Get the maximum TB execution count in the profile.
pub fn max_tb_exec_count(profile: &Profile) -> u64 {
    profile
        .tb_execs
        .iter()
        .map(|t| t.exec_count)
        .max()
        .unwrap_or(0)
}

/// Helper macro to skip a test if a prerequisite is missing.
#[macro_export]
macro_rules! skip_if_missing {
    ($opt:expr, $msg:expr) => {
        match $opt {
            Some(v) => v,
            None => {
                eprintln!("SKIP: {}", $msg);
                return;
            }
        }
    };
}
