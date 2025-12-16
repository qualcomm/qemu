#!/usr/bin/env sh

# Copyright(c) 2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
# SPDX-License-Identifier: GPL-2.0-or-later

set -ex

"${PWD}"/quic-gitlab-ci.d/clone-perf-repos.sh

readonly TEST_CFG="${PWD}/quic-gitlab-ci.d/hex_nn_v3.py"

ITERS="1"
if [ "${CI_PIPELINE_SOURCE}" = "schedule" ]; then
    ITERS="3"
fi
readonly ITERS

readonly BENCH_DIR="${PWD}/qemu-hexagon-benchmarks"
readonly BUILD_DIR="${PWD}/build"
readonly OUTPUT_FILE="${PWD}/perf-results.json"

"${PWD}"/qemu-hexagon-perf/customer_test_suite.py \
    --bench-dir "${BENCH_DIR}" \
    --qemu-bin-dir "${BUILD_DIR}" \
    --qemu-user-bin-dir "none" \
    --output-file "${OUTPUT_FILE}" \
    --test-cfg "${TEST_CFG}" \
    --iters "${ITERS}"
