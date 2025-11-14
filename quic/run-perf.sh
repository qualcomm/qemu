#!/usr/bin/env sh

# Copyright(c) 2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
# SPDX-License-Identifier: GPL-2.0-or-later

set -ex

readonly TEST_CFG="${1}"
readonly ITERS="${2}"
readonly INSPECTOR="${3}"
readonly SYS_ARGS="${4}"

readonly BENCH_DIR="${PWD}/qemu-hexagon-benchmarks"
readonly BUILD_DIR="${PWD}/build"
readonly OUTPUT_FILE="${PWD}/perf-results.json"

readonly EVENTS="\
instructions:u,cache-misses,cache-references,\
branch-misses,branches,iTLB-load-misses,iTLB-loads,dTLB-store-misses,\
dTLB-stores,dTLB-load-misses,dTLB-loads\
"

if test "${INSPECTOR}" = perf-stat
then
    echo test for perf - if this fails, check qqvp-dev kernel ver and perf \
         host sysctl
    perf stat --event "${EVENTS}" /bin/true
fi

./qemu-hexagon-perf/customer_test_suite.py \
    --bench-dir "${BENCH_DIR}" \
    --qemu-user-bin-dir "${BUILD_DIR}" \
    --qemu-bin-dir "${BUILD_DIR}" \
    --output-file "${OUTPUT_FILE}" \
    --test-cfg "${TEST_CFG}" \
    --iters "${ITERS}" \
    --inspector "${INSPECTOR}" \
    --sys-args="${SYS_ARGS}"
