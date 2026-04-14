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

[ $# -ge 1 ] || die "usage: branch [--based-on message-id] [git-publish options]...

Publish a QEMU series, using git-publish.
It ensures series apply cleanly on top of upstream, pass checkpatch.pl and
adds CI on GitHub.

Commits to include are taken from \${branch}_base..\${branch}.
In addition, it will push on origin a branch named \${USER}/\${branch}-ci
containing your series + CI patch from qemu-ci.

This script assumes you have some remotes setup, and will accordingly report an
error with what it's expecting if needed.

Workflow:
- develop some stuff
- create a \${USER}/\${branch} on top of it
- create a \${USER}/\${branch}_base on base commit
- ./publish.sh \${branch} [--based-on message-id]
- edit cover letter and abort before sending
- monitor CI for github branch (see link before editing cover letter)
- Once CI is green, call ./publish.sh \${branch} and send patches"

PULL_REQUEST=${PULL_REQUEST:-}
GITUSER=$USER
[ "$GITUSER" == "user" ] && GITUSER=pbouvier

branch_name=$1;shift
based_on=
if [ "${1:-}" == "--based-on" ]; then
    [ $# -ge 2 ] || die "usage: branch [--based-on message-id] [git-publish options]..."
    based_on=$2
    shift 2
fi

which git-publish > /dev/null || "missing req: sudo apt install -y git-publish"

current_branch=$(git branch --show-current)
patches=$(mktemp -d)
trap "rm -rf $patches; git checkout $current_branch >& /dev/null" EXIT

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

publish_base=upstream/master

# apply the base series before the requested branch
if [ -n "$based_on" ]; then
    b4 shazam "$based_on"
    publish_base=$(git rev-parse HEAD)
fi

# apply and check patches
if ! git am $patches/*; then
    git am --show-current-patch=diff
    git am --abort
    exit 1
fi
if [ "${SKIP_CHECKPATCH:-}" != 1 ]; then
    ./scripts/checkpatch.pl $(git merge-base upstream/master HEAD)..HEAD
fi

# check tags
if [ "$PULL_REQUEST" == "1" ]; then
    echo "--------------------------------------------------------"
    err=0
    for p in $patches/*; do
        if ! grep -q -i 'Reviewed-by:' $p > /dev/null; then
            echo ERROR: $p missing 'Reviewed-by: (ignore with SKIP_REVIEW=1)'
            if [ "${SKIP_REVIEW:-}" != 1 ]; then
                err=1
            fi
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

git checkout -b $branch-ci || git checkout $branch-ci
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
git push --force --set-upstream origin $branch-ci
echo "--------------------------------------------------------"

# Add Gitlab CI
git remote get-url gitlab ||
    die "missing 'gitlab' remote "\
        "(git remote add gitlab git@gitlab.com:<YOUR_USER>/qemu.git)"
if [ "$PULL_REQUEST" == "1" ]; then
    git push --force --set-upstream gitlab $branch-ci
    echo "--------------------------------------------------------"
fi

branch_links()
{
    echo "CI for $branch:"
    echo "- https://github.com/qualcomm/qemu/tree/$branch-ci"
    gitlab_user=$(git remote get-url gitlab | sed -e 's/.*://' -e 's#/.*##')
    echo "- https://gitlab.com/$gitlab_user/qemu/-/tree/$branch-ci"
    echo "--------------------------------------------------------"
}

branch_links

# publish (! pull request)
if [ "$PULL_REQUEST" != "1" ]; then
    git checkout $branch
    if ! git publish --base "$publish_base" "$@"; then
        exit 1
    fi
    echo "--------------------------------------------------------"
fi

branch_links

exit 0
