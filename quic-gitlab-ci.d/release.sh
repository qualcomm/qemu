#!/usr/bin/env sh

# Copyright(c) 2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
# SPDX-License-Identifier: GPL-2.0-or-later

set -ex

readonly VERSION="${CI_COMMIT_TAG#qemu-hexagon-}"

readonly BASE_URL="${CI_PROJECT_URL}/-/jobs/artifacts/${CI_COMMIT_TAG}/raw/${QUIC_BUILD_DIR}"

derive_label()
{
    PLATFORM="${1#qemu-hexagon-}"
    case "${PLATFORM}" in
        *-*)
            OS="${PLATFORM%%-*}"
            ARCH="${PLATFORM#*-}"
            ;;
        *)
            OS="${PLATFORM}"
            ARCH=""
            ;;
    esac
    case "${OS}" in
        *[A-Z]*) ;;
        *) OS="$(printf '%c' "${OS}" | tr '[:lower:]' '[:upper:]')${OS#?}" ;;
    esac
    printf '%s\n' "${OS}${ARCH:+ ${ARCH}}"
}

ASSET_LINKS=""
for TARBALL in "${QUIC_BUILD_DIR_ABS}"/qemu-*-"${VERSION}".tar.gz; do
    [ -e "${TARBALL}" ] || continue
    NAME="$(basename "${TARBALL}")"
    JOB_BASE="${NAME%-"${VERSION}".tar.gz}"
    URL="${BASE_URL}/${NAME}?job=${JOB_BASE}-tag"
    LABEL="$(derive_label "${JOB_BASE}")"
    LINK="$(printf '{"name":"%s","url":"%s","link_type":"other"}' "${LABEL}" "${URL}")"
    ASSET_LINKS="${ASSET_LINKS:+${ASSET_LINKS},}${LINK}"
done

release-cli create \
    --tag-name "${CI_COMMIT_TAG}" \
    --name "QEMU Hexagon ${VERSION}" \
    --description "QEMU Hexagon ${VERSION}" \
    --assets-link "[${ASSET_LINKS}]"
