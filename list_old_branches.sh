#!/usr/bin/env bash
#
# List local branches named $USER/* that are older than the merge-base of
# HEAD with upstream/master, i.e. branches whose own fork point from
# upstream/master is a strict ancestor of the current branch's fork point.
# Branches pointing exactly at that merge-base are listed as well.
#
# Usage: list_old_branches.sh [prefix]
#   prefix defaults to $USER (or the current login name if $USER is unset).

set -euo pipefail

prefix=${1:-${USER:-$(id -un)}}
upstream=upstream/master

base=$(git merge-base HEAD "$upstream")

git for-each-ref \
    --sort=-committerdate \
    --format='%(refname:short)' \
    "refs/heads/$prefix/" |
while read -r branch; do
    fork=$(git merge-base "$branch" "$upstream") || continue
    tip=$(git rev-parse "$branch")
    if { [ "$fork" != "$base" ] || [ "$tip" = "$base" ]; } &&
       git merge-base --is-ancestor "$fork" "$base"; then
        echo "$branch"
    fi
done
