#!/usr/bin/env bash

# meson test -C build --setup thorough \
# --suite func-quick --suite func-thorough \
# func-aarch64-aarch64_rme_virt --verbose
#
# yes ./try_rme.sh /path/to/rme_stack/out | head -n 1000 | parallel --bar -j$(nproc)

# set -x
set -euo pipefail

if [ $# -ne 1 ]; then
    echo "usage: rme_stack_data_dir"
    exit 1
fi

data=$1; shift
tmp=$(mktemp -d)
qemu=$(pwd)/build/qemu-system-aarch64
$(pwd)/run_rme.sh $data $qemu \
    -d int -D $tmp/int.log \
    -icount shift=auto,rr=record,rrfile=$tmp/replay.log >& $tmp/run.log &
job=$!

kill_job()
{
    kill $job
    while kill -0 $job >& /dev/null; do
        sleep 1
    done
}

while true; do
    sleep 1

    if ! kill -0 $job >& /dev/null; then
        echo "QEMU failed: $tmp"
        exit 1
    fi

    if grep -i sync $tmp/run.log; then
        echo "reproduce with:" 1>&2
        echo "./run_rme.sh $data $qemu" \
             "-icount shift=auto,rr=replay,rrfile=$tmp/replay.log" 1>&2
        kill_job
        exit 1
    fi

    if grep -qi 'Linux version' $tmp/run.log; then
        kill_job
        rm -rf $tmp
        exit 0
    fi
done
