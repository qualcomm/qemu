#!/bin/bash

#  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
#  SPDX-License-Identifier: BSD-3-Clause

set -euo pipefail

usage() {
    echo "Usage: $0 <dest-toolchain-root> <src-linux-toolchain>"
    echo ""
    echo "Augments a baremetal-only Hexagon toolchain with Linux cross-compilation"
    echo "support by copying the sysroot, QEMU, symlinks, and driver config from"
    echo "a reference hexagon-unknown-linux-musl cross-toolchain."
    echo ""
    echo "  dest-toolchain-root   Path to the baremetal toolchain (e.g. .../Tools)"
    echo "  src-linux-toolchain   Path to the reference Linux cross-toolchain host dir"
    echo "                        (e.g. .../clang+llvm-22.1.0-cross-"
    echo "                        hexagon-unknown-linux-musl/x86_64-linux-gnu)"
    exit 1
}

[ $# -eq 2 ] || usage

DEST="$(cd "$1" && pwd)"
SRC="$(cd "$2" && pwd)"

BIN="${DEST}/bin"
TARGET="${DEST}/target"
SYSROOT="${TARGET}/hexagon-unknown-linux-musl"

# Validate inputs
[ -x "${BIN}/clang" ] || { echo "Error: ${BIN}/clang not found"; exit 1; }
[ -d "${SRC}/bin" ]   || { echo "Error: ${SRC}/bin not found"; exit 1; }
[ -d "${SRC}/target/hexagon-unknown-linux-musl" ] || {
    echo "Error: source sysroot not found"
    exit 1
}

# Resolve the real clang binary name for symlinks
CLANG_NAME="$(basename "$(readlink -f "${BIN}/clang")")"

echo "=== Adding hexagon-unknown-linux-musl support ==="
echo "  dest: ${DEST}"
echo "  src:  ${SRC}"
echo ""

# --- 1. Copy Linux/musl sysroot ---
echo "Copying Linux/musl sysroot..."
if [ -d "${SYSROOT}" ]; then
    echo "  WARNING: ${SYSROOT} already exists, skipping"
else
    cp -a "${SRC}/target/hexagon-unknown-linux-musl" "${TARGET}/"
fi
# Convenience symlink (skip if target/hexagon-linux-musl already exists)
ln -sfn hexagon-unknown-linux-musl "${TARGET}/hexagon-linux-musl" 2>/dev/null || true

# --- 1b. Install compiler-rt builtins into the clang resource dir ---
# Clang's --rtlib=compiler-rt (set in the .cfg in step 5) looks for
# libclang_rt.builtins.a in the clang resource directory at
# lib/clang/<ver>/lib/hexagon-unknown-linux-musl/. The 23.x baremetal
# toolchain ships an empty resource dir, so we have to populate it
# ourselves from the reference linux-musl toolchain. We also replace
# the sysroot copy with a symlink to the canonical resource-dir file
# so both lookup paths resolve identically.
echo "Installing compiler-rt builtins into clang resource dir..."

# Discover dest clang version dir. The lib/clang/ tree may contain
# non-version siblings (e.g. a 'python/' dir in the 23.x toolchain),
# so filter to numeric-only basenames.
dest_clang_versions=()
for d in "${DEST}"/lib/clang/*/; do
    name="$(basename "$d")"
    case "$name" in
        ''|*[!0-9]*) ;;  # skip non-numeric
        *) dest_clang_versions+=("${d%/}") ;;
    esac
done
if [ ${#dest_clang_versions[@]} -ne 1 ]; then
    echo "Error: expected exactly one numeric version dir under" \
        "${DEST}/lib/clang/, found: ${dest_clang_versions[*]:-<none>}"
    exit 1
fi
DEST_CLANG_VER_DIR="${dest_clang_versions[0]}"
DEST_CLANG_VER="$(basename "${DEST_CLANG_VER_DIR}")"

# Discover src clang version dir, same filter.
src_clang_versions=()
for d in "${SRC}"/lib/clang/*/; do
    name="$(basename "$d")"
    case "$name" in
        ''|*[!0-9]*) ;;
        *) src_clang_versions+=("${d%/}") ;;
    esac
done
if [ ${#src_clang_versions[@]} -ne 1 ]; then
    echo "Error: expected exactly one numeric version dir under" \
        "${SRC}/lib/clang/, found: ${src_clang_versions[*]:-<none>}"
    exit 1
fi
SRC_CLANG_VER_DIR="${src_clang_versions[0]}"

SRC_RTLIB_DIR="${SRC_CLANG_VER_DIR}/lib/hexagon-unknown-linux-musl"
DEST_RTLIB_DIR="${DEST_CLANG_VER_DIR}/lib/hexagon-unknown-linux-musl"

# Validate source files exist
for f in libclang_rt.builtins.a clang_rt.crtbegin.o clang_rt.crtend.o; do
    if [ ! -f "${SRC_RTLIB_DIR}/${f}" ]; then
        echo "Error: missing ${SRC_RTLIB_DIR}/${f}"
        exit 1
    fi
done

mkdir -p "${DEST_RTLIB_DIR}"
for f in libclang_rt.builtins.a clang_rt.crtbegin.o clang_rt.crtend.o; do
    if [ -e "${DEST_RTLIB_DIR}/${f}" ] &&
            [ ! -L "${DEST_RTLIB_DIR}/${f}" ]; then
        echo "  ${f} already present, skipping"
    else
        cp -a "${SRC_RTLIB_DIR}/${f}" "${DEST_RTLIB_DIR}/${f}"
        echo "  installed ${f}"
    fi
done

# Replace the sysroot copy (or stale symlink) with a symlink into the
# resource dir.  Use a relative target so the toolchain stays
# relocatable.
sysroot_lib_link="${SYSROOT}/usr/lib/libclang_rt.builtins-hexagon.a"
sysroot_lib_target="../../../../lib/clang/${DEST_CLANG_VER}"
sysroot_lib_target+="/lib/hexagon-unknown-linux-musl/libclang_rt.builtins.a"
ln -sfn "${sysroot_lib_target}" "${sysroot_lib_link}"
echo "  linked sysroot ${sysroot_lib_link##*/} -> ${sysroot_lib_target}"

# --- 2. Copy ld.lld from the reference toolchain ---
# The SDK driver hardcodes "hexagon-linux-link" as the linker, and ld.eld
# produces 4K-aligned ELF segments.  QEMU Hexagon requires 64K (0x10000)
# alignment, so we use ld.lld instead.
echo "Copying ld.lld..."
if [ -x "${SRC}/bin/ld.lld" ]; then
    cp -aL "${SRC}/bin/ld.lld" "${BIN}/"
elif [ -x "${SRC}/bin/lld" ]; then
    cp -aL "${SRC}/bin/lld" "${BIN}/ld.lld"
fi

# Create hexagon-linux-link wrapper.  The SDK driver invokes the linker as
# "hexagon-linux-link" which lld doesn't recognise via argv[0] (it needs
# to be called as "ld.lld" for the ELF flavour).  This wrapper also strips
# eld-specific flags that lld doesn't understand and fixes --hash-style.
echo "Creating hexagon-linux-link wrapper..."
cat > "${BIN}/hexagon-linux-link" << 'LINK_WRAPPER'
#!/bin/sh
# Translate eld-specific linker flags for ld.lld
args=""
skip_next=0
for arg in "$@"; do
    if [ "$skip_next" = 1 ]; then
        skip_next=0
        continue
    fi
    case "$arg" in
        -march=hexagon) ;;          # eld-specific, lld uses -m hexagonelf
        -mcpu=hexagon*) ;;          # eld-specific
        -G[0-9]*) ;;               # small-data threshold, eld-specific
        -G) skip_next=1 ;;         # -G <n> form
        --hash-style=*) ;;         # override below; musl needs .gnu.hash
        *) args="$args $arg" ;;
    esac
done
exec "$(dirname "$0")/ld.lld" --hash-style=both $args
LINK_WRAPPER
chmod +x "${BIN}/hexagon-linux-link"

# --- 3. Copy QEMU ---
echo "Copying QEMU..."
for qemu in qemu-hexagon qemu-system-hexagon; do
    if [ -x "${SRC}/bin/${qemu}" ]; then
        cp -a "${SRC}/bin/${qemu}" "${BIN}/"
        echo "  ${qemu}"
    fi
done

# --- 4. Create target-triple symlinks ---
echo "Creating hexagon-unknown-linux-musl-* symlinks..."
for tool in clang clang++; do
    ln -sf "${CLANG_NAME}" "${BIN}/hexagon-unknown-linux-musl-${tool}"
done

ln -sf ld.lld "${BIN}/hexagon-unknown-linux-musl-ld.lld"

# Map tool names to their underlying binaries
declare -A TOOL_MAP=(
    [ar]=llvm-ar
    [nm]=llvm-nm
    [objcopy]=llvm-objcopy
    [objdump]=llvm-objdump
    [ranlib]=llvm-ar
    [readelf]=llvm-readobj
    [size]=llvm-size
    [strip]=llvm-objcopy
)

for tool in "${!TOOL_MAP[@]}"; do
    target="${TOOL_MAP[$tool]}"
    # Try the exact name first, then fall back to hexagon-prefixed names
    if [ -e "${BIN}/${target}" ] || [ -L "${BIN}/${target}" ]; then
        ln -sf "${target}" "${BIN}/hexagon-unknown-linux-musl-${tool}"
    elif [ -e "${BIN}/hexagon-${target}" ] || [ -L "${BIN}/hexagon-${target}" ]; then
        ln -sf "hexagon-${target}" "${BIN}/hexagon-unknown-linux-musl-${tool}"
    fi
done

# Also create short-form aliases (hexagon-linux-musl-*)
echo "Creating hexagon-linux-musl-* alias symlinks..."
for f in "${BIN}"/hexagon-unknown-linux-musl-*; do
    short="hexagon-linux-musl-$(basename "$f" | sed 's/^hexagon-unknown-linux-musl-//')"
    ln -sf "$(basename "$f")" "${BIN}/${short}"
done

# --- 5. Create driver config file ---
# The SDK Hexagon driver hardcodes target/hexagon/ as the sysroot
# regardless of target triple.  This cfg file overrides that for
# hexagon-unknown-linux-musl so that clang finds musl headers/libs.
echo "Creating hexagon-unknown-linux-musl.cfg..."
cat > "${BIN}/hexagon-unknown-linux-musl.cfg" << 'CFG'
# Clang driver configuration for hexagon-unknown-linux-musl
#
# This file is auto-discovered when clang is invoked as
# hexagon-unknown-linux-musl-clang (or with --target=hexagon-unknown-linux-musl
# and a matching --config).

# Point the sysroot at the musl-based Linux sysroot
--sysroot=<CFGDIR>/../target/hexagon-unknown-linux-musl

# Use the musl C and libc++ headers
-isystem <CFGDIR>/../target/hexagon-unknown-linux-musl/usr/include/c++/v1
-isystem <CFGDIR>/../target/hexagon-unknown-linux-musl/usr/include

# Use lld as the linker (lld produces 64K-aligned ELF segments required by QEMU)
--ld-path=<CFGDIR>/ld.lld

# Use compiler-rt and libunwind (the SDK driver defaults to --rtlib=libgcc
# which pulls in -lc_eh, a library that doesn't exist in the musl sysroot)
--rtlib=compiler-rt
--unwindlib=libunwind
CFG

echo ""
echo "=== Done ==="
echo ""
echo "Test with:"
echo "  ${BIN}/hexagon-unknown-linux-musl-clang -o hello hello.c -static"
echo "  ${BIN}/qemu_wrapper.sh ./hello"
