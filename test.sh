#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later

set -euo pipefail

env ASAN_OPTIONS=detect_leaks=0 \
./build/pyvenv/bin/meson test -C build --print-errorlogs \
--setup thorough \
--timeout-multiplier 0 \
--max-lines=0 \
"$@"
