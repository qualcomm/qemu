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

[ $# -ge 2 ] || die "usage: branch [message_ids]..."

from=$(git branch --show-current)
branch=$USER/$1; shift
git checkout -b $branch || git checkout $branch
git reset --hard upstream/master
while [ $# -ne 0 ]; do
    series=$1;shift
    b4 shazam $series
done

git merge qemu-ci/ci --squash --ff
mv .github/workflows/build.yml build.yml
git rm -f .github/workflows/*
mkdir -p .github/workflows/
mv build.yml .github/workflows/
git add .github
git commit -a -m 'ci' --signoff
git push -f --set-upstream origin $branch
git checkout $from

cat << EOF
----------
https://github.com/qualcomm/qemu/tree/$branch
---------
EOF
