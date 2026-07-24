#!/usr/bin/env bash

# Copyright(c) 2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
# SPDX-License-Identifier: GPL-2.0-or-later


set -euxo pipefail

mkdir -p "${QUIC_BUILD_DIR_ABS}"

{
    ./quic/build.sh configure "${QUIC_BUILD_CONFIG}"
    ./quic/build.sh build
    ./quic/build.sh install

    ./quic/create-tarball.sh
} 2>&1 | tee "${QUIC_BUILD_LOG}"
