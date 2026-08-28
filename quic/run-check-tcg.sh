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
    -m    enable MTTCG
    -h    print this help"

readonly OPTIONS="hb:m"
while getopts "${OPTIONS}" option; do
    case "${option}" in
        "b") readonly BUILD_DIR="${OPTARG}";;
        "m") readonly MTTCG="-accel tcg,thread=multi";;
        "h") print_help;;
        *) print_help_error "Unknown option";;
    esac
done

shift $((OPTIND-1))

[ -z "${BUILD_DIR}" ] && readonly BUILD_DIR="${PWD}/build"

set -x

make --directory="${BUILD_DIR}" \
     --jobs="$(getconf _NPROCESSORS_ONLN)" \
     --no-print-directory \
     --output-sync \
     check-tcg \
     V=1 \
     OVERRIDE_OPTS="${MTTCG}"
