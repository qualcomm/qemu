#!/usr/bin/env sh

# Copyright(c) 2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
# SPDX-License-Identifier: GPL-2.0-or-later

# This file is intended to be sourced from other shell scripts. It contains
# helper functions to print shell script help messages. `HELP_MESSAGE` needs to
# be defined prior to calling one of the helper functions.

print_help()
{
    set -eu
    set +x
    printf "%s\n" "${HELP_MESSAGE}"
    exit 0
}

print_help_error()
{
    set -eu
    set +x
    printf "ERROR: %s\n\n%s\n" "${1}" "${HELP_MESSAGE}"
    exit 1
}
