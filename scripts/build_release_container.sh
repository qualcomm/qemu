#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later

set -euo pipefail

script_dir=$(dirname $(readlink -f $0))

# We use ubuntu containers, as repo urls are correctly updated for old
# distros. Oldest available is 14.04 (in 2026-10). The oldest buildable release is
# v1.6.0. We want to skip 1.5.3, so let's start from 1.6.1.
oldest_release=v1.6.1

if [ $# -ne 1 ]; then
    echo "usage: tag (>= $oldest_release)"
    exit 1
fi
version=$1;shift

tags()
{
    git tag --sort=creatordate | grep '^v[0-9]' | grep -A 10000 $oldest_release
}

if ! tags | grep -q "^$version$"; then
    echo "version $version is not available, check git tag (>= $oldest_release)"
    exit 1
fi

year=$(git log -1 --format='%cd' --date='format:%Y' $version)
month=$(git log -1 --format='%cd' --date='format:%m' $version)

# Ubuntu LTS are published every (even) two years in April.
ubuntu=
if [ $((year % 2)) -eq 1 ]; then
    # pick latest LTS from previous year
    ubuntu=$((year - 1))
elif [ $month -gt 6 ]; then
    # LTS published this year should work
    ubuntu=$year
else
    # pick latest LTS from two years ago
    ubuntu=$((year - 2))
fi
if [ $ubuntu -lt 2014 ]; then
    # oldest ubuntu release available with containers is 14.04
    ubuntu=2014
fi
ubuntu=$((ubuntu - 2000))
ubuntu_version=$ubuntu.04

dockerfile=$script_dir/../tests/docker/dockerfiles/release.docker
docker=$script_dir/../tests/docker/docker.py

set -x
$docker build \
    -f $dockerfile \
    --build-arg QEMU_VERSION=$version \
    --build-arg UBUNTU_VERSION=$ubuntu_version \
    -t qemu/$version
