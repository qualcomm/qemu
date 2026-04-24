#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later

set -euo pipefail

test=$(basename "$*" .py)
test_log=$(mktemp -d)

show_output()
{
    test_dir=$(find tests/functional -name "$test"'.*')
    for log in "$test_dir"/*.log; do
        cat << EOF
-----------------------------------------------------
$(echo $log)
$(tail -n 200 $log)
EOF
    done
    echo
}

show_log()
{
    cat $test_log/stdout
    cat $test_log/stderr >&2
}

trap "rm -rf $test_log" EXIT
trap show_output SIGTERM

delay=30m
num_tries=3

err=0
for try in $(seq 1 $num_tries); do
    timeout $delay "$@" > $test_log/stdout 2> $test_log/stderr || err=$?
    if [ $err -eq 0 ]; then
        # success
        show_log
        exit 0
    fi
    if [ $err -eq 124 ]; then
        # timeout
        echo "not ok TIMEOUT" > $test_log/stdout
        continue
    fi
    if [ $err -eq 137 ]; then
        # timeout
        echo "not ok TIMEOUT" > $test_log/stdout
        continue
    fi
done
show_log
show_output >&2
exit $err
