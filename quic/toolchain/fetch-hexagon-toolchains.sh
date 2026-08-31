#!/usr/bin/env sh

# Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
# SPDX-License-Identifier: GPL-2.0-or-later

set -e

SCRIPT_DIR="$(cd "$(dirname "${0}")" && pwd)"
readonly SCRIPT_DIR

. "${SCRIPT_DIR}/../util/help.sh"
. "${SCRIPT_DIR}/versions.sh"

HELP_MESSAGE="Usage: ${0}

Fetch the baremetal and Linux/musl Hexagon toolchains.

Requires access to: /prj/qct/llvm/release/internal/HEXAGON"

readonly BAREMETAL_SRC_PATH="/prj/qct/llvm/release/internal/HEXAGON/\
${BAREMETAL_BRANCH}/linux64/${BAREMETAL_TOOLSET}"
[ ! -d "${BAREMETAL_SRC_PATH}" ] \
    &&  print_help_error "${BAREMETAL_SRC_PATH} not found."

mkdir -p "${BAREMETAL_DST_PATH}"

printf 'Copying baremetal toolchain %s to %s ...\n' \
    "${BAREMETAL_SRC_PATH}" "${BAREMETAL_DST_PATH}"
cp --archive "${BAREMETAL_SRC_PATH}/Tools/." "${BAREMETAL_DST_PATH}"

readonly MUSL_TARBALL="clang+llvm-${MUSL_VERSION}\
-cross-hexagon-unknown-linux-musl.tar.zst"
readonly MUSL_URL="https://artifacts.codelinaro.org/artifactory/\
codelinaro-toolchain-for-hexagon/${MUSL_VERSION}_/${MUSL_TARBALL}"

mkdir -p "${MUSL_DST_PATH}"

printf "Downloading musl toolchain %s to %s ...\n" \
    "${MUSL_VERSION}" "${MUSL_DST_PATH}/${MUSL_TARBALL}"
curl --silent --fail --show-error --output "${MUSL_DST_PATH}/${MUSL_TARBALL}" \
    --location "${MUSL_URL}"
tar --use-compress-program=zstd --extract --directory="${MUSL_DST_PATH}" \
    --file="${MUSL_DST_PATH}/${MUSL_TARBALL}" --strip-components=2
rm -rf "${MUSL_DST_PATH:?}/${MUSL_TARBALL:?}"
