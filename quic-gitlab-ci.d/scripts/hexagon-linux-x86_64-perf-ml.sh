#!/usr/bin/env sh

# Copyright(c) 2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
# SPDX-License-Identifier: GPL-2.0-or-later

set -ex

"${PWD}"/quic-gitlab-ci.d/scripts/clone-perf-repos.sh

TEST_CFG="${PWD}/quic-gitlab-ci.d/hex_nn_v3.py"
ITERS="1"
if [ "${CI_PIPELINE_SOURCE}" = "schedule" ] || [ "${CI_PIPELINE_SOURCE}" = "web" ]; then
    ITERS="3"
fi
readonly TEST_CFG
readonly ITERS

"${PWD}"/quic/scripts/run-perf.sh "${TEST_CFG}" "${ITERS}" perf-stat
