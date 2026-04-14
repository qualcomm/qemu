#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later

set -euo pipefail

test=$(basename "$*" .py)
test_log=$(mktemp -d)

test_dir()
{
    find tests/functional -name "$test"'.*' || true
}

show_logs()
{
    for log in $(test_dir)/*.log; do
        cat << EOF
-----------------------------------------------------
$(echo $log)
$(tail -n 200 $log)
EOF
    done
    echo
}

trap "rm -rf $test_log" EXIT

err=0
delay=10m
timeout $delay "$@" > $test_log/stdout 2> $test_log/stderr || err=$?

if [ $err -eq 0 ]; then
    # success, show results
    cat $test_log/stdout
    exit 0
fi

if [[ $err -eq 124 || $err -eq 137 ]]; then
    echo "not ok TIMEOUT" >> $test_log/stdout
else
    echo "FAIL: exit $err" >> $test_log/stderr
fi

show_logs >&2
cat $test_log/stderr >&2
cat $test_log/stdout >&2

cat $test_log/stdout
exit $err
