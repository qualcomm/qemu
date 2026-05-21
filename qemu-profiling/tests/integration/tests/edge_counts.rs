// SPDX-License-Identifier: BSD-3-Clause-Clear
// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//
// Edge count tests: quantitative branch edge verification.

use pgo_integration_tests::*;
use std::collections::HashSet;

#[test]
fn single_loop_min_count() {
    let qemu = skip_if_missing!(get_qemu("hexagon"), "hexagon QEMU not found");
    skip_if_missing!(plugin_path(), "PGO plugin not built");
    let bin = skip_if_missing!(
        fixture_binary("hexagon", "single_loop"),
        "hexagon fixtures not compiled"
    );

    let run = run_profile(&qemu, &bin, 1);
    assert_clean_exit(&run);
    assert_min_edge_count(&run.profile, 1000);
}

#[test]
fn nested_loops_min_count() {
    let qemu = skip_if_missing!(get_qemu("hexagon"), "hexagon QEMU not found");
    skip_if_missing!(plugin_path(), "PGO plugin not built");
    let bin = skip_if_missing!(
        fixture_binary("hexagon", "nested_loops"),
        "hexagon fixtures not compiled"
    );

    let run = run_profile(&qemu, &bin, 1);
    assert_clean_exit(&run);
    assert_min_edge_count(&run.profile, 5000);
}

#[test]
fn tight_loop_min_count() {
    let qemu = skip_if_missing!(get_qemu("hexagon"), "hexagon QEMU not found");
    skip_if_missing!(plugin_path(), "PGO plugin not built");
    let bin = skip_if_missing!(
        fixture_binary("hexagon", "tight_loop"),
        "hexagon fixtures not compiled"
    );

    let run = run_profile(&qemu, &bin, 1);
    assert_clean_exit(&run);
    assert_min_edge_count(&run.profile, 100000);
}

#[test]
fn monotonicity_across_fixtures() {
    let qemu = skip_if_missing!(get_qemu("hexagon"), "hexagon QEMU not found");
    skip_if_missing!(plugin_path(), "PGO plugin not built");

    let single = skip_if_missing!(
        fixture_binary("hexagon", "single_loop"),
        "hexagon fixtures not compiled"
    );
    let nested = skip_if_missing!(
        fixture_binary("hexagon", "nested_loops"),
        "hexagon fixtures not compiled"
    );
    let tight = skip_if_missing!(
        fixture_binary("hexagon", "tight_loop"),
        "hexagon fixtures not compiled"
    );

    let run_single = run_profile(&qemu, &single, 1);
    let run_nested = run_profile(&qemu, &nested, 1);
    let run_tight = run_profile(&qemu, &tight, 1);

    let max_single = max_branch_edge_count(&run_single.profile);
    let max_nested = max_branch_edge_count(&run_nested.profile);
    let max_tight = max_branch_edge_count(&run_tight.profile);

    assert!(
        max_tight > max_nested,
        "tight ({}) should be > nested ({})",
        max_tight,
        max_nested
    );
    assert!(
        max_nested > max_single,
        "nested ({}) should be > single ({})",
        max_nested,
        max_single
    );
}

#[test]
fn function_calls_tb_exec() {
    let qemu = skip_if_missing!(get_qemu("hexagon"), "hexagon QEMU not found");
    skip_if_missing!(plugin_path(), "PGO plugin not built");
    let bin = skip_if_missing!(
        fixture_binary("hexagon", "function_calls"),
        "hexagon fixtures not compiled"
    );

    let run = run_profile(&qemu, &bin, 1);
    assert_clean_exit(&run);
    assert!(
        max_tb_exec_count(&run.profile) >= 200,
        "max TB exec count should be >= 200"
    );
}

#[test]
fn if_else_chain_multiple_targets() {
    let qemu = skip_if_missing!(get_qemu("hexagon"), "hexagon QEMU not found");
    skip_if_missing!(plugin_path(), "PGO plugin not built");
    let bin = skip_if_missing!(
        fixture_binary("hexagon", "if_else_chain"),
        "hexagon fixtures not compiled"
    );

    let run = run_profile(&qemu, &bin, 1);
    assert_clean_exit(&run);

    // Collect distinct branch edge destinations
    let destinations: HashSet<u64> = run
        .profile
        .branch_edges
        .iter()
        .map(|e| e.to)
        .collect();

    assert!(
        destinations.len() >= 3,
        "expected at least 3 distinct branch targets, got {}",
        destinations.len()
    );
}
