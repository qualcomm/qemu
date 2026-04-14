#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later

./show_duplicated_compilation_units.sh  |
    awk '{print $2}' | xargs -n1 dirname | sed -e 's#^\.\./##' |
    sort | uniq -c | sort -rn
