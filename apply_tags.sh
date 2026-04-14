#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later

set -euo pipefail

if [ $# -ne 1 ]; then
    echo "usage: msg-id"
    exit 1
fi

m=$1
rm -f .git/filter-repo/already_ran
b4 mbox $m --mbox-name $m.mbox
trap "rm $m.mbox; git rebase --abort" EXIT
b4="b4 --no-interactive trailers --update --sloppy-trailers"
git rebase upstream/master --signoff --committer-date-is-author-date --exec \
    "$b4 --since-commit HEAD -m $m.mbox" ||
    git rebase --abort
rm $m.mbox
trap - EXIT
