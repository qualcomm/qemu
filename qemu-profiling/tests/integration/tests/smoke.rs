// SPDX-License-Identifier: BSD-3-Clause-Clear
// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//
// Smoke tests: minimum viability of the PGO plugin.

use pgo_integration_tests::*;

#[test]
fn linear_edges_hexagon() {
    let qemu = skip_if_missing!(get_qemu("hexagon"), "hexagon QEMU not found");
    skip_if_missing!(plugin_path(), "PGO plugin not built");
    let bin = skip_if_missing!(
        fixture_binary("hexagon", "linear"),
        "hexagon fixtures not compiled"
    );

    let run = run_profile(&qemu, &bin, 1);

    assert_clean_exit(&run);
    assert!(
        run.profile.tb_execs.len() > 0,
        "expected at least one TB"
    );
    let total_exec: u64 = run.profile.tb_execs.iter().map(|t| t.exec_count).sum();
    assert!(total_exec > 0, "expected total_exec > 0");
}

#[test]
fn single_loop_edges_hexagon() {
    let qemu = skip_if_missing!(get_qemu("hexagon"), "hexagon QEMU not found");
    skip_if_missing!(plugin_path(), "PGO plugin not built");
    let bin = skip_if_missing!(
        fixture_binary("hexagon", "single_loop"),
        "hexagon fixtures not compiled"
    );

    let run = run_profile(&qemu, &bin, 1);

    assert_clean_exit(&run);
    assert!(
        run.profile.tb_execs.len() > 0,
        "expected at least one TB"
    );
    assert_has_edges(&run.profile);
}
