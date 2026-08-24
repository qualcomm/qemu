#!/usr/bin/env sh

# Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
# SPDX-License-Identifier: GPL-2.0-or-later

# Callers must set HELP_MESSAGE before invoking these functions.

print_help()
{
    [ -z "${HELP_MESSAGE:-}" ] && printf "Error: HELP_MESSAGE not set" >&2
    printf '%s\n' "${HELP_MESSAGE}"
    exit 0
}

print_help_error()
{
    [ -z "${1:-}" ] && printf "Error: Error message missing" >&2
    [ -z "${HELP_MESSAGE:-}" ] && "Error: HELP_MESSAGE not set" >&2
    printf 'Error: %s\n\n%s' "${1}" "${HELP_MESSAGE}" >&2
    exit 1
}
