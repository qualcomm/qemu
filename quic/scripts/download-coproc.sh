#!/usr/bin/env sh

set -ex

readonly JOB="${1}"
readonly OUTPUT_DIR="${2}"
readonly BASE_URL="${3}"
readonly TOKEN="${4}"

readonly ARCHIVE="coproc.zip"
readonly ID=7631

if [ -z "${JOB}" ] || [ -z "${BASE_URL}" ] || [ -z "${TOKEN}" ] || [ -z "${OUTPUT_DIR}" ]; then
    echo "Error: Missing argument"
    exit 1
fi

# Determine if the currently checked out commit matches a tag
TAG="$(git describe --tags --exact-match --match qemu-hexagon-* 2>/dev/null || echo no)"
REF_NAME="main"
if test "${TAG}" != no
then
    REF_NAME="$(echo "qemu-coproc-plugin-${TAG}" | sed -e 's/qemu-hexagon-//')"
fi

readonly REF_NAME
readonly URL="${BASE_URL}/api/v4/projects/${ID}/jobs/artifacts/${REF_NAME}/download?job=${JOB}"

curl --location --output "${ARCHIVE}" --header "JOB-TOKEN: ${TOKEN}" "${URL}"
unzip -d "${OUTPUT_DIR}" "${ARCHIVE}"
rm -f "${ARCHIVE}"
