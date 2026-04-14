#!/usr/bin/env bash

set -euo pipefail

ninja -C build qemu-system --quiet |
sed -e '/: in function/d' -e 's/.*multiple definition of .//g' -e 's/.;.*//' |
sort -u
