#!/usr/bin/env bash
#
# Delete branches reported by list_old_branches.sh, both on origin and
# locally. The list is shown and confirmation is asked before deleting.
#
# Usage: cleanup_old_branches.sh [-y] [prefix]
#   -y      do not ask for confirmation
#   prefix  forwarded to list_old_branches.sh

set -euo pipefail

remote=origin
yes=false
if [ "${1:-}" = "-y" ]; then
    yes=true
    shift
fi

mapfile -t branches < <("$(dirname "$0")/list_old_branches.sh" "$@")

if [ ${#branches[@]} -eq 0 ]; then
    echo "no old branches"
    exit 0
fi

# Only delete on the remote the branches that actually exist there.
heads=$(git ls-remote --heads "$remote")
mapfile -t remote_heads < <(sed 's#.*refs/heads/##' <<< "$heads")
remote_branches=()
for branch in "${branches[@]}"; do
    for head in "${remote_heads[@]}"; do
        if [ "$branch" = "$head" ]; then
            remote_branches+=("$branch")
            break
        fi
    done
done

echo "local branches to delete:"
printf '  %s\n' "${branches[@]}"
echo "branches to delete on $remote:"
if [ ${#remote_branches[@]} -gt 0 ]; then
    printf '  %s\n' "${remote_branches[@]}"
fi

if ! $yes; then
    read -r -p "proceed? [y/N] " answer
    [ "$answer" = "y" ] || [ "$answer" = "Y" ] || exit 1
fi

if [ ${#remote_branches[@]} -gt 0 ]; then
    git push "$remote" --delete "${remote_branches[@]}"
fi
git branch -D "${branches[@]}"
