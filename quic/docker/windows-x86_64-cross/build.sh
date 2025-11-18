#!/usr/bin/env sh

SCIRPT_DIR="$(realpath "$(dirname "${0}")")"
readonly SCIRPT_DIR

# Points to the root directory of the qemu repo
BUILD_CONTEXT="${SCIRPT_DIR}/../../.."
readonly BUILD_CONTEXT

DOCKERFILE="${SCIRPT_DIR}/Dockerfile"
readonly DOCKERFILE

readonly TAG="docker-registry.qualcomm.com/qqvp/qemu:windows-x86_64-cross"

set -ex

docker build --file "${DOCKERFILE}" --tag "${TAG}" "${BUILD_CONTEXT}"
