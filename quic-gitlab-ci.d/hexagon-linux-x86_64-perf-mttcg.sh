#!/usr/bin/env sh

# Copyright(c) 2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
# SPDX-License-Identifier: GPL-2.0-or-later

set -ex

"${PWD}"/quic-gitlab-ci.d/clone-perf-repos.sh

TEST_CFG="${PWD}/quic-gitlab-ci.d/mttcg_min_test_config.py"
ITERS="3"
if [ "${CI_PIPELINE_SOURCE}" = "schedule" ] || [ "${CI_PIPELINE_SOURCE}" = "web" ]; then
    TEST_CFG="${PWD}/quic-gitlab-ci.d/mttcg_full_test_config.py"
    ITERS="6"
fi
readonly TEST_CFG
readonly ITERS

readonly SYS_ARGS="-accel tcg,thread=multi"

"${PWD}"/quic/run-perf.sh "${TEST_CFG}" "${ITERS}" perf-stat "${SYS_ARGS}"

"${PWD}"/quic/emit-metrics.sh \
    --input perf-results.json \
    --output metrics.txt || \
    echo "emit-metrics.sh: warning: failed to emit metrics.txt"
