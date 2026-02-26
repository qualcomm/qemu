#!/usr/bin/env sh

# Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
# SPDX-License-Identifier: GPL-2.0-or-later

# Emit a Prometheus text-format metrics file from a perf-results.json.
# One line per valid (non-timed-out) run:
#
#   <label>{run="<n>"} <dur_sec>
#
# Usage:
#   ./emit-metrics.sh --input perf-results.json [--output metrics.txt]

INPUT=""
OUTPUT=""

while [ $# -gt 0 ]; do
    case "$1" in
        --input)  INPUT="$2";  shift 2 ;;
        --output) OUTPUT="$2"; shift 2 ;;
        *) echo "$0: unknown option: $1" >&2; exit 1 ;;
    esac
done

if [ -z "${INPUT}" ]; then
    echo "Usage: $0 --input perf-results.json [--output metrics.txt]" >&2
    exit 1
fi

readonly JQ_EXPR='
    .results[] |
    .label as $label |
    [.runs[] | select(.dur_sec)] |
    to_entries[] |
    "\($label){run=\"\(.key + 1)\"} \(.value.dur_sec)"
'

if [ -n "${OUTPUT}" ]; then
    jq -r "${JQ_EXPR}" "${INPUT}" > "${OUTPUT}"
else
    jq -r "${JQ_EXPR}" "${INPUT}"
fi
