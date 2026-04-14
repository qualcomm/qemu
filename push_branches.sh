#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
set -euo pipefail
GITUSER=$USER
[ "$GITUSER" == "user" ] && GITUSER=pbouvier

branches=$(ls .git/refs/heads/$GITUSER/ |
           grep -v '^series$' |
           grep -v '^pr$' |
           sed -e "s#^#$GITUSER/#")
echo "qualcomm/qemu:"
echo "$branches"
git push -f origin $branches
echo "---------------------------------"
echo "qemu-ci: $GITUSER/ci -> ci"
git push -f qemu-ci $GITUSER/ci:ci
echo "qemu-ci: $GITUSER/gitlab_ci_full -> gitlab_ci_full"
git push -f qemu-ci $GITUSER/gitlab_ci_full:gitlab_ci_full
echo "---------------------------------"
