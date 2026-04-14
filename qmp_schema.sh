#!/usr/bin/env bash

set -euo pipefail

if [ $# -lt 1 ]; then
    echo 'usage: qemu_cmd' 1>&2
    exit 1
fi

qemu_cmd=$*

$qemu_cmd \
-display none \
-qmp unix:qmp-socket,server \
&

qemu_pid=$!

sleep 1

qmp_session()
{
    cat << EOF
    { "execute": "qmp_capabilities" }
    { "execute": "query-qmp-schema" }
    { "execute": "query-cpu-model-expansion",
      "arguments": { "type": "full", "model": { "name": "max" } } }
    { "execute": "rtc-reset-reinjection" }
EOF
}

qmp_session | socat - unix-connect:qmp-socket | jq
kill $qemu_pid
