#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
set -euo pipefail
branches=$(ls .git/refs/heads/$USER/ |
           grep -v '^series$' |
           grep -v '^pr$' |
           sed -e "s#^#$USER/#")
echo "quic/qemu:"
echo "$branches"
git push -f origin $branches
echo "---------------------------------"
echo "qemu-ci: $USER/ci -> ci"
git push -f qemu-ci $USER/ci:ci
echo "qemu-ci: $USER/gitlab_ci_full -> gitlab_ci_full"
git push -f qemu-ci $USER/gitlab_ci_full:gitlab_ci_full
echo "---------------------------------"
