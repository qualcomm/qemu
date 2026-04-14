#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later

if [ $# -lt 1 ]; then
    echo "usage: cmd args..." 1>&2
    exit 1
fi

gdbserver :12345 "$@" &
pid=$!
code --wait
kill $pid
wait $pid || true
