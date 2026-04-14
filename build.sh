#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later

set -euo pipefail

type=
if [ $# -ge 1 ]; then
    type=$1; shift
fi

QEMU_CONFIGURE_OPTS=${QEMU_CONFIGURE_OPTS:-}
cross_prefix=$(echo "$QEMU_CONFIGURE_OPTS" | sed -e 's/--cross-prefix=//')
build_arch=
if [ "$cross_prefix" != "" ]; then
    build_arch=" [$cross_prefix]"
fi

mkdir -p build
touch build/.build_type
previous_type="$(cat build/.build_type)"

if [ "$type" == "" ]; then
    type=$(echo $previous_type | cut -f 1 -d ' ')
fi
if [ "$type" == "" ]; then
    type="debug"
fi

if [ "$previous_type" != "$type$build_arch" ]; then
    echo "build type is different ($previous_type -> $type$build_arch)"
    rm -rf build || true
    mkdir -p build
    echo "$type$build_arch" > build/.build_type
fi

echo "build: $type$build_arch"

if [ $type == tsan ]; then
    echo "Build with TSAN: export LD_LIBRARY_PATH=$(pwd)/build/glib_tsan"
fi

target="--target-list=aarch64-linux-user,x86_64-linux-user"
target=$target,aarch64-softmmu,x86_64-softmmu,arm-softmmu

pushd build > /dev/null
if [ ! -f .configured ]; then
    export CC="ccache ${cross_prefix}gcc"
    export CXX="ccache ${cross_prefix}g++"
    configure_flags=
    case $type in
    dev)
        configure_flags="--enable-debug-tcg --enable-debug-graph-lock \
                         --enable-debug-mutex --enable-asan --enable-ubsan"
        export CFLAGS="-fno-omit-frame-pointer"
        ;;
    debug)
        configure_flags="--enable-debug"
        export CFLAGS="-fno-omit-frame-pointer"
        ;;
    opt)
        configure_flags=""
        export CFLAGS="-fno-omit-frame-pointer"
        ;;
    uftrace)
        configure_flags="--disable-werror"
        export CFLAGS="-fno-omit-frame-pointer -pg -fno-inline"
        # -pg is faster than -finstrument-functions (x2)
        # we need to disable inlining or we skip inlined func.
        ;;
    tsan)
        if [ ! -d glib_tsan ]; then
            rm -rf glib
            git clone --depth=1 --branch=2.87.0 \
                https://github.com/GNOME/glib.git
            pushd glib
            CFLAGS="-O2 -g -fsanitize=thread" \
                meson setup \
                --prefix=$(pwd)/out \
                build -Dtests=false
            ninja -C build install
            popd
            mkdir -p glib_tsan
            rsync -av glib/out/lib/*/* glib_tsan/
            rm -rf glib/
        fi
        configure_flags="--enable-tsan"
        ;;
    clang)
        export CC="ccache clang"
        export CXX="ccache clang++"
        configure_flags="--enable-debug"
        ;;
    single-binary)
        configure_flags="--enable-debug"
        export CFLAGS="-fno-omit-frame-pointer"
        target=--target-list=arm-softmmu,aarch64-softmmu
        ;;
    all-debug)
        configure_flags="--enable-debug"
        export CFLAGS="-fno-omit-frame-pointer"
        target= # build all targets
        ;;
    all-opt)
        configure_flags=""
        export CFLAGS="-fno-omit-frame-pointer"
        target= # build all targets
        ;;
    all-dev)
        configure_flags="--enable-debug-tcg --enable-debug-graph-lock \
                         --enable-debug-mutex --enable-asan --enable-ubsan"
        export CFLAGS="-fno-omit-frame-pointer"
        target= # build all targets
        ;;
    coverage)
        export CFLAGS="--coverage"
        ;;
    docs)
        configure_flags="--enable-docs"
        target="--target-list=" # no target
        ;;
    *)
        choices="dev, debug, opt, uftrace, tsan, clang, docs, single-binary"
        choices="$choices, coverage, all-debug, all-opt, all-dev"
        echo "Unknown build type $type (choices: $choices)"
        exit 1
        ;;
    esac
    ../configure $target --disable-docs $configure_flags --cross-prefix=$cross_prefix
    touch .configured
fi
ninja "$@" | sed -e "s#^\.\./#$(readlink -f ../)/#"
popd > /dev/null
