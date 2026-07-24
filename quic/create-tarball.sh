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
    echo "    -p    tarball prefix (default: none; tarball is named <tag-or-sha>.tar.gz)"
    echo "    -v    tarball version (default: exact git tag or short SHA)"
    echo "    -h    print this help"
}

readonly OPTIONS="hb:i:p:v:"
while getopts "${OPTIONS}" option; do
    case "${option}" in
        "b") readonly BUILD_DIR="${OPTARG}";;
        "i") readonly INSTALL_DIR="${OPTARG}";;
        "p") readonly TARBALL_PREFIX="${OPTARG}";;
        "v") readonly TARBALL_VERSION="${OPTARG}";;
        "h") print_help; exit 0;;
        *) print_help; exit 1;;
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

if [ -n "${TARBALL_VERSION:-}" ]; then
    TAG_OR_SHA="${TARBALL_VERSION}"
else
    TAG_OR_SHA="$(git describe --tags --exact-match 2>/dev/null \
                      || git rev-parse --short HEAD)"
fi
readonly TAG_OR_SHA

readonly TARBALL_NAME="${TARBALL_PREFIX:+${TARBALL_PREFIX}-}${TAG_OR_SHA}.tar.gz"
readonly SRC_TARBALL_NAME="${TARBALL_PREFIX:+${TARBALL_PREFIX}-}${TAG_OR_SHA}-src.tar.gz"

relative_realpath() {
    test $# -eq 2 || { echo "relative_realpath usage error"; exit 1; }
    python3 -c "import os, sys; print(os.path.relpath(os.path.realpath('$2'), start=os.path.realpath('$1')))"
}

# Convert absolute paths to relative for exclusions
BUILD_DIR_REL="$(relative_realpath "${PWD}" "${BUILD_DIR}")"
INSTALL_DIR_REL="$(relative_realpath "${PWD}" "${INSTALL_DIR}")"

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
    .

tar --directory="${INSTALL_DIR}" \
    --create \
    --gzip \
    --file "${BUILD_DIR}/${TARBALL_NAME}" \
    --exclude-vcs \
    .
