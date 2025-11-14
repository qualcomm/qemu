#!/usr/bin/env sh

# Copyright(c) 2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
# SPDX-License-Identifier: GPL-2.0-or-later

set -ex

TEST_CASE="${1}"
if [ -z "${TEST_CASE}" ]; then
    echo "Error: Missing test case"
    exit 1
fi
readonly TEST_CASE

BUILD_DIR="${2}"
if [ ! -d "${BUILD_DIR}" ]; then
    BUILD_DIR="$(pwd)/build"
fi
readonly BUILD_DIR

QEMU_TEST_ALLOW_UNTRUSTED_CODE=1 \
"${BUILD_DIR}/pyvenv/bin/meson" test -C "${BUILD_DIR}" --no-rebuild --verbose \
    --print-errorlogs --suite func --suite func-quick --suite func-thorough \
    "${TEST_CASE}"
