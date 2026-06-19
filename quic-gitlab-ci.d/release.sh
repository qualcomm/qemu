#!/usr/bin/env sh

# Copyright(c) 2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
# SPDX-License-Identifier: GPL-2.0-or-later

set -ex

readonly VERSION="${CI_COMMIT_TAG#qemu-hexagon-}"

readonly BASE_URL="${CI_PROJECT_URL}/-/jobs/artifacts/${CI_COMMIT_TAG}/raw/${QUIC_BUILD_DIR}"

ASSET_LINKS=""
for TARBALL in "${QUIC_BUILD_DIR_ABS}"/qemu-*-"${VERSION}".tar.gz; do
    [ -e "${TARBALL}" ] || continue
    NAME="$(basename "${TARBALL}")"
    JOB_BASE="${NAME%-"${VERSION}".tar.gz}"
    URL="${BASE_URL}/${NAME}?job=${JOB_BASE}-tag"
    LINK="$(printf '{"name":"%s","url":"%s","link_type":"other"}' "${NAME}" "${URL}")"
    ASSET_LINKS="${ASSET_LINKS:+${ASSET_LINKS},}${LINK}"
done

release-cli create \
    --tag-name "${CI_COMMIT_TAG}" \
    --name "Release ${CI_COMMIT_TAG}" \
    --description "Release ${CI_COMMIT_TAG}" \
    --assets-link "[${ASSET_LINKS}]"
