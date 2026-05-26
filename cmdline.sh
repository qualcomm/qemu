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

f()
{
    filter_version | filter_usage
}

./build.sh single-binary all qemu-system

ref=cmdline_ref
rm -rf $ref
mkdir -p $ref
for t in $(targets); do
    bin=qemu-system-$t
    ./build/$bin -help |& f         > $ref/$bin.help
    ./build/$bin -cpu help |& f     > $ref/$bin.cpu_help
    ./build/$bin -machine help |& f > $ref/$bin.machine_help
    ./build/$bin -device help |& f  > $ref/$bin.device_help
done

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
