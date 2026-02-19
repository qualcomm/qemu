#!/usr/bin/env sh

# Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
# SPDX-License-Identifier: GPL-2.0-or-later

# Compare current perf-results.json against the baseline from the latest
# successful scheduled pipeline.  Produces perf-comparison.pdf via plot_perf.
#
# Usage: compare-perf-baseline.sh <ci-job-name>
#   e.g. compare-perf-baseline.sh hexagon-linux-x86_64-perf

set -e

JOB_NAME="${1:?Usage: compare-perf-baseline.sh <ci-job-name>}"
readonly JOB_NAME

# Best-effort: any failure just prints a message and exits cleanly.
fail_gracefully() {
    printf '%s\n' "compare-perf-baseline: ${1}"
    printf '%s\n' "Skipping baseline comparison."
    exit 0
}

if [ -z "${CI_JOB_TOKEN}" ]; then
    fail_gracefully "CI_JOB_TOKEN not set (not running in CI?)"
fi

if [ ! -f perf-results.json ]; then
    fail_gracefully "perf-results.json not found"
fi

# Helper: query GitLab API and post-process JSON response with Python.
api() {
    API_URL="${CI_SERVER_URL}/api/v4/${1}"
    API_SCRIPT="${2}"
    printf '%s\n' "compare-perf-baseline: GET ${API_URL}"
    RESPONSE="$(curl --silent --fail \
        --header "JOB-TOKEN: ${CI_JOB_TOKEN}" "${API_URL}")" || \
        fail_gracefully "API request failed: ${API_URL}"
    OUT="$(printf '%s' "${RESPONSE}" | \
        python3 -c "import json, sys; data = json.load(sys.stdin)
${API_SCRIPT}")" || \
        fail_gracefully "Failed to parse API response from ${API_URL}"
}

# 1. Find the latest successful scheduled pipeline.
#    For MR pipelines, compare against the target branch (e.g. master) where
#    scheduled baselines actually run, not the MR source branch.
if [ -n "${CI_MERGE_REQUEST_TARGET_BRANCH_NAME}" ]; then
    REF="${CI_MERGE_REQUEST_TARGET_BRANCH_NAME}"
else
    REF="${CI_COMMIT_REF_NAME:-master}"
fi
readonly REF
PIPE_QUERY="projects/${CI_PROJECT_ID}/pipelines"
PIPE_QUERY="${PIPE_QUERY}?ref=${REF}&source=schedule"
PIPE_QUERY="${PIPE_QUERY}&status=success&per_page=1"
readonly PIPE_QUERY
api "${PIPE_QUERY}" \
    'print(data[0]["id"] if data else "")'

PIPELINE_ID="${OUT}"
readonly PIPELINE_ID
if [ -z "${PIPELINE_ID}" ]; then
    fail_gracefully "No successful scheduled pipeline found for ref '${REF}'"
fi
printf '%s\n' "compare-perf-baseline: baseline pipeline ${PIPELINE_ID}"

# 2. Find the matching job in that pipeline.
api "projects/${CI_PROJECT_ID}/pipelines/${PIPELINE_ID}/jobs?per_page=100" \
"found = False
for d in data:
    if d[\"name\"] == \"${JOB_NAME}\" and d[\"status\"] == \"success\":
        print(d[\"id\"])
        found = True
        break
if not found:
    print(\"\")"

BASELINE_JOB_ID="${OUT}"
readonly BASELINE_JOB_ID
if [ -z "${BASELINE_JOB_ID}" ]; then
    fail_gracefully "No successful '${JOB_NAME}' job in pipeline ${PIPELINE_ID}"
fi
printf '%s\n' "compare-perf-baseline: baseline job ${BASELINE_JOB_ID}"

# 3. Download artifacts from the baseline job.
ARTIFACTS_URL="${CI_SERVER_URL}/api/v4"
ARTIFACTS_URL="${ARTIFACTS_URL}/projects/${CI_PROJECT_ID}"
ARTIFACTS_URL="${ARTIFACTS_URL}/jobs/${BASELINE_JOB_ID}/artifacts"
readonly ARTIFACTS_URL
printf '%s\n' \
    "compare-perf-baseline: downloading artifacts from job ${BASELINE_JOB_ID}"
curl --silent --fail \
    --header "JOB-TOKEN: ${CI_JOB_TOKEN}" \
    --output baseline-artifacts.zip \
    "${ARTIFACTS_URL}" || \
    fail_gracefully "Failed to download artifacts from job ${BASELINE_JOB_ID}"

# 4. Extract baseline perf-results.json.
unzip -qo baseline-artifacts.zip perf-results.json \
    -d baseline-artifacts/ || \
    fail_gracefully "perf-results.json not found in baseline artifacts"
mv baseline-artifacts/perf-results.json baseline-perf-results.json
rm -rf baseline-artifacts baseline-artifacts.zip

# 5. Ensure plot_perf dependencies are available.  The CI container may
#    already have them; only attempt install as a fallback.
if ! python3 -c "import seaborn, matplotlib, pandas" 2>/dev/null; then
    printf '%s\n' \
        "compare-perf-baseline: installing plot_perf dependencies"
    pip install --quiet seaborn matplotlib pandas 2>/dev/null || \
    pip install --quiet --break-system-packages seaborn matplotlib pandas 2>/dev/null || \
        printf '%s\n' \
            "compare-perf-baseline: warning: pip install failed, trying plot_perf anyway"
fi

# 6. Run plot_perf to generate comparison PDF.
TITLE="Pipeline ${CI_PIPELINE_ID:-local} vs scheduled baseline (pipeline ${PIPELINE_ID})"
readonly TITLE
"${PWD}"/qemu-hexagon-perf/plot_perf \
    -b baseline-perf-results.json \
    -n perf-results.json \
    -o perf-comparison.pdf \
    -t "${TITLE}" || \
    fail_gracefully "plot_perf failed"

printf '%s\n' \
    "compare-perf-baseline: perf-comparison.pdf generated successfully"

# 7. Run summarize_perf to generate text summary (no heavy deps needed).
"${PWD}"/quic/summarize_perf \
    -b baseline-perf-results.json \
    -n perf-results.json \
    -o perf-summary.md || \
    printf '%s\n' "compare-perf-baseline: warning: summarize_perf failed"

# 8. Post summary as MR comment (best-effort).
if [ -n "${CI_MERGE_REQUEST_IID}" ] && [ -f perf-summary.md ]; then
    MR_IID="${CI_MERGE_REQUEST_IID}"
    readonly MR_IID
    printf '%s\n' "compare-perf-baseline: posting summary to MR ${MR_IID}"
    COMMENT_BODY_JSON="$(python3 -c \
        "import sys, json; print(json.dumps(sys.stdin.read()))" \
        < perf-summary.md)"
    readonly COMMENT_BODY_JSON
    MR_URL="${CI_SERVER_URL}/api/v4"
    MR_URL="${MR_URL}/projects/${CI_PROJECT_ID}"
    MR_URL="${MR_URL}/merge_requests/${MR_IID}/notes"
    readonly MR_URL
    curl --request POST \
        --header "JOB-TOKEN: ${CI_JOB_TOKEN}" \
        --header "Content-Type: application/json" \
        --data "{\"body\": ${COMMENT_BODY_JSON}}" \
        "${MR_URL}" \
        >/dev/null 2>&1 || \
        printf '%s\n' "compare-perf-baseline: warning: MR comment failed"
fi
