#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later

set -euo pipefail

if [ $# -lt 1 ]; then
    echo "usage: test_cmd [args...]"
    exit 1
fi

tmp=$(mktemp -d)
trap "rm -rf $tmp" EXIT

dump_log()
{
    cat $tmp/stdout
    cat $tmp/stderr 1>&2
}

trap "dump_log; exit 1" SIGTERM

run()
{
    err=0
    "$@" 1> $tmp/stdout 2> $tmp/stderr || err=$?
    return $err
}

exit_code=$?
# retry 2 times
run "$@" || run "$@" || exit_code=$?

dump_log
exit $exit_code
