#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later

set -euo pipefail

if [ $# -lt 1 ]; then
    echo "usage: branch [git-publish options]..."
    exit 1
fi

PULL_REQUEST=${PULL_REQUEST:-}
GITUSER=$USER

current_branch=$(git branch --show-current)
patches=$(mktemp -d)
trap "rm -rf $patches; git checkout $current_branch >& /dev/null" EXIT

branch_name=$1
branch="$GITUSER/$branch_name"; shift
ci_branch="$GITUSER/ci"
base_revision=${branch}_base

# export patches
git format-patch ${base_revision}..${branch} -o $patches

# switch to series branch
branch="$GITUSER/series/$branch_name"

# create publish branch
git fetch -a upstream
git checkout -b $branch || git checkout $branch
git reset --hard upstream/master

# apply and check patches
if ! git am $patches/*; then
    git am --show-current-patch=diff
    git am --abort
    exit 1
fi
./scripts/checkpatch.pl $(git merge-base upstream/master HEAD)..HEAD

# check tags
if [ "$PULL_REQUEST" == "1" ]; then
    echo "--------------------------------------------------------"
    err=0
    for p in $patches/*; do
        if ! grep -q -i 'Reviewed-by:' $p > /dev/null; then
            echo ERROR: $p missing 'Reviewed-by:'
            err=1
        fi

        if ! grep -q -i 'Link:' $p > /dev/null; then
            echo ERROR: $p missing 'Link:'
            err=1
        fi
    done

    if [ $err == 1 ]; then
        echo "ERROR: missing reviews or link"
        echo "--------------------------------------------------------"
        exit 1
    fi
    echo "--------------------------------------------------------"
fi

# add GitHub CI on top
echo "--------------------------------------------------------"
git checkout -b $branch-github-ci || git checkout $branch-github-ci
git reset --hard $branch
git merge $ci_branch --squash --ff
mv .github/workflows/build.yml build.yml
git rm -f .github/workflows/*
mkdir -p .github/workflows/
mv build.yml .github/workflows/
git add .github
git commit -a -m 'ci' --signoff
# TODO: remove once action is whitelisted
git cherry-pick $GITUSER/ci_remove_bsd_actions
git push --force --set-upstream origin $branch-github-ci
echo "--------------------------------------------------------"

# Add Gitlab CI
if [ "$PULL_REQUEST" == "1" ]; then
    git checkout -b $branch-gitlab-ci || git checkout $branch-gitlab-ci
    git reset --hard $branch
    git cherry-pick gitlab_ci_full~1
    git push --force --set-upstream gitlab $branch-gitlab-ci
    echo "--------------------------------------------------------"
fi

branch_links()
{
    echo "CI for $branch:"
    echo "- https://github.com/quic/qemu/tree/$branch-github-ci"
    if [ "$PULL_REQUEST" == "1" ]; then
        echo "- https://gitlab.com/p-b-o/qemu/-/tree/$branch-gitlab-ci"
    fi
    echo "--------------------------------------------------------"
}

branch_links

# publish (! pull request)
if [ "$PULL_REQUEST" != "1" ]; then
    git checkout $branch
    if ! git publish --base upstream/master "$@"; then
        exit 1
    fi
    echo "--------------------------------------------------------"
fi

branch_links

exit 0
