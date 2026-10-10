#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later

set -euo pipefail

script_dir=$(dirname $(readlink -f $0))

if [ $# -ne 1 ]; then
    echo "usage: out_folder"
    exit 1
fi

out=$1;shift

releases()
{
    git tag --sort=creatordate |
        grep '^v[0-9]' |
        grep -A 10000 'v1\.6\.1' |
        grep -v '\-rc[0-9]$'
}

qemu_targets()
{
    version=$1;shift
    git ls-tree -r $version --name-only |
        grep configs.*mak |
        sed -e 's#.*/##' -e 's#\.mak$##'
}

softmmu_binaries()
{
    qemu_targets "$@" |
        grep softmmu |
        sed -e 's/-softmmu//' -e 's/^/qemu-system-/'
}

linux_user_binaries()
{
    qemu_targets "$@" |
        grep linux-user |
        sed -e 's/-linux-user//' -e 's/^/qemu-/'
}

root=$(readlink -f $script_dir/..)
docker=$($root/tests/docker/docker.py probe)

registry=docker.io/pboqemu/qemu-releases
for release in $(releases); do
    echo "generate links for $release"
    public_image=$registry:$release
    mkdir -p $out/$release
    for bin in $(softmmu_binaries $release) $(linux_user_binaries $release); do
        script=$out/$release/$bin
        cat > $script << EOF
#!/usr/bin/env bash

here=\$(pwd)
exec $docker run -u $UID \\
     --security-opt label=disable \\
     -w \$(pwd) -v \$(pwd):\$(pwd) \\
     $public_image $bin "\$@"
EOF
        chmod +x $script
    done
done
