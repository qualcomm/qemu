#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later

set -euo pipefail

echo "RESET ALL BRANCHES to remote ones - press any key when ready"
read -n 1

for branch in $(git branch -r | grep origin/ | grep -v /master | grep -v '\->'); do
    git branch -f --track "${branch#origin/}" "$branch";
done
git checkout pbouvier/master
git reset --hard origin/pbouvier/master
