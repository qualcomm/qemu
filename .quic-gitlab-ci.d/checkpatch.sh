#!/usr/bin/env bash

error=0

for rev in $(git rev-list --first-parent $1..$2)
do
    git rev-parse --verify -q $rev^2 >/dev/null && continue
    echo "========= Checking commit $(git log --pretty=reference $rev^!)"
    ./scripts/checkpatch.pl --color=always --no-signoff --branch "$rev^!"
    error=$(($error || $?))
done

if test $? -ne 0 || test $error -ne 0
then
    exit 1
fi
