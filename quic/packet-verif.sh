#!/usr/bin/env bash

# Copyright(c) 2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
# SPDX-License-Identifier: GPL-2.0-or-later

set -e

readonly VERIF_REPO_NAME="qemu-hexagon-testing"
readonly VERIF_URL="https://github.com/qualcomm/${VERIF_REPO_NAME}.git"
readonly QEMU_ID=3968
readonly SERVER_URL="https://gitlab.qualcomm.com"

api() {
    local rel_url="${1}"
    local url="${SERVER_URL}/api/v4/${rel_url}"
    local script="${2}"
    echo "FETCHING '${url}'"
    out="$(curl --silent --header "JOB-TOKEN: ${CI_JOB_TOKEN}" "${url}")"
    {
        echo "FETCHING '${url}'"
        echo "GOT: ======"
        echo "${out}"
        echo "======="
    } >> "${VERIF_REPO_NAME}/curl.log"
    out="$(printf "%s" "${out}" | \
           python3 -c "import json, sys; data=json.load(sys.stdin);
${script}")"
    echo "post-processed out is '${out}'" >> "${VERIF_REPO_NAME}/curl.log"
}

print_help() {
    echo
    echo "Usage: $(basename "${0}") [OPTIONS]"
    echo
    echo "Options:"
    echo "    -j    job authentication token used by CI"
    echo "    -p    private authentication token used outside of CI"
}

readonly OPTIONS="hj:p:"
while getopts "${OPTIONS}" option; do
    case "${option}" in
        "j") readonly HEADER="JOB-TOKEN: ${OPTARG}";;
        "p") readonly HEADER="PRIVATE-TOKEN: ${OPTARG}";;
        "*") print_help; exit 1;;
    esac
done

if test -z "$HEADER"; then
    echo "Error: Either a job token or a private token need to be specified."
    print_help
    exit 1
fi

test -d qemu-hexagon-testing || git clone "${VERIF_URL}"

test -d base || {
    # Take the penultimate nightly (as we might be running the latest right now)
    api "projects/${QEMU_ID}/pipelines?ref=master&source=schedule&per_page=2" \
        'print(data[1]["id"])'

    api "projects/${QEMU_ID}/pipelines/${out}/jobs?per_page=100" \
    'for d in data:
        if d["name"] == "qemu-hexagon-linux-x86_64-schedule":
            print(d["id"])
            break'

    wget --no-verbose \
        "https://gitlab.qualcomm.com/qqvp/qemu/qemu/-/jobs/${out}/artifacts/download"
    unzip -q download -d base
}

toolchain="$(dirname "$(command -v hexagon-clang)")"
echo "using toolchain '${toolchain}'"

cd "${VERIF_REPO_NAME}/verif-hexagon"
./packet_verif verif \
    -n 100 -l 0 --exit-code \
    --iset "$(realpath ../../build/target/hexagon/iset.py)" \
    --toolchain-path "${toolchain}" \
    -b "$(realpath ../../base/build/qemu-system-hexagon)" \
    -q "$(realpath ../../build/qemu-system-hexagon)"
