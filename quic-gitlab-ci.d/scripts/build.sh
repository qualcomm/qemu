#!/usr/bin/env sh

# Copyright(c) 2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
# SPDX-License-Identifier: GPL-2.0-or-later

print_help()
{
    echo
    echo "Usage: $(basename "${0}") [OPTIONS]" QEMU_TARGET
    echo
    echo "Options:"
    echo "    -j    coproc CI job to download from"
    echo "    -r    coproc revision to download"
    echo "    -p    tarball prefix"
    echo "    -s    script for installing custom dependencies"
    echo "    -h    print this help"
    echo
    echo "Downloading a coproc requires both -j and -r to be set"
}

readonly OPTIONS="hj:r:p:s:"
while getopts "${OPTIONS}" option; do
    case "${option}" in
        "j") readonly COPROC_JOB="${OPTARG}";;
        "r") readonly COPROC_REF="${OPTARG}";;
        "p") readonly TARBALL_PREFIX="${OPTARG}";;
        "s") readonly INSTALL_SCRIPT="${OPTARG}";;
        "h") print_help; exit 0;;
        "*") print_help; exit 1;;
    esac
done

if [ -n "${COPROC_JOB}" ] && [ -n "${COPROC_REF}" ]; then
    readonly DOWNLOAD_COPROC="download coproc"
fi

shift $((OPTIND-1))

readonly QEMU_TARGET="${1}"

if [ -z "${QEMU_TARGET}" ]; then
    echo "Error: Missing positional argument"
    print_help
    exit 1
fi

set -ex

./quic/scripts/build-qemu.sh "${QEMU_TARGET}"

if [ -n "${DOWNLOAD_COPROC}" ]; then
    ./quic/scripts/download-coproc-gitlab.sh \
        -j "${CI_JOB_TOKEN}" "${COPROC_JOB}" "${COPROC_REF}"
    ./quic/scripts/install-coproc.sh
fi

./quic/scripts/create-tarball.sh -s "${INSTALL_SCRIPT}" -p "${TARBALL_PREFIX}"
