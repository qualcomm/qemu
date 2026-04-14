# SPDX-License-Identifier: GPL-2.0-or-later
git fetch  --all -a -p
if [ -d .cache/lore/qemu-devel ]; then
    ./lore/scripts/lore-update
fi
