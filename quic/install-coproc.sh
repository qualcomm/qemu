#!/usr/bin/env sh

# Copyright(c) 2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
# SPDX-License-Identifier: GPL-2.0-or-later

SCRIPT_DIR="$(dirname "$(realpath -e "${0}")")"
readonly SCRIPT_DIR

. "${SCRIPT_DIR}/help.sh"

HELP_MESSAGE=$(cat << EOF
Usage: $(basename "${0}") [OPTIONS]

Options:
    -b    qemu build directory
          (default: ${PWD}/build)
    -d    directory containing the coproc executable
          (default: ${PWD})
    -i    qemu install directory
          (default: ${PWD}/build/install)
    -h    print this help
EOF
)

readonly OPTIONS=":hb:d:i:"
while getopts "${OPTIONS}" option; do
    case "${option}" in
        "b") readonly BUILD_DIR="${OPTARG}";;
        "d") readonly COPROC_DIR="${OPTARG}";;
        "i") readonly INSTALL_DIR="${OPTARG}";;
        "h") print_help;;
        "?") print_help_error "Unknown option: -${OPTARG}";;
        ":") print_help_error "Option -${OPTARG} requires an argument";;
    esac
done

if [ -z "${COPROC_DIR}" ]; then
    readonly COPROC_DIR="${PWD}"
fi

if [ -z "${BUILD_DIR}" ]; then
    readonly BUILD_DIR="${PWD}/build"
fi

if [ -z "${INSTALL_DIR}" ]; then
    readonly INSTALL_DIR="${BUILD_DIR}/install"
fi

set -ex

readonly EXECUTABLE_NAME="coproc_rpc_remote"

# Find the coproc executable (with or without .exe extension)
EXECUTABLE=$(find "${COPROC_DIR}" -maxdepth 1 -name "${EXECUTABLE_NAME}*" -type f -print -quit)
readonly EXECUTABLE

if [ -z "${EXECUTABLE}" ] || ! [ -f "${EXECUTABLE}" ]; then
    echo "Missing executable '${EXECUTABLE_NAME}*' in directory '${COPROC_DIR}'"
    exit 1
fi

readonly BUNDLE_DIR="${BUILD_DIR}/qemu-bundle"

if [ ! -d "${BUILD_DIR}" ] || [ ! -d "${INSTALL_DIR}" ] || \
   [ ! -d "${BUNDLE_DIR}" ]; then
    echo "Couldn't find build and/or install dir to put the coproc at."
    exit 1
fi

cp "${EXECUTABLE}" "${BUILD_DIR}"

if [ -d "${INSTALL_DIR}/bin" ]; then
    cp "${EXECUTABLE}" "${INSTALL_DIR}/bin/"
else
    cp "${EXECUTABLE}" "${INSTALL_DIR}/"
fi

readonly QEMU_NAME="qemu-system-hexagon"

# Order matters for `-print` and `-quit`
BUNDLE_QEMU_EXECUTABLE="$(find "${BUNDLE_DIR}" -name "${QEMU_NAME}" -print -quit)"
readonly BUNDLE_QEMU_EXECUTABLE

if [ -n "${BUNDLE_QEMU_EXECUTABLE}" ]; then
    BUNDLE_BIN_DIR="$(dirname "${BUNDLE_QEMU_EXECUTABLE}")"
    readonly BUNDLE_BIN_DIR
    cp "${EXECUTABLE}" "${BUNDLE_BIN_DIR}"
fi
