// SPDX-License-Identifier: BSD-3-Clause-Clear
// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//
// Build script: cross-compile C test fixtures for each target architecture.

use std::env;
use std::fs;
use std::path::{Path, PathBuf};
use std::process::Command;

struct ArchConfig {
    name: &'static str,
    env_var: &'static str,
    default_cc: &'static str,
    extra_flags: &'static [&'static str],
}

const ARCHES: &[ArchConfig] = &[
    ArchConfig {
        name: "hexagon",
        env_var: "HEXAGON_CC",
        default_cc: "hexagon-unknown-linux-musl-clang",
        extra_flags: &["-mv73"],
    },
    ArchConfig {
        name: "riscv64",
        env_var: "RISCV64_CC",
        default_cc: "riscv64-linux-gnu-gcc",
        extra_flags: &[],
    },
    ArchConfig {
        name: "aarch64",
        env_var: "AARCH64_CC",
        default_cc: "aarch64-linux-gnu-gcc",
        extra_flags: &[],
    },
];

fn find_compiler(arch: &ArchConfig) -> Option<String> {
    // Check env var first
    if let Ok(cc) = env::var(arch.env_var) {
        if compiler_works(&cc) {
            return Some(cc);
        }
    }
    // Try default
    if compiler_works(arch.default_cc) {
        return Some(arch.default_cc.to_string());
    }
    None
}

fn compiler_works(cc: &str) -> bool {
    Command::new(cc)
        .arg("--version")
        .output()
        .map(|o| o.status.success())
        .unwrap_or(false)
}

fn compile_fixture(
    cc: &str,
    source: &Path,
    output: &Path,
    extra_flags: &[&str],
) -> bool {
    let mut cmd = Command::new(cc);
    cmd.arg("-static")
        .arg("-O1")
        .arg("-fno-unroll-loops")
        .arg("-o")
        .arg(output);
    for flag in extra_flags {
        cmd.arg(flag);
    }
    cmd.arg(source);

    match cmd.output() {
        Ok(out) => {
            if !out.status.success() {
                let stderr = String::from_utf8_lossy(&out.stderr);
                println!(
                    "cargo:warning=Failed to compile {}: {}",
                    source.display(),
                    stderr.lines().next().unwrap_or("unknown error")
                );
                false
            } else {
                true
            }
        }
        Err(e) => {
            println!(
                "cargo:warning=Failed to run compiler for {}: {}",
                source.display(),
                e
            );
            false
        }
    }
}

fn main() {
    let out_dir = PathBuf::from(env::var("OUT_DIR").unwrap());
    let manifest_dir = PathBuf::from(env::var("CARGO_MANIFEST_DIR").unwrap());
    let fixtures_dir = manifest_dir.join("fixtures");

    // Rerun if fixtures change
    println!("cargo:rerun-if-changed=fixtures");
    for entry in fs::read_dir(&fixtures_dir).unwrap_or_else(|_| {
        panic!("fixtures directory not found at {}", fixtures_dir.display())
    }) {
        let entry = entry.unwrap();
        println!("cargo:rerun-if-changed={}", entry.path().display());
    }

    // Rerun if compiler env vars change
    for arch in ARCHES {
        println!("cargo:rerun-if-env-changed={}", arch.env_var);
    }

    // Collect C source files
    let sources: Vec<PathBuf> = fs::read_dir(&fixtures_dir)
        .unwrap()
        .filter_map(|e| e.ok())
        .map(|e| e.path())
        .filter(|p| p.extension().map(|e| e == "c").unwrap_or(false))
        .collect();

    for arch in ARCHES {
        let arch_out = out_dir.join(arch.name);
        fs::create_dir_all(&arch_out).unwrap();

        match find_compiler(arch) {
            Some(cc) => {
                println!(
                    "cargo:warning=Found {} compiler: {}",
                    arch.name, cc
                );
                let mut all_ok = true;
                for src in &sources {
                    let stem = src.file_stem().unwrap().to_str().unwrap();
                    let bin = arch_out.join(stem);
                    if !compile_fixture(&cc, src, &bin, arch.extra_flags) {
                        all_ok = false;
                    }
                }
                if all_ok {
                    println!(
                        "cargo:rustc-env=FIXTURE_DIR_{}={}",
                        arch.name.to_uppercase(),
                        arch_out.display()
                    );
                } else {
                    println!(
                        "cargo:rustc-cfg=skip_{}",
                        arch.name
                    );
                }
            }
            None => {
                println!(
                    "cargo:warning={} cross-compiler not found (set {} or install {}), skipping",
                    arch.name, arch.env_var, arch.default_cc
                );
                println!("cargo:rustc-cfg=skip_{}", arch.name);
            }
        }
    }
}
