#!/usr/bin/env sh

set -ex

readonly JOB="${1}"
readonly OUTPUT_DIR="${2}"
readonly BASE_URL="${3}"
readonly TOKEN="${4}"
readonly REF_NAME="${5}"

readonly ARCHIVE="coproc.zip"
readonly ID=7631
readonly URL="${BASE_URL}/api/v4/projects/${ID}/jobs/artifacts/${REF_NAME}/download?job=${JOB}"

if [ -z "${JOB}" ] || [ -z "${BASE_URL}" ] || [ -z "${TOKEN}" ] || [ -z "${OUTPUT_DIR}" ]; then
    echo "Error: Missing argument"
    exit 1
fi

curl --location --output "${ARCHIVE}" --header "JOB-TOKEN: ${TOKEN}" "${URL}"
unzip -d "${OUTPUT_DIR}" "${ARCHIVE}"
rm -f "${ARCHIVE}"
