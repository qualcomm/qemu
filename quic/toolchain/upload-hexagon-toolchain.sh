#!/usr/bin/env sh

# Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
# SPDX-License-Identifier: GPL-2.0-or-later

set -e

SCRIPT_DIR="$(cd "$(dirname "${0}")" && pwd)"
readonly SCRIPT_DIR

. "${SCRIPT_DIR}/../util/help.sh"
. "${SCRIPT_DIR}/versions.sh"

readonly HELP_MESSAGE="Usage: ${0}

Requires GITLAB_TOOLCHAIN_UPLOAD_TOKEN to be set."

[ -z "${GITLAB_TOOLCHAIN_UPLOAD_TOKEN}" ] \
    && print_help_error "GITLAB_TOOLCHAIN_UPLOAD_TOKEN missing"

[ ! -f "${EXTENDED_TARBALL}" ] \
    && print_help_error "Tarball not found: ${EXTENDED_TARBALL}"

! printf "%s" "${EXTENDED_TARBALL}" | grep -q '\.tar\.zst$' \
    && print_help_error "Tarball must end with .tar.zst: ${EXTENDED_TARBALL}"

readonly GITLAB_URL="https://gitlab.qualcomm.com"
readonly PROJECT_ID="qqvp%2Fqemu%2Fqemu"
readonly PACKAGE_NAME="${EXTENDED_TARBALL%.tar.zst}"
readonly PACKAGE_BASE="${GITLAB_URL}/api/v4/projects/${PROJECT_ID}"
readonly PACKAGE_PATH="packages/generic/${PACKAGE_NAME}/${EXTENDED_REVISION}"
readonly UPLOAD_URL="${PACKAGE_BASE}/${PACKAGE_PATH}/${EXTENDED_TARBALL}"

printf "%s\n" "Uploading to GitLab Package Registry ..."
! curl --fail --header "PRIVATE-TOKEN: ${GITLAB_TOOLCHAIN_UPLOAD_TOKEN}" \
    --upload-file "${EXTENDED_TARBALL}" "${UPLOAD_URL}" \
    && print_help_error "Upload failed: ${UPLOAD_URL}"
