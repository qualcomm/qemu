#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later

set -euo pipefail

if [ $# -lt 1 ]; then
    echo "usage: container [build.sh args]..."
    exit 1
fi

container=$1; shift
dockerfile=./tests/docker/dockerfiles/$container.docker
if [ ! -f $dockerfile ]; then
    echo " not available:"
    ls tests/docker/dockerfiles/*.docker |
        xargs -n1 -I file basename file .docker |
        sort |
        sed -e 's/^/- /'
    exit 1
fi

./container.sh $container ./build.sh "$@"
