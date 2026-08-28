#!/usr/bin/env sh

# Copyright(c) 2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
# SPDX-License-Identifier: GPL-2.0-or-later

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd -P)"
readonly SCRIPT_DIR

. "${SCRIPT_DIR}/util/help.sh"

HELP_MESSAGE="Usage: $(basename "${0}") [OPTIONS]

Options:
    -b    name of the build directory (default: ./build)
    -i    name of the install directory (default: ./build/install)
    -p    tarball prefix (default: none; tarball is named <tag-or-sha>.tar.gz)
    -v    tarball version (default: exact git tag or short SHA)
    -h    print this help"

readonly OPTIONS="hb:i:p:v:"
while getopts "${OPTIONS}" option; do
    case "${option}" in
        "b") readonly BUILD_DIR="${OPTARG}";;
        "i") readonly INSTALL_DIR="${OPTARG}";;
        "p") readonly TARBALL_PREFIX="${OPTARG}";;
        "v") readonly TARBALL_VERSION="${OPTARG}";;
        "h") print_help;;
        *) print_help_error "Unknown option";;
    esac
done

shift $((OPTIND-1))

[ -z "${BUILD_DIR}" ] && readonly BUILD_DIR="${PWD}/build"
[ -z "${INSTALL_DIR}" ] && readonly INSTALL_DIR="${BUILD_DIR}/install"

set -x

[ ! -d "${BUILD_DIR}" ] && print_help_error "Build dir missing"
[ ! -d "${INSTALL_DIR}" ] && print_help_error "Install dir missing"

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
