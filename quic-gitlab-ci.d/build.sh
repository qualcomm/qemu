#!/usr/bin/env bash

# Copyright(c) 2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
# SPDX-License-Identifier: GPL-2.0-or-later

: "${QUIC_COPROC_JOB:=}"
: "${QUIC_COPROC_REF:=}"

set -euxo pipefail

mkdir -p "${QUIC_BUILD_DIR}"

{
    ./quic/build.sh configure "${QUIC_BUILD_CONFIG}"
    ./quic/build.sh build
    ./quic/build.sh install

    if [ -n "${QUIC_COPROC_JOB}" ] && [ -n "${QUIC_COPROC_REF}" ]; then
        ./quic/download-coproc-gitlab.sh -j "${CI_JOB_TOKEN}" \
            "${QUIC_COPROC_JOB}" "${QUIC_COPROC_REF}"

        ./quic/install-coproc.sh -d "${PWD}"
    fi

    if [ -n "${QUIC_TARBALL_PREFIX:-}" ]; then
        ./quic/create-tarball.sh -p "${QUIC_TARBALL_PREFIX}"
    else
        ./quic/create-tarball.sh
    fi
} 2>&1 | tee "${QUIC_BUILD_LOG}"
