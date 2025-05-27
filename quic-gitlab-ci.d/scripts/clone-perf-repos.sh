#!/usr/bin/env sh

# Copyright(c) 2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
# SPDX-License-Identifier: GPL-2.0-or-later

set -ex

URL="$(echo "${CI_SERVER_URL}" | sed -e 's/^.*:\/\///g' -e 's/:[0-9]*$//g')"
readonly URL

readonly PROJECT_URL="${URL}/${CI_PROJECT_ROOT_NAMESPACE}"
readonly HEXAGON_PERF="${PROJECT_URL}/testing/qemu-hexagon-perf.git"
readonly HEXAGON_BENCHMARK="${PROJECT_URL}/testing/qemu-hexagon-benchmarks.git"

git clone https://gitlab-ci-token:"${CI_JOB_TOKEN}"@"${HEXAGON_PERF}"
git clone https://gitlab-ci-token:"${CI_JOB_TOKEN}"@"${HEXAGON_BENCHMARK}"

