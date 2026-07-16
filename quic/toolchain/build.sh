#!/usr/bin/env sh
# SPDX-License-Identifier: GPL-2.0-or-later

set -eu

SCRIPT_DIR="$(cd "$(dirname "${0}")" && pwd)"
readonly SCRIPT_DIR

if [ ${#} -lt 1 ]; then
    printf "Usage: %s <revision> [<baremetal-ver>] [<musl-ver>]\n" "${0}" >&2
    exit 1
fi

HEXAGON_TOOLCHAIN_REVISION="${1}"
readonly HEXAGON_TOOLCHAIN_REVISION
BAREMETAL_VER="${2:-21.0.03}"
readonly BAREMETAL_VER
MUSL_VER="${3:-22.1.4}"
readonly MUSL_VER

readonly WORKDIR="/tmp/hexagon-toolchain-build"

if [ -z "${GITLAB_TOOLCHAIN_UPLOAD_TOKEN:-}" ]; then
    printf "Error: GITLAB_TOOLCHAIN_UPLOAD_TOKEN is not set.\n" >&2
    exit 1
fi

rm -rf "${WORKDIR}"
mkdir -p "${WORKDIR}"
cd "${WORKDIR}"

"${SCRIPT_DIR}/fetch-hexagon-toolchains.sh" "${BAREMETAL_VER}" "${MUSL_VER}"

"${SCRIPT_DIR}/add-linux-musl-support.sh" \
    "./${BAREMETAL_VER}/Tools" \
    "./clang+llvm-${MUSL_VER}-cross-hexagon-unknown-linux-musl/x86_64-linux-gnu"

"${SCRIPT_DIR}/pack-hexagon-toolchain.sh" "./${BAREMETAL_VER}"

"${SCRIPT_DIR}/upload-hexagon-toolchain.sh" \
    "./hexagon-toolchain-${BAREMETAL_VER}-extended.tar.zst" \
    "${HEXAGON_TOOLCHAIN_REVISION}"
