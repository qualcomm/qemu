// SPDX-License-Identifier: BSD-3-Clause-Clear
// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//
// Profile structure tests: header, footer, metadata validation.

use pgo_integration_tests::*;

#[test]
fn header_fields() {
    let qemu = skip_if_missing!(get_qemu("hexagon"), "hexagon QEMU not found");
    skip_if_missing!(plugin_path(), "PGO plugin not built");
    let bin = skip_if_missing!(
        fixture_binary("hexagon", "single_loop"),
        "hexagon fixtures not compiled"
    );

    let run = run_profile(&qemu, &bin, 1);
    let h = &run.profile.header;

    assert_eq!(h.format_version, 1, "format_version should be 1");
    assert_eq!(h.tier, 1, "tier should be 1 (edges)");
    assert_eq!(h.pointer_size, 8, "pointer_size should be 8");
    assert!(
        h.text_start < h.text_end,
        "text_start (0x{:x}) should be < text_end (0x{:x})",
        h.text_start,
        h.text_end
    );
}

#[test]
fn metadata_fields() {
    let qemu = skip_if_missing!(get_qemu("hexagon"), "hexagon QEMU not found");
    skip_if_missing!(plugin_path(), "PGO plugin not built");
    let bin = skip_if_missing!(
        fixture_binary("hexagon", "single_loop"),
        "hexagon fixtures not compiled"
    );

    let run = run_profile(&qemu, &bin, 1);

    assert_arch(&run.profile, "hexagon");

    let mode = run.profile.metadata.get("mode");
    assert!(
        mode.is_some(),
        "profile should have 'mode' metadata"
    );
    assert_eq!(
        mode.unwrap(),
        "linux-user",
        "mode should be 'linux-user'"
    );
}

#[test]
fn footer_fields() {
    let qemu = skip_if_missing!(get_qemu("hexagon"), "hexagon QEMU not found");
    skip_if_missing!(plugin_path(), "PGO plugin not built");
    let bin = skip_if_missing!(
        fixture_binary("hexagon", "single_loop"),
        "hexagon fixtures not compiled"
    );

    let run = run_profile(&qemu, &bin, 1);
    let footer = run
        .profile
        .footer
        .as_ref()
        .expect("profile should have footer");

    assert!(footer.clean_exit, "should be a clean exit");
    assert!(footer.wall_time_ns > 0, "wall_time_ns should be > 0");
    assert_eq!(
        footer.total_tb_count as usize,
        run.profile.tb_execs.len(),
        "footer.total_tb_count should match tb_execs.len()"
    );
    assert!(
        footer.total_exec_count > 0,
        "total_exec_count should be > 0"
    );
}
