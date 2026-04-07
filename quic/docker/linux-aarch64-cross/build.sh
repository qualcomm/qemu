#!/usr/bin/env sh

# Copyright(c) 2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
# SPDX-License-Identifier: GPL-2.0-or-later

SCRIPT_DIR="$(realpath "$(dirname "${0}")")"
readonly SCRIPT_DIR

# Points to the root directory of the qemu repo
BUILD_CONTEXT="${SCRIPT_DIR}/../../.."
readonly BUILD_CONTEXT

DOCKERFILE="${SCRIPT_DIR}/Dockerfile"
readonly DOCKERFILE

readonly TAG="docker-registry.qualcomm.com/qqvp/qemu:linux-aarch64-cross"

set -ex

docker build --file "${DOCKERFILE}" --tag "${TAG}" "${BUILD_CONTEXT}"
