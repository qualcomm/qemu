#!/usr/bin/env sh

# Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
# SPDX-License-Identifier: GPL-2.0-or-later

set -eux

SCRIPT_DIR="$(cd "$(dirname "${0}")" && pwd -P)"
readonly SCRIPT_DIR

. "${SCRIPT_DIR}/../util/help.sh"

readonly HELP_MESSAGE="Usage: ${0}

Create and upload a Hexagon toolchain package."

[ -z "${GITLAB_TOOLCHAIN_UPLOAD_TOKEN}" ] \
    && print_help_error "GITLAB_TOOLCHAIN_UPLOAD_TOKEN missing"

readonly WORKDIR="/tmp/hexagon-toolchain-build"

rm -rf "${WORKDIR}"
mkdir -p "${WORKDIR}"
cd "${WORKDIR}"

"${SCRIPT_DIR}/fetch-hexagon-toolchains.sh"
"${SCRIPT_DIR}/add-linux-musl-support.sh"
"${SCRIPT_DIR}/pack-hexagon-toolchain.sh"
"${SCRIPT_DIR}/upload-hexagon-toolchain.sh"
