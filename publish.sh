#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later

set -euo pipefail

die()
{
    echo "-----------------------------------------" 1>&2
    echo "$@" 1>&2
    echo "-----------------------------------------" 1>&2
    exit 1
}

[ $# -eq 1 ] || die "usage: branch [git-publish options]...

Publish a QEMU series, using git-publish.
It ensures series apply cleanly on top of upstream, pass checkpatch.pl and
adds CI on GitHub.

Commits to include are taken from \${branch}_base..\${branch}.
In addition, it will push on origin a branch named \${USER}/\${branch}-github-ci
containing your series + CI patch from qemu-ci.

This script assumes you have some remotes setup, and will accordingly report an
error with what it's expecting if needed.

Workflow:
- develop some stuff
- create a \${USER}/\${branch} on top of it
- create a \${USER}/\${branch}_base on base commit
- ./publish.sh \${branch}
- edit cover letter and abort before sending
- monitor CI for github branch (see link before editing cover letter)
- Once CI is green, call ./publish.sh \${branch} and send patches"

PULL_REQUEST=${PULL_REQUEST:-}
GITUSER=$USER

which git-publish > /dev/null || "missing req: sudo apt install -y git-publish"

current_branch=$(git branch --show-current)
patches=$(mktemp -d)
trap "rm -rf $patches; git checkout $current_branch >& /dev/null" EXIT

branch_name=$1;shift
branch="$GITUSER/$branch_name"
ci_branch="qemu-ci/ci"
base_revision=${branch}_base

# export patches
[ "$(git rev-parse ${branch})" ] || die "can't find ${branch} branch"
[ "$(git rev-parse ${base_revision})" ] || die "can't find ${base_revision} branch"
git format-patch ${base_revision}..${branch} -o $patches

# switch to series branch
branch="$GITUSER/series/$branch_name"

# create publish branch
git fetch -a upstream || die "missing 'upstream' remote (git remote add "\
                             "upstream https://gitlab.com/qemu-project/qemu)"

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

# fetch CI
git fetch -a qemu-ci || die "missing 'qemu-ci' remote "\
                            "(git remote add qemu-ci https://github.com/p-b-o/qemu-ci)"

git merge $ci_branch --squash --ff
mv .github/workflows/build.yml build.yml
git rm -f .github/workflows/*
mkdir -p .github/workflows/
mv build.yml .github/workflows/
git add .github
git commit -a -m 'ci' --signoff
git push --force --set-upstream origin $branch-github-ci
echo "--------------------------------------------------------"

# Add Gitlab CI
if [ "$PULL_REQUEST" == "1" ]; then
    git checkout -b $branch-gitlab-ci || git checkout $branch-gitlab-ci
    git reset --hard $branch
    git cherry-pick qemu-ci/gitlab_ci_full~1
    git remote get-url gitlab ||
        die "missing 'gitlab' remote "\
            "(git remote add gitlab git@gitlab.com:<YOUR_USER>/qemu.git)"
    git push --force --set-upstream gitlab $branch-gitlab-ci
    echo "--------------------------------------------------------"
fi

branch_links()
{
    echo "CI for $branch:"
    echo "- https://github.com/quic/qemu/tree/$branch-github-ci"
    if [ "$PULL_REQUEST" == "1" ]; then
        gitlab_user=$(git remote get-url gitlab | sed -e 's/.*://' -e 's#/.*##')
        echo "- https://gitlab.com/$gitlab_user/qemu/-/tree/$branch-gitlab-ci"
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
