#!/usr/bin/env sh
# SPDX-License-Identifier: GPL-2.0-or-later

set -eu

if [ ${#} -ne 1 ]; then
    printf "%s\n" "Usage: ${0} <toolchain-dir>" >&2
    printf "%s\n" "" >&2
    printf "%s\n" "Pack a hexagon toolchain into <basename>-extended.tar.zst." >&2
    printf "%s\n" "<toolchain-dir> should be the directory containing Tools/" >&2
    printf "%s\n" "(typically the unpacked SDK release; basename is used as the version)." >&2
    exit 1
fi

readonly TOOLCHAIN_DIR="${1}"
VERSION="$(basename "${TOOLCHAIN_DIR}")"
readonly VERSION
readonly TARBALL="${VERSION}-extended.tar.zst"

if [ ! -d "${TOOLCHAIN_DIR}/Tools" ]; then
    printf "%s\n" "Error: ${TOOLCHAIN_DIR}/Tools not found" >&2
    exit 1
fi

if ! command -v zstd >/dev/null 2>&1; then
    printf "%s\n" "Error: zstd not found in PATH" >&2
    exit 1
fi

printf "%s\n" "Packing ${TOOLCHAIN_DIR}/Tools into ${TARBALL}..."
tar -I zstd -cf "${TARBALL}" -C "${TOOLCHAIN_DIR}" Tools

printf "%s\n" "Done: ${TARBALL}"
