// SPDX-License-Identifier: BSD-3-Clause-Clear
// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//
// Multi-tier tests: verify tier-specific record types.

use pgo_integration_tests::*;

#[test]
fn tier0_has_tb_execs_no_edges() {
    let qemu = skip_if_missing!(get_qemu("hexagon"), "hexagon QEMU not found");
    skip_if_missing!(plugin_path(), "PGO plugin not built");
    let bin = skip_if_missing!(
        fixture_binary("hexagon", "function_calls"),
        "hexagon fixtures not compiled"
    );

    let run = run_profile(&qemu, &bin, 0);
    assert_clean_exit(&run);

    assert!(
        !run.profile.tb_execs.is_empty(),
        "tier 0 should have tb_execs"
    );
    assert!(
        run.profile.branch_edges.is_empty(),
        "tier 0 should not have branch_edges"
    );
    assert!(
        run.profile.fall_through_edges.is_empty(),
        "tier 0 should not have fall_through_edges"
    );
    assert!(
        run.profile.direct_calls.is_empty(),
        "tier 0 should not have direct_calls"
    );
}

#[test]
fn tier1_has_edges() {
    let qemu = skip_if_missing!(get_qemu("hexagon"), "hexagon QEMU not found");
    skip_if_missing!(plugin_path(), "PGO plugin not built");
    let bin = skip_if_missing!(
        fixture_binary("hexagon", "function_calls"),
        "hexagon fixtures not compiled"
    );

    let run = run_profile(&qemu, &bin, 1);
    assert_clean_exit(&run);

    assert!(
        !run.profile.tb_execs.is_empty(),
        "tier 1 should have tb_execs"
    );
    assert!(
        !run.profile.branch_edges.is_empty(),
        "tier 1 should have branch_edges"
    );
    assert!(
        !run.profile.fall_through_edges.is_empty(),
        "tier 1 should have fall_through_edges"
    );
}

#[test]
fn tier2_has_calls() {
    let qemu = skip_if_missing!(get_qemu("hexagon"), "hexagon QEMU not found");
    skip_if_missing!(plugin_path(), "PGO plugin not built");
    let bin = skip_if_missing!(
        fixture_binary("hexagon", "function_calls"),
        "hexagon fixtures not compiled"
    );

    let run = run_profile(&qemu, &bin, 2);
    assert_clean_exit(&run);

    assert!(
        !run.profile.tb_execs.is_empty(),
        "tier 2 should have tb_execs"
    );
    assert!(
        !run.profile.branch_edges.is_empty(),
        "tier 2 should have branch_edges"
    );
    assert_has_calls(&run.profile);
}

#[test]
fn tier_header_matches_request() {
    let qemu = skip_if_missing!(get_qemu("hexagon"), "hexagon QEMU not found");
    skip_if_missing!(plugin_path(), "PGO plugin not built");
    let bin = skip_if_missing!(
        fixture_binary("hexagon", "single_loop"),
        "hexagon fixtures not compiled"
    );

    for tier in 0..=2 {
        let run = run_profile(&qemu, &bin, tier);
        assert_eq!(
            run.profile.header.tier, tier,
            "header tier should match requested tier {}",
            tier
        );
    }
}
