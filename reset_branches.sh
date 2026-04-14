#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later

set -euo pipefail

echo "READY TO RESET ALL BRANCHES - wait 5 sec"
sleep 5

for branch in $(git branch -r | grep origin/ | grep -v /master | grep -v '\->'); do
    git branch -f --track "${branch#origin/}" "$branch";
done
git checkout pbouvier/master
git reset --hard origin/pbouvier/master
