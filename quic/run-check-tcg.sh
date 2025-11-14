#!/usr/bin/env sh

# Copyright(c) 2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
# SPDX-License-Identifier: GPL-2.0-or-later

BUILD_DIR="$(pwd)/build"

print_help()
{
    echo
    echo "Usage: $(basename "${0}") [OPTIONS]"
    echo
    echo "Options:"
    echo "    -b    name of the build directory"
    echo "          (default: ${BUILD_DIR})"
    echo "    -m    enable MTTCG"
    echo "    -h    print this help"
}

readonly OPTIONS="hb:m"
while getopts "${OPTIONS}" option; do
    case "${option}" in
        "b") readonly BUILD_DIR="${OPTARG}";;
        "m") readonly MTTCG="-accel tcg,thread=multi";;
        "h") print_help; exit 0;;
        "*") print_help; exit 1;;
    esac
done

shift $((OPTIND-1))

set -ex

make --directory="${BUILD_DIR}" \
     --jobs="$(getconf _NPROCESSORS_ONLN)" \
     --no-print-directory \
     --output-sync \
     check-tcg \
     V=1 \
     OVERRIDE_OPTS="${MTTCG}"
