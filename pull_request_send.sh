#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later

set -euo pipefail

if [ $# -ne 2 ]; then
    echo "usage: branch subject"
    exit 1
fi

patches=$(mktemp -d)
trap "rm -rf $patches" EXIT

GITUSER=$USER

branch_name="$1";shift
subject="$1";shift
branch="$GITUSER/series/$branch_name"
tag="$GITUSER/pr/${branch_name}-$(date -I | sed -e 's/-//g')"

base=$(git merge-base upstream/master $branch)

git tag --sign --force $tag $branch
git push gitlab --force $tag

git format-patch -o "$patches" $base..$branch \
    --subject-prefix=PULL --numbered --cover-letter
COVERLETTER="$patches/0000-cover-letter.patch"
sed -i -e 's@^Subject: \[PULL\(.*\)].*@Subject: [PULL\1] '"$subject"'@;/^$/q' \
    "$COVERLETTER"
git request-pull upstream/master "https://gitlab.com/p-b-o/qemu" "$tag" >>"$COVERLETTER"
vi "$COVERLETTER"
git send-email $patches --8bit-encoding=UTF-8 \
    --to qemu-devel@nongnu.org \
    --to peter.maydell@linaro.org \
    --to richard.henderson@linaro.org \
    --to pbonzini@redhat.com \
    --to stefanha@redhat.com \
    --cc pierrick.bouvier@oss.qualcomm.com \
    --suppress-cc=all
