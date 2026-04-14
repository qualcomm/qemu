#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later

set -euo pipefail

semcode-index
semcode-index --lore qemu-devel/0
semcode-index --lore qemu-devel/1
semcode-index --lore qemu-devel/2
semcode-index --lore qemu-devel/3
semcode-index --lore qemu-devel
