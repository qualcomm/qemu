#!/usr/bin/env sh

# Copyright(c) 2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
# SPDX-License-Identifier: GPL-2.0-or-later

readonly EXECUTABLE_NAME="coproc_rpc_remote"

EXECUTABLE="$(pwd)/${EXECUTABLE_NAME}"
BUILD_DIR="$(pwd)/build"
INSTALL_DIR="${BUILD_DIR}/install/bin"

print_help()
{
    echo "Usage: $(basename "${0}") [OPTIONS]"
    echo
    echo "Options:"
    echo "    -b    qemu build directory"
    echo "          (default: ${BUILD_DIR})"
    echo "    -e    location of the coproc executable"
    echo "          (default: ${EXECUTABLE})"
    echo "    -i    qemu install directory"
    echo "          (default: ${INSTALL_DIR})"
    echo "    -h    print this help"
}

readonly OPTIONS="hb:e:i:"
while getopts "${OPTIONS}" option; do
    case "${option}" in
        "b") readonly BUILD_DIR="${OPTARG}";;
        "e") readonly EXECUTABLE="${OPTARG}";;
        "i") readonly INSTALL_DIR="${OPTARG}";;
        "k") print_help; exit 0;;
        "*") print_help; exit 1;;
    esac
done

set -ex

if [ -f "${EXECUTABLE}" ];  then
    if [ -d "${BUILD_DIR}" ]; then
        cp "${EXECUTABLE}" "${BUILD_DIR}"
    fi

    if [ -d "${INSTALL_DIR}" ]; then
        cp "${EXECUTABLE}" "${INSTALL_DIR}"
    fi

    readonly BUNDLE_DIR="${BUILD_DIR}/qemu-bundle"
    if [ -d "${BUNDLE_DIR}" ]; then
        readonly QEMU_NAME="qemu-system-hexagon"

        # Order matters for `-print` and `-quit`
        BUNDLE_QEMU_EXECUTABLE="$(find "${BUNDLE_DIR}" -name "${QEMU_NAME}" -print -quit)"
        readonly BUNDLE_QEMU_EXECUTABLE

        if [ -n "${BUNDLE_QEMU_EXECUTABLE}" ]; then
            BUNDLE_BIN_DIR="$(dirname "${BUNDLE_QEMU_EXECUTABLE}")"
            readonly BUNDLE_BIN_DIR
            cp "${EXECUTABLE}" "${BUNDLE_BIN_DIR}"
        fi
    fi
fi

