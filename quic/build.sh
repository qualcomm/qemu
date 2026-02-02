#!/usr/bin/env sh

# Copyright(c) 2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
# SPDX-License-Identifier: GPL-2.0-or-later

set -e

SOURCE_DIR="$(cd "$(dirname "$0")/.." && pwd -P)"
readonly SOURCE_DIR

HELP_MESSAGE=$(cat << EOF
Usage: $(basename "${0}") [OPTIONS] COMMAND

Commands:
    build       Execute a build
    configure   Configure a build
    install     Install build artifacts
    list        List build configurations

Options:
    -b    build directory
          (default: ${SOURCE_DIR}/build)
    -i    install directory
          (default: ${SOURCE_DIR}/build/install)
    -h    print this help

Description:
    The install directory needs to be an absolute path. That means:
        1. If -i is used, it has to be an absolute path
        2. If -b is used and -i is NOT used, -b has to be an absolute path,
           because the default install directory is based on the build directory
EOF
)

print_help()
{
    set +x
    printf "%s\n" "${HELP_MESSAGE}"
    exit 0
}

print_help_error()
{
    set +x
    printf "ERROR: %s\n\n%s\n" "${1}" "${HELP_MESSAGE}"
    exit 1
}

readonly OPTIONS=":hb:i:s:"
while getopts "${OPTIONS}" OPTION; do
    case "${OPTION}" in
        "b") readonly BUILD_DIR="${OPTARG}";;
        "i") readonly INSTALL_DIR="${OPTARG}";;
        "h") print_help;;
        "*") print_help_error "Unknown option";;
    esac
done

if [ -z "${BUILD_DIR}" ]; then
    readonly BUILD_DIR="${SOURCE_DIR}/build"
fi

if [ -z "${INSTALL_DIR}" ]; then
    readonly INSTALL_DIR="${BUILD_DIR}/install"
fi

shift $((OPTIND-1))

readonly COMMAND="${1}"
if [ -z "${COMMAND}" ]; then
    print_help_error "Missing command"
fi

shift

if [ "${COMMAND}" = "configure" ]; then
    HELP_MESSAGE=$(cat << EOF
Usage: $(basename "${0}") ${COMMAND} CONFIG

To see possible build configurations run: $(basename "${0}") list
EOF
    )

    readonly CONFIGURATION="${1}"
    if [ -z "${CONFIGURATION}" ]; then
        print_help_error "Missing build configuration"
    fi

    set -u
    # shellcheck disable=1090
    . "${SOURCE_DIR}/quic/build-configs.sh"

    # Load the configuration
    if ! load_config "${CONFIGURATION}"; then
        print_help_error "Unknown build configuration: ${CONFIGURATION}"
    fi

    mkdir -p "${BUILD_DIR}"

    cd "${BUILD_DIR}" || exit 1

    set -x
    eval "${CONFIG_CMD}"
elif [ "${COMMAND}" = "build" ]; then
    make --directory "${BUILD_DIR}" --jobs "$(getconf _NPROCESSORS_ONLN)"
elif [ "${COMMAND}" = "install" ]; then
    make --directory "${BUILD_DIR}" --jobs "$(getconf _NPROCESSORS_ONLN)" install
elif [ "${COMMAND}" = "list" ]; then
    # List all functions that start with "config_"
    grep -E "^config_[a-zA-Z0-9_]+\(\)" "${SOURCE_DIR}/quic/build-configs.sh" \
        | sed 's/config_//' \
        | sed 's/().*//' \
        | tr '_' '-'
else
    print_help_error "Unknown command"
fi
