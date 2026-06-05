#!/usr/bin/env sh
# SPDX-License-Identifier: GPL-2.0-or-later

set -eu

# Check for required argument
if [ ${#} -ne 1 ]; then
    printf "%s\n" "Usage: ${0} <tarball>" >&2
    exit 1
fi

readonly TARBALL="${1}"
TARBALL_BASENAME="$(basename "${TARBALL}")"
readonly TARBALL_BASENAME

# Verify tarball exists
if [ ! -f "${TARBALL}" ]; then
    printf "%s\n" "Error: Tarball not found: ${TARBALL}" >&2
    exit 1
fi

# Verify tarball ends with .tar.zst
if ! printf "%s" "${TARBALL_BASENAME}" | grep -q '\.tar\.zst$'; then
    printf "%s\n" "Error: must end with .tar.zst: ${TARBALL_BASENAME}" >&2
    exit 1
fi

# Derive version from tarball basename by stripping .tar.zst suffix
readonly VERSION="${TARBALL_BASENAME%.tar.zst}"

# Configuration
readonly GITLAB_URL="https://gitlab.qualcomm.com"
readonly PROJECT_ID="qqvp%2Fqemu%2Fqemu"
readonly PACKAGE_NAME="hexagon-toolchain"

# Check for GITLAB_TOOLCHAIN_UPLOAD_TOKEN
if [ -z "${GITLAB_TOOLCHAIN_UPLOAD_TOKEN:-}" ]; then
    printf "%s\n" \
        "Error: GITLAB_TOOLCHAIN_UPLOAD_TOKEN is not set." >&2
    printf "%s\n" \
        "  export GITLAB_TOOLCHAIN_UPLOAD_TOKEN=<token>" >&2
    exit 1
fi

# Build the registry URL
readonly PKG_BASE="${GITLAB_URL}/api/v4/projects/${PROJECT_ID}"
readonly PKG_PATH="packages/generic/${PACKAGE_NAME}/${VERSION}"
readonly UPLOAD_URL="${PKG_BASE}/${PKG_PATH}/${TARBALL_BASENAME}"

printf "%s\n" "Uploading to GitLab Package Registry..."
if curl -f \
    -H "PRIVATE-TOKEN: ${GITLAB_TOOLCHAIN_UPLOAD_TOKEN}" \
    --upload-file "${TARBALL}" \
    "${UPLOAD_URL}"; then
    printf "%s\n" ""
    printf "%s\n" "Upload successful!"
else
    printf "%s\n" "Error: Upload failed." >&2
    exit 1
fi

printf "%s\n" "Download URL: ${UPLOAD_URL}"
