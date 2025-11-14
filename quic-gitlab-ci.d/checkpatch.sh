#!/usr/bin/env sh

# shellcheck source=/dev/null
. "$(pwd)/quic-gitlab-ci.d/merge-request.sh"

set +e
set -x

error=0

for rev in $(git rev-list --first-parent "${TARGET_REF}".."${SOURCE_REF}")
do
    git rev-parse --verify -q "${rev}^2" >/dev/null && continue
    echo "========= Checking commit $(git log --pretty=reference "${rev}^!")"
    ./scripts/checkpatch.pl --color=always --no-signoff --branch "${rev}^!"
    error=$((error || ${?}))
    date
done

if test "${?}" -ne 0 || test "${error}" -ne 0
then
    exit 1
fi
