#!/usr/bin/env sh
# SPDX-License-Identifier: GPL-2.0-or-later

set -eu

BAREMETAL_VER="${1:-21.0.03}"
readonly BAREMETAL_VER
MUSL_VER="${2:-22.1.4}"
readonly MUSL_VER

readonly BAREMETAL_SRC="/pkg/qct/software/hexagon/releases/tools/${BAREMETAL_VER}"
readonly MUSL_URL="https://artifacts.codelinaro.org/artifactory/codelinaro-toolchain-for-hexagon/${MUSL_VER}_/clang+llvm-${MUSL_VER}-cross-hexagon-unknown-linux-musl.tar.zst"

if [ ! -d "${BAREMETAL_SRC}" ]; then
    printf "Error: %s not found.\n" "${BAREMETAL_SRC}" >&2
    printf "Run from a host with /pkg/qct/software/hexagon mounted.\n" >&2
    exit 1
fi
printf "Copying baremetal toolchain %s...\n" "${BAREMETAL_VER}"
mkdir -p "./${BAREMETAL_VER}"
cp -a "${BAREMETAL_SRC}/Tools" "./${BAREMETAL_VER}/Tools"

printf "Downloading musl toolchain %s...\n" "${MUSL_VER}"
curl -fSL "${MUSL_URL}" | tar -I zstd -xf -

printf "Done.\n"
printf "  Baremetal: ./%s/Tools\n" "${BAREMETAL_VER}"
printf "  Musl:      ./clang+llvm-%s-cross-hexagon-unknown-linux-musl/x86_64-linux-gnu\n" "${MUSL_VER}"
