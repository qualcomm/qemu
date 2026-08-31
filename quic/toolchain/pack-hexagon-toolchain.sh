#!/usr/bin/env sh

# Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
# SPDX-License-Identifier: GPL-2.0-or-later

set -e

SCRIPT_DIR="$(cd "$(dirname "${0}")" && pwd)"
readonly SCRIPT_DIR

. "${SCRIPT_DIR}/../util/help.sh"
. "${SCRIPT_DIR}/versions.sh"

HELP_MESSAGE="Usage: ${0}

Pack a hexagon toolchain into ${EXTENDED_TARBALL}"

! command -v zstd >/dev/null 2>&1 \
    && print_help_error "zstd not found in PATH"

printf "%s\n" "Packing ${BAREMETAL_DST_PATH} into ${EXTENDED_TARBALL} ..."
tar --owner=0 --group=0 --numeric-owner --mode='u+rwX,go+rX' \
    --use-compress-program=zstd --create --file="${EXTENDED_TARBALL}" \
    --directory="${BAREMETAL_DST_PATH}" .
