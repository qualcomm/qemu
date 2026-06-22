#!/usr/bin/env sh

# Copyright(c) 2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
# SPDX-License-Identifier: GPL-2.0-or-later

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd -P)"
readonly SCRIPT_DIR

. "${SCRIPT_DIR}/util/help.sh"

HELP_MESSAGE="Usage: $(basename "${0}") [OPTIONS] COMMAND

Commands:
    build       Execute a build
    configure   Configure a build
    install     Install build artifacts
    list        List build configurations

Options:
    -b    build directory (default: ./build)
    -i    install directory (default: ./build/install)
    -h    print this help

Description:
    The install directory needs to be an absolute path. That means:
        1. If -i is used, it has to be an absolute path
        2. If -b is used and -i is NOT used, -b has to be an absolute path,
           because the default install directory is based on the build
           directory"

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
    readonly BUILD_DIR="${PWD}/build"
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
    HELP_MESSAGE="Usage: $(basename "${0}") ${COMMAND} CONFIG [EXTRA_ARGS...]

To see possible build configurations run: $(basename "${0}") list
EXTRA_ARGS are appended verbatim to the configure command."

    readonly CONFIGURATION="${1}"
    if [ -z "${CONFIGURATION}" ]; then
        print_help_error "Missing build configuration"
    fi

    shift

    readonly SOURCE_DIR="${SCRIPT_DIR}/.."
    . "${SCRIPT_DIR}/build-configs.sh"

    # Load the configuration, forwarding any extra configure args.
    ! load_config "${CONFIGURATION}" "${@}" \
        && print_help_error "Unknown build configuration: ${CONFIGURATION}"

    mkdir -p "${BUILD_DIR}"
    cd "${BUILD_DIR}" || exit 1

    set -x
    eval "${CONFIG_CMD}"
elif [ "${COMMAND}" = "build" ]; then
    make --directory "${BUILD_DIR}" --jobs "$(getconf _NPROCESSORS_ONLN)"
elif [ "${COMMAND}" = "install" ]; then
    make --directory "${BUILD_DIR}" --jobs "$(getconf _NPROCESSORS_ONLN)" \
        install
elif [ "${COMMAND}" = "list" ]; then
    # List all functions that start with "config_" with their descriptions
    # Descriptions are in "# desc: ..." comments right above the function
    awk '
        /^# desc:/ {
            desc = substr($0, 9)  # Skip "# desc: "
            next
        }
        /^config_[a-zA-Z0-9_]+\(\)/ {
            name = $0
            gsub(/^config_/, "", name)
            gsub(/\(\).*/, "", name)
            gsub(/_/, "-", name)
            if (desc != "") {
                printf "%-25s %s\n", name, desc
                desc = ""
            } else {
                print name
            }
        }
    ' "${SCRIPT_DIR}/build-configs.sh"
else
    print_help_error "Unknown command"
fi
