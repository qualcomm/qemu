#!/bin/sh

set -eu

dir="$1"
pkgversion="$2"
version="$3"

if [ -z "$pkgversion" ]; then
    cd "$dir"
    if [ -e .git ]; then
        pkgversion=$(git describe --match 'v*' --dirty) || :
    fi
fi

if [ -n "$pkgversion" ]; then
    fullversion="$version ($pkgversion)"
else
    fullversion="$version"
fi

sha="$(git rev-parse HEAD)"
if [ -n "$(git status --porcelain)" ]; then
    sha="$sha-dirty"
fi

hexagon_tag="$(git describe --tags --exact-match --match 'qemu-hexagon-*' HEAD \
               2>/dev/null | sed -e 's/^qemu-hexagon-//')" || hexagon_tag=""

cat <<EOF
#define QEMU_PKGVERSION "$pkgversion"
#define QEMU_FULL_VERSION "$fullversion"
#define QEMU_HEXAGON_SHA "$sha"
#define QEMU_HEXAGON_TAG "$hexagon_tag"
EOF
