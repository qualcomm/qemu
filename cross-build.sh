#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later

set -euo pipefail

if [ $# -lt 1 ]; then
    echo "usage: arch [build.sh args]..."
    exit 1
fi

arch=$1; shift
dockerfile=./tests/docker/dockerfiles/debian-$arch-cross.docker
if [ ! -f $dockerfile ]; then
    echo "$arch not available:"
    ls tests/docker/dockerfiles/debian*cross*.docker |
        xargs -n1 basename |
        sed -e 's/debian-//' -e 's/-cross.docker//' |
        grep -v all-test |
        sort |
        sed -e 's/^/- /'
    exit 1
fi

./container.sh debian-$arch-cross ./build.sh "$@"
