#!/usr/bin/env bash

# Trigger workflow_dispatch jobs on https://github.com/p-b-o/qemu-ci.
# Usage: ./trigger_ci_jobs.sh TOKEN [WORKFLOW...]
# TOKEN needs "Actions: read and write" permission on the repository.
# WORKFLOW defaults to: upstream new_series

set -euo pipefail

if [ $# -lt 1 ]; then
    echo "usage: $0 TOKEN [WORKFLOW...]" >&2
    exit 1
fi

token=$1
shift
workflows=("$@")
[ ${#workflows[@]} -gt 0 ] || workflows=(upstream new_series)

repo=p-b-o/qemu-ci
# workflows live on the default branch
ref=ci

ret=0
set -x
for w in "${workflows[@]}"; do
    w=${w%.yml}
    code=$(curl -sS -o /dev/stderr -w '%{http_code}' -X POST \
        -H 'Accept: application/vnd.github+json' \
        -H "Authorization: Bearer $token" \
        -H 'X-GitHub-Api-Version: 2022-11-28' \
        "https://api.github.com/repos/$repo/actions/workflows/$w.yml/dispatches" \
        -d "{\"ref\":\"$ref\"}")
    if [ "$code" = 204 ]; then
        echo "$w: triggered"
    else
        echo "$w: failed (HTTP $code)" >&2
        ret=1
    fi
done
exit $ret
