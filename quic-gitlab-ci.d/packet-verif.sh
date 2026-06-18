#!/usr/bin/env bash

# Copyright(c) 2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
# SPDX-License-Identifier: GPL-2.0-or-later

set -e

readonly VERIF_REPO_NAME="verif-qemu-hexagon"
readonly PROJECT_URL="${CI_SERVER_HOST}/${CI_PROJECT_ROOT_NAMESPACE}"
readonly VERIF_URL="${PROJECT_URL}/testing/${VERIF_REPO_NAME}.git"

api() {
    local rel_url="${1}"
    local url="${CI_SERVER_URL}/api/v4/${rel_url}"
    local script="${2}"
    echo "FETCHING '${url}'"
    out="$(curl --silent --header "JOB-TOKEN: ${CI_JOB_TOKEN}" "${url}")"
    {
        echo "FETCHING '${url}'"
        echo "GOT: ======"
        echo "${out}"
        echo "======="
    } >> verif-qemu-hexagon/curl.log
    out="$(printf "%s" "${out}" | \
           python3 -c "import json, sys; data=json.load(sys.stdin);
${script}")"
    echo "post-processed out is '${out}'" >> verif-qemu-hexagon/curl.log
}

git clone https://gitlab-ci-token:"${CI_JOB_TOKEN}"@"${VERIF_URL}"

# Take the penultimate nightly (as we might be running the latest right now)
api "projects/${CI_PROJECT_ID}/pipelines?ref=master&source=schedule&per_page=2" \
    'print(data[1]["id"])'

api "projects/${CI_PROJECT_ID}/pipelines/${out}/jobs?per_page=100" \
'for d in data:
    if d["name"] == "qemu-hexagon-linux-x86_64-schedule":
        print(d["id"])
        break'

wget --no-verbose \
    "https://gitlab.qualcomm.com/qqvp/qemu/qemu/-/jobs/${out}/artifacts/download"
unzip -q download -d base

toolchain="$(dirname "$(command -v hexagon-clang)")"
echo "using toolchain '${toolchain}'"

cd "${VERIF_REPO_NAME}"
./packet_verif verif \
    -n 100 -l 0 --exit-code \
    --iset "$(realpath ../iset.py)" \
    --toolchain-path "${toolchain}" \
    -b "$(realpath ../base/build/qemu-system-hexagon)" \
    -q "$(realpath ../build/qemu-system-hexagon)"
