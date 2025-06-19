#!/usr/bin/env sh

# Copyright(c) 2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
# SPDX-License-Identifier: GPL-2.0-or-later

BUILD_DIR="$(pwd)/build"
PUBLISH_PATH="/prj/qct/llvm/target/vp_qemu_llvm/qemu_builds"

print_help()
{
    echo
    echo "Usage: $(basename "${0}") [OPTIONS]"
    echo
    echo "Options:"
    echo "    -b    name of the build directory"
    echo "          (default: ${BUILD_DIR})"
    echo "    -l    location to install to"
    echo "          (default: ${PUBLISH_PATH})"
    echo "    -h    print this help"
}

readonly OPTIONS="hb:l:"
while getopts "${OPTIONS}" option; do
    case "${option}" in
        "b") readonly BUILD_DIR="${OPTARG}";;
        "l") readonly PUBLISH_PATH="${OPTARG}";;
        "h") print_help; exit 0;;
        "*") print_help; exit 1;;
    esac
done

shift $((OPTIND-1))

set -ex

REL_PUBLISHDIR="build-$(date +%Y%m%d)"
readonly REL_PUBLISHDIR

readonly PUBLISHDIR="${PUBLISH_PATH}/${REL_PUBLISHDIR}"

mkdir --parents "${PUBLISHDIR}"
ln --force --symbolic "${REL_PUBLISHDIR}" "${PUBLISH_PATH}/build-latest"

tar -xf "${BUILD_DIR}/qemu-hexagon-*-.tar.gz" -C "${PUBLISHDIR}"

readonly UG_DIR="${PUBLISHDIR}/user_guide"
mkdir --parents "${UG_DIR}/pdf"

cp quic/user_guide/_build/html "${UG_DIR}/"
cp quic/user_guide/_build/latex/qualcommhexagonqemu-vpuserguide.pdf "${UG_DIR}/pdf/"

NR="$(find "${PUBLISH_PATH}" -mindepth 1 -maxdepth 1 | wc --lines)"
readonly NR

readonly MAX_PUBLISH_NR=90 # 3 months

if test "${NR}" -gt $((MAX_PUBLISH_NR + 1))
then
    TO_RM=$(( NR - MAX_PUBLISH_NR - 1 ))
    (
        cd "${PUBLISH_PATH}"
        for file in $(find "$(pwd)" -mindepth 1 -maxdepth 1 | head --lines="${TO_RM}")
        do
            echo "Removing old ${file}"
            rm --force --recursive "${file}"
        done
    )
fi
