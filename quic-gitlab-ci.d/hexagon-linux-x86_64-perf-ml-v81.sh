#!/usr/bin/env sh

# Copyright(c) 2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
# SPDX-License-Identifier: GPL-2.0-or-later

set -ex

"${PWD}"/quic-gitlab-ci.d/clone-perf-repos.sh

TEST_CFG="${PWD}/quic-gitlab-ci.d/hex_nn_v3_v81.py"
ITERS="1"
if [ "${CI_PIPELINE_SOURCE}" = "schedule" ] || \
    [ "${CI_PIPELINE_SOURCE}" = "web" ]; then
    ITERS="3"
fi
readonly TEST_CFG
readonly ITERS

"${PWD}"/quic/run-perf.sh "${TEST_CFG}" "${ITERS}" perf-stat

"${PWD}"/quic/emit-metrics.sh \
    --input perf-results.json \
    --output metrics.txt || \
    printf '%s\n' "emit-metrics.sh: warning: failed to emit metrics.txt"

"${PWD}"/quic-gitlab-ci.d/compare-perf-baseline.sh \
    qemu-hexagon-linux-x86_64-perf-ml-v81
