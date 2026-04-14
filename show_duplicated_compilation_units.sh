#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later

set -euo pipefail

jq --raw-output < build/compile_commands.json '.[].command' |
    grep -v 'CONFIG_USER_ONLY'  |
    grep -v tests/ |
    grep -v qemu-keymap |
    grep -v linux-user |
    grep -v qemu-pr-helper |
    grep -v qemu-ga |
    grep -v hw/display/edid-generate.c |
    grep -v system/main.c |
    sed -e 's/.* //' -e 's/",//' |
    sort | uniq -c | sort -rn | grep -v '^\s*1 ' | less
