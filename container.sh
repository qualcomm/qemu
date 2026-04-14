#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later

set -euo pipefail

die()
{
    echo "$@" >&2
    exit 1
}

[ $# -ge 2 ] || die "usage: container command [args...]"
container=$1;shift

script_dir="$(dirname $(readlink -f $0))"
pushd "$script_dir"

dockerfile=tests/docker/dockerfiles/$container.docker
context_hash=$(sha1sum $dockerfile | cut -f 1 -d ' ')
image=qemu-$container:$context_hash
if ! podman image exists $image; then
    podman build -t $image -f $dockerfile
fi
podman tag $image $container

mkdir -p $HOME/.cache/ccache
# run privileged container: kvm + all capability (ptrace needed for Lsan)
podman run -it \
    --pull newer \
    --privileged \
    -e CCACHE_DIR=$HOME/.cache/ccache \
    -v $HOME/.cache/ccache/:$HOME/.cache/ccache/ \
    -v $(pwd):$(pwd) -w $(pwd)\
    -v /:/host \
    $image "$@"
