#!/usr/bin/env sh

set -ex

export DEBIAN_FRONTEND=noninteractive

apt-get --assume-yes update
apt-get --assume-yes upgrade
# shellcheck disable=2086
apt-get --assume-yes install \
    bison ca-certificates cmake curl file flex git jq libglib2.0-dev mingw-w64 \
    mingw-w64-tools ninja-build python3 python3-venv unzip wget zstd

readonly PACKAGE_DB="${1}"
if [ ! -f "${PACKAGE_DB}" ]; then
    echo "Error: Package database not found: ${PACKAGE_DB}"
    exit 1
fi

# Extract mirror, prefix, and package list from JSON
MIRROR="$(jq -r '.["windows-x86_64"].mirror' "${PACKAGE_DB}")"
readonly MIRROR
PREFIX="$(jq -r '.["windows-x86_64"].prefix' "${PACKAGE_DB}")"
readonly PREFIX

# Build package URLs from JSON package database
MSYS2_PACKAGE_URLS="$(
    jq -r '.["windows-x86_64"].packages[] |
    "'"${MIRROR}"'/'"${PREFIX}"'/" + .package' "${PACKAGE_DB}"
)"
readonly MSYS2_PACKAGE_URLS

for url in ${MSYS2_PACKAGE_URLS}; do
    ARCHIVE="$(basename "${url}")"
    wget "${url}"
    tar  --directory="/" --extract --file "${ARCHIVE}" --verbose
    rm --force "${ARCHIVE}"
done
