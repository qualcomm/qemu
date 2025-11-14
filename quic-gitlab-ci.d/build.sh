#!/usr/bin/env bash

# Copyright(c) 2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
# SPDX-License-Identifier: GPL-2.0-or-later

: "${QUIC_COPROC_JOB:=}"
: "${QUIC_COPROC_REF:=}"
: "${QUIC_INSTALL_SCRIPT:=}"
: "${QUIC_TARBALL_PREFIX:=}"

set -euxo pipefail

mkdir -p "${QUIC_BUILD_DIR}"

{
    ./quic/build.sh -b "${QUIC_BUILD_DIR}" configure "${QUIC_BUILD_CONFIG}"
    ./quic/build.sh -b "${QUIC_BUILD_DIR}" build
    ./quic/build.sh -b "${QUIC_BUILD_DIR}" install

    if [ -n "${QUIC_COPROC_JOB}" ] && [ -n "${QUIC_COPROC_REF}" ]; then
        ./quic/download-coproc-gitlab.sh -j "${CI_JOB_TOKEN}" \
            "${QUIC_COPROC_JOB}" "${QUIC_COPROC_REF}"
        ./quic/install-coproc.sh
    fi

    ./quic/create-tarball.sh -s "${QUIC_INSTALL_SCRIPT}" \
        -p "${QUIC_TARBALL_PREFIX}"
} 2>&1 | tee "${QUIC_BUILD_LOG}"
