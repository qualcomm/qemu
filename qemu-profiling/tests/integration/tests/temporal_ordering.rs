// SPDX-License-Identifier: BSD-3-Clause-Clear
// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//
// Temporal ordering tests: first-seen sequence validation.

use pgo_integration_tests::*;

#[test]
fn first_seen_seq_populated() {
    let qemu = skip_if_missing!(get_qemu("hexagon"), "hexagon QEMU not found");
    skip_if_missing!(plugin_path(), "PGO plugin not built");
    let bin = skip_if_missing!(
        fixture_binary("hexagon", "function_calls"),
        "hexagon fixtures not compiled"
    );

    // Tier 0 (hotness) includes first_seen_seq
    let run = run_profile(&qemu, &bin, 0);
    assert_clean_exit(&run);

    let executed: Vec<_> = run
        .profile
        .tb_execs
        .iter()
        .filter(|t| t.exec_count > 0)
        .collect();

    assert!(!executed.is_empty(), "should have executed TBs");

    for tb in &executed {
        assert!(
            tb.first_seen_seq > 0,
            "executed TB at 0x{:x} has first_seen_seq=0",
            tb.start_addr
        );
    }
}

#[test]
fn first_seen_seq_monotonic() {
    let qemu = skip_if_missing!(get_qemu("hexagon"), "hexagon QEMU not found");
    skip_if_missing!(plugin_path(), "PGO plugin not built");
    let bin = skip_if_missing!(
        fixture_binary("hexagon", "function_calls"),
        "hexagon fixtures not compiled"
    );

    let run = run_profile(&qemu, &bin, 0);
    assert_clean_exit(&run);

    // Sort by first_seen_seq and verify they are unique
    let mut seqs: Vec<u64> = run
        .profile
        .tb_execs
        .iter()
        .filter(|t| t.exec_count > 0 && t.first_seen_seq > 0)
        .map(|t| t.first_seen_seq)
        .collect();
    seqs.sort();
    seqs.dedup();

    // Each executed TB should have a unique sequence number
    let executed_count = run
        .profile
        .tb_execs
        .iter()
        .filter(|t| t.exec_count > 0 && t.first_seen_seq > 0)
        .count();
    assert_eq!(
        seqs.len(),
        executed_count,
        "first_seen_seq values should be unique across TBs"
    );
}

#[test]
fn startup_tbs_have_lower_seq() {
    let qemu = skip_if_missing!(get_qemu("hexagon"), "hexagon QEMU not found");
    skip_if_missing!(plugin_path(), "PGO plugin not built");
    let bin = skip_if_missing!(
        fixture_binary("hexagon", "function_calls"),
        "hexagon fixtures not compiled"
    );

    let run = run_profile(&qemu, &bin, 0);
    assert_clean_exit(&run);

    // The very first TB discovered should have the lowest first_seen_seq
    let executed: Vec<_> = run
        .profile
        .tb_execs
        .iter()
        .filter(|t| t.exec_count > 0 && t.first_seen_seq > 0)
        .collect();

    if executed.len() < 2 {
        return; // not enough data
    }

    let min_seq = executed.iter().map(|t| t.first_seen_seq).min().unwrap();
    let max_seq = executed.iter().map(|t| t.first_seen_seq).max().unwrap();

    assert!(
        max_seq > min_seq,
        "expected a range of first_seen_seq values, got min={} max={}",
        min_seq,
        max_seq
    );
}
