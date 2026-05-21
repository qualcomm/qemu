// SPDX-License-Identifier: BSD-3-Clause-Clear
// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//
// Multi-architecture tests: verify profiles across architectures.

use pgo_integration_tests::*;

fn run_single_loop_for_arch(arch: &str, expected_arch_substr: &str) {
    let qemu = skip_if_missing!(
        get_qemu(arch),
        &format!("{} QEMU not found", arch)
    );
    skip_if_missing!(plugin_path(), "PGO plugin not built");
    let bin = skip_if_missing!(
        fixture_binary(arch, "single_loop"),
        &format!("{} fixtures not compiled", arch)
    );

    let run = run_profile(&qemu, &bin, 1);
    assert_clean_exit(&run);
    assert_arch(&run.profile, expected_arch_substr);
    assert_min_edge_count(&run.profile, 1000);
}

#[test]
fn single_loop_hexagon() {
    run_single_loop_for_arch("hexagon", "hexagon");
}

#[test]
fn single_loop_riscv64() {
    run_single_loop_for_arch("riscv64", "riscv");
}

#[test]
fn single_loop_aarch64() {
    run_single_loop_for_arch("aarch64", "aarch64");
}
