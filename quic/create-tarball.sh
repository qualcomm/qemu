#!/usr/bin/env sh

# Copyright(c) 2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
# SPDX-License-Identifier: GPL-2.0-or-later

BUILD_DIR="$(pwd)/build"
INSTALL_DIR="${BUILD_DIR}/install"

print_help()
{
    echo
    echo "Usage: $(basename "${0}") [OPTIONS]"
    echo
    echo "Options:"
    echo "    -b    name of the build directory"
    echo "          (default: ${BUILD_DIR})"
    echo "    -i    name of the install directory"
    echo "          (default: ${INSTALL_DIR})"
    echo "    -p    tarball prefix"
    echo "    -h    print this help"
}

readonly OPTIONS="hb:i:p:"
while getopts "${OPTIONS}" option; do
    case "${option}" in
        "b") readonly BUILD_DIR="${OPTARG}";;
        "i") readonly INSTALL_DIR="${OPTARG}";;
        "p") readonly TARBALL_PREFIX="${OPTARG}";;
        "h") print_help; exit 0;;
        "*") print_help; exit 1;;
    esac
done

shift $((OPTIND-1))

set -ex

if [ ! -d "${BUILD_DIR}" ] || [ ! -d "${INSTALL_DIR}" ]; then
    echo "Error: Build artifacts missing"
    exit 1
fi

readonly RELEASE_NOTE="quic/RELEASE-NOTES.txt"
if [ -f "${RELEASE_NOTE}" ]; then
    cp "${RELEASE_NOTE}" "${INSTALL_DIR}"
fi

TAG_NAME="$(git describe --tags --exact-match --match qemu-hexagon* 2>/dev/null || \
           { printf "qemu-hexagon-" && git rev-parse --short HEAD; } )"
readonly TAG_NAME

readonly SRC_TARBALL_NAME="${TAG_NAME}-src.tar.gz"

# Convert absolute paths to relative for exclusions
BUILD_DIR_REL="$(realpath --relative-to="${PWD}" "${BUILD_DIR}")"
INSTALL_DIR_REL="$(realpath --relative-to="${PWD}" "${INSTALL_DIR}")"

tar --directory="${PWD}" \
    --create \
    --gzip \
    --file "${INSTALL_DIR}/${SRC_TARBALL_NAME}" \
    --exclude-vcs \
    --exclude="${BUILD_DIR_REL}" \
    --exclude="${INSTALL_DIR_REL}" \
    --exclude="__pycache__" \
    --exclude=".github" \
    --exclude=".gitlab" \
    --exclude="quic" \
    --exclude="quic-gitlab-ci.d" \
    --exclude="coproc_rpc_remote*" \
    .

readonly TARBALL_NAME="${TARBALL_PREFIX:+${TARBALL_PREFIX}-}${TAG_NAME}.tar.gz"
tar --directory="${INSTALL_DIR}" \
    --create \
    --gzip \
    --file "${BUILD_DIR}/${TARBALL_NAME}" \
    --exclude-vcs \
    .
