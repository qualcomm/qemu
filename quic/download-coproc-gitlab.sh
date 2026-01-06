#!/usr/bin/env sh

# Copyright(c) 2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
# SPDX-License-Identifier: GPL-2.0-or-later

ARCHIVE="${PWD}/coproc.zip"
OUTPUT_DIR="${PWD}"
SERVER_URL="https://gitlab.qualcomm.com"
readonly EXECUTABLE_NAME="coproc_rpc_remote"

print_help()
{
    echo
    echo "Usage: $(basename "${0}") [OPTIONS]" JOB REF_NAME
    echo
    echo "Options:"
    echo "    -a    name of the artifacts archive"
    echo "          (default: ${ARCHIVE})"
    echo "    -j    job authentication token used by CI"
    echo "    -o    directory to extract the archive to"
    echo "          (default: ${OUTPUT_DIR})"
    echo "    -p    private authentication token used outside of CI"
    echo "    -s    base url of the gitlab server"
    echo "          (default: ${SERVER_URL})"
    echo "    -h    print this help"
}

readonly OPTIONS="ha:j:o:p:s:"
while getopts "${OPTIONS}" option; do
    case "${option}" in
        "a") readonly ARCHIVE="${OPTARG}";;
        "j") readonly JOB_TOKEN="${OPTARG}";;
        "o") readonly OUTPUT_DIR="${OPTARG}";;
        "p") readonly PRIVATE_TOKEN="${OPTARG}";;
        "s") readonly SERVER_URL="${OPTARG}";;
        "h") print_help; exit 0;;
        "*") print_help; exit 1;;
    esac
done

shift $((OPTIND-1))

set -ex

if { [ -n "${PRIVATE_TOKEN}" ] && [ -n "${JOB_TOKEN}" ]; } ||
   { [ -z "${PRIVATE_TOKEN}" ] && [ -z "${JOB_TOKEN}" ]; }; then
    echo "Error: Either a job token or a private token need to be specified."
    print_help
    exit 1
fi

readonly JOB="${1}"
readonly REF_NAME="${2}"

if [ -z "${JOB}" ] || [ -z "${REF_NAME}" ]; then
    echo "Error: Missing positional argument."
    print_help
    exit 1
fi

if [ -n "${JOB_TOKEN}" ]; then
    readonly HEADER="JOB-TOKEN: ${JOB_TOKEN}"
elif [ -n "${PRIVATE_TOKEN}" ]; then
    readonly HEADER="PRIVATE-TOKEN: ${PRIVATE_TOKEN}"
fi

readonly ID=7631
readonly URL="\
${SERVER_URL}/api/v4/projects/${ID}/jobs/artifacts/${REF_NAME}/download?job=${JOB}"

curl --location --output "${ARCHIVE}" --header "${HEADER}" "${URL}"

# Find the executable inside the archive and extract it to $OUTPUT_DIR
readonly EXECUTABLE_PATH="$(unzip -l "${ARCHIVE}" | grep "${EXECUTABLE_NAME}" | sed 's/.* //')"
unzip -j -d "${OUTPUT_DIR}" "${ARCHIVE}" "${EXECUTABLE_PATH}"
rm -rf "${ARCHIVE}"
