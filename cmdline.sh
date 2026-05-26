#!/usr/bin/env bash

set -euo pipefail

targets()
{
    ./build/qemu-system -target help |& grep '^-' | sed -e 's/-//g'
}

filter_version()
{
    grep -v 'QEMU emulator version'
}

filter_usage()
{
    grep -v '^usage:'
}

filter_trailing_whitespace()
{
    sed -e 's/\s+$//'
}

f()
{
    filter_version | filter_usage | filter_trailing_whitespace
}

./build.sh single-binary all qemu-system

ref=cmdline_ref
err=0
for t in $(targets); do
    bin=qemu-system-$t
    echo ------------------------------------------------------------------
    git diff --color-words --no-index \
        $ref/$bin.help \
        <(./build/qemu-system -target $t -help |& f) || err=1
    git diff --color-words --no-index \
        $ref/$bin.cpu_help \
        <(./build/qemu-system -target $t -cpu help |& f) || err=1
    git diff --color-words --no-index \
        $ref/$bin.machine_help \
        <(./build/qemu-system -target $t -machine help |& f) || err=1
    git diff --color-words --no-index \
        $ref/$bin.device_help \
        <(./build/qemu-system -target $t -device help |& f) || err=1
done

exit $err
