#!/usr/bin/env sh

# Copyright(c) 2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
# SPDX-License-Identifier: GPL-2.0-or-later

SOURCE_DIR="$(pwd)"
BUILD_DIR="${SOURCE_DIR}/build"
INSTALL_DIR="${BUILD_DIR}/install"

print_help()
{
    echo
    echo "Usage: $(basename "${0}") [OPTIONS]" CONFIGURATION
    echo
    echo "Configurations:"
    echo "    hexagon,linux-x86_64"
    echo "    hexagon,linux-x86_64,san"
    echo "    hexagon,linux-x86_64,no-idef-parser"
    echo "    hexagon,windows-x86_64"
    echo "    hexagon,windows-aarch64"
    echo "    hexagon,mac"
    echo "    aarch64,linux-x86_64"
    echo
    echo "Options:"
    echo "    -b    name of the build directory"
    echo "          (default: ${BUILD_DIR})"
    echo "    -i    name of the install directory"
    echo "          (default: ${INSTALL_DIR})"
    echo "    -s    name of the source directory"
    echo "          (default: ${SOURCE_DIR})"
    echo "    -h    print this help"
    echo
    echo "When specifying -b it's likely that -s also needs to be specified."
    echo "This script assumes the build directory is located inside the source"
    echo "directoy and is one level deep (see defaults). If another layout is"
    echo "preferred, the source directory needs to be specified for"
    echo "configuration to properly work. For example:"
    echo
    echo "    Alternate layout:"
    echo "       Source directory: /foo/bar/qemu"
    echo "       Build directory:  /foo/bar/qemu/build/debug"
    echo
    echo "    Another alternate layout:"
    echo "       Source directory: /foo/bar/qemu"
    echo "       Build directory:  /foo/qemu-build"
    echo
    echo "Description:"
    echo "    The values for the configuration argument shown above determine"
    echo "    how Qemu will be configured. Each argument is a comma-separated"
    echo "    list that specify:"
    echo "        1. The guest architecture Qemu will be emulating"
    echo "        2. The host system Qemu will be running on"
    echo "        3. Special build types"
    echo
    echo "    The second argument (the host system) consists of two parts that"
    echo "    are separated by a dash:"
    echo "        1. The operating system"
    echo "        2. The hardware architecture"
    echo
    echo "    Example - hexagon,linux-x86_64,san"
    echo "        This configuration will build Qemu Hexagon for Linux running"
    echo "        on x86_64 hardware. It will also enable various sanitziers"
}

readonly OPTIONS="hb:i:s:"
while getopts "${OPTIONS}" option; do
    case "${option}" in
        "b") readonly BUILD_DIR="${OPTARG}";;
        "i") readonly INSTALL_DIR="${OPTARG}";;
        "s") readonly SOURCE_DIR="${OPTARG}";;
        "h") print_help; exit 0;;
        "*") print_help; exit 1;;
    esac
done

shift $((OPTIND-1))

readonly CONFIGURATION="${1}"

if [ -z "${CONFIGURATION}" ]; then
    echo "Error: Missing positional argument"
    print_help
    exit 1
fi

# What's going on here with all of those variables? Good question!
#
# The QEMU build system, by default, picks up optional dependencies if they're
# installed on the system. And if they're not, it doesn't. For example: If
# running `configure` without any options and `libsdl2-image-dev` is installed,
# it will be included in the resulting binary. If it is not installed, it won't
# be included.
#
# This can lead to binaries having different dependencies depending on what
# container is used to build it. We don't want that. We want the binaries to
# have the same dependencies every time we build them. Basically, they should
# be (somewhat) reproducible. And that's what the variables are for.
#
# Instead of using the defaults, QEMU is configured with
# `--without-default-features` (see `DEFAULT_CONFIG_OPTS` below) and additional
# features are enabled explicitly.
#
# The naming of the `QEMU_*` variables comes from the `configure` output. When
# `configure` finishes, it prints a summary of the features which are enabled
# and categorizes them. The variables here try to match that. Strictly
# speaking, they're not necessary. There could just be one big list of options
# in `DEFAULT_CONFIG_OPTS`. But this makes it a little easier to find them.
#
# `DEFAULT_CONFIG_OPTS` is then used together with architecture-specific
# configure options to create an architecture-specific configure command, based
# on a generic configure command.
#
# The architecture-specific configure options can be used to override features
# that are set by the default configure options. That's because QEMU uses the
# last option passed on the command line to determine if a feature should be
# enabled. If configure is called like this: `configure --enable-FOO
# --disable-FOO` the feature FOO will be disabled.
#
# Example: The linux x86 build of qemu hexagon might look like this:
#
#   readonly HEX_CONFIG_CMD="
#       ${GENERIC_CONFIG_CMD}
#       ${DEFAULT_CONFIG_OPTS}
#       ${HEX_CONFIG_OPTS}
#       --target-list=hexagon-softmmu,hexagon-linux-user
#   "

readonly QEMU_CONFIGURABLE_FEATURES_OPTS="
    --enable-vhost-kernel
    --enable-vhost-net
    --enable-vhost-user
    --enable-vhost-crypto
    --enable-vhost-user-blk-server
    --enable-vhost-vdpa
"

readonly QEMU_COMPILATION_OPTS="--enable-malloc-trim"

readonly QEMU_TARGETS_AND_ACCELERATORS_OPTS="
    --enable-plugins
    --enable-multiprocess
"

readonly QEMU_BLOCK_LAYER_SUPPORT_OPTS="
    --enable-coroutine-pool
    --enable-virtfs
    --enable-replication
    --enable-bochs
    --enable-cloop
    --enable-dmg
    --enable-qcow1
    --enable-vdi
    --enable-vvfat
    --enable-qed
    --enable-parallels
    --enable-vduse-blk-export
    --enable-libvduse
"

readonly QEMU_CRYPTO_OPTS="--enable-keyring"

readonly QEMU_USER_INTERFACE_OPTS="--enable-png"

readonly QEMU_AUDIO_OPTS="--enable-oss --enable-pa --enable-alsa"

readonly QEMU_NETWORK_BACKENDS_OPTS="--enable-l2tpv3"

readonly QEMU_DEPENDENCIES_OPTS="
    --enable-iconv
    --enable-virglrenderer
    --enable-attr
    --enable-cap-ng
    --enable-opengl
    --enable-tpm
    --enable-libudev
"

readonly DEFAULT_CONFIG_OPTS="
    --without-default-features
    --assert-target-compiler
    ${QEMU_CONFIGURABLE_FEATURES_OPTS}
    ${QEMU_COMPILATION_OPTS}
    ${QEMU_TARGETS_AND_ACCELERATORS_OPTS}
    ${QEMU_BLOCK_LAYER_SUPPORT_OPTS}
    ${QEMU_CRYPTO_OPTS}
    ${QEMU_USER_INTERFACE_OPTS}
    ${QEMU_AUDIO_OPTS}
    ${QEMU_NETWORK_BACKENDS_OPTS}
    ${QEMU_DEPENDENCIES_OPTS}
"

readonly GENERIC_CONFIG_CMD="
    ${SOURCE_DIR}/configure
    --prefix=${INSTALL_DIR}
    --disable-install-blobs
"

readonly HEX_CONFIG_OPTS="
    --target-list=hexagon-softmmu,hexagon-linux-user
    --extra-ldflags=-lrt
"
readonly HEX_CONFIG_CMD="${GENERIC_CONFIG_CMD} ${DEFAULT_CONFIG_OPTS} ${HEX_CONFIG_OPTS}"
readonly HEX_SAN_CONFIG_CMD="
    ${HEX_CONFIG_CMD}
    --enable-asan
    --enable-ubsan
"
readonly HEX_NO_IDEF_PARSER_CONFIG_CMD="
    ${HEX_CONFIG_CMD}
    --disable-hexagon-idef-parser
"

readonly AARCH64_CONFIG_OPTS="
    --target-list=aarch64-softmmu,aarch64-linux-user
    --enable-fdt
    --enable-slirp
    --enable-sdl
    --enable-gtk
    --enable-pixman
    --enable-install-blobs
    --disable-png
    --disable-debug-info
    --disable-debug-tcg
    --gdb=aarch64-linux-gnu-gdb
"
readonly AARCH64_CONFIG_CMD="
    ${GENERIC_CONFIG_CMD}
    ${DEFAULT_CONFIG_OPTS}
    ${AARCH64_CONFIG_OPTS}
"

readonly HEX_CONFIG_OPTS_MAC="
    --target-list=hexagon-softmmu
    --disable-strip
    --disable-pie
    --disable-virglrenderer
    --disable-opengl
"
readonly HEX_CONFIG_CMD_MAC="${GENERIC_CONFIG_CMD} ${HEX_CONFIG_OPTS_MAC}"

readonly HEX_CONFIG_OPTS_WIN="
    --target-list=hexagon-softmmu
    --without-default-features
    --assert-target-compiler
    --qemu-testing-path=\"wine ${INSTALL_DIR}/qemu-system-hexagon.exe\"
    --cross-prefix=x86_64-w64-mingw32-
"
readonly HEX_WIN_CONFIG_CMD="${GENERIC_CONFIG_CMD} ${HEX_CONFIG_OPTS_WIN} "

readonly HEX_WIN_AARCH64_CONFIG_OPTS="
    --target-list=hexagon-softmmu
    --without-default-features
    --disable-werror
    --cross-prefix=aarch64-w64-mingw32-
    --extra-cflags=\"-isystem /clangarm64/include\"
    --extra-ldflags=\"-L/clangarm64/lib\"
"
readonly HEX_WIN_AARCH64_CONFIG_CMD="${GENERIC_CONFIG_CMD} ${HEX_WIN_AARCH64_CONFIG_OPTS}"

set -ex

# Select the configuration to build
if [ "${CONFIGURATION}" = "hexagon,linux-x86_64" ]; then
    CONFIG_CMD="${HEX_CONFIG_CMD}"
elif [ "${CONFIGURATION}" = "hexagon,linux-x86_64,san" ]; then
    CONFIG_CMD="${HEX_SAN_CONFIG_CMD}"
elif [ "${CONFIGURATION}" = "hexagon,linux-x86_64,no-idef-parser" ]; then
    CONFIG_CMD="${HEX_NO_IDEF_PARSER_CONFIG_CMD}"
elif [ "${CONFIGURATION}" = "hexagon,windows-x86_64" ]; then
    CONFIG_CMD="${HEX_WIN_CONFIG_CMD}"
elif [ "${CONFIGURATION}" = "hexagon,windows-aarch64" ]; then
    CONFIG_CMD="${HEX_WIN_AARCH64_CONFIG_CMD}"
elif [ "${CONFIGURATION}" = "hexagon,mac" ]; then
    CONFIG_CMD="${HEX_CONFIG_CMD_MAC}"
elif [ "${CONFIGURATION}" = "aarch64,linux-x86_64" ]; then
    CONFIG_CMD="${AARCH64_CONFIG_CMD}"
else
    echo "Error: Unknown configuration"
    print_help
    exit 1
fi

CONFIG_CMD=$(echo "${CONFIG_CMD}" | tr -d "\n" | tr -s " ")
readonly CONFIG_CMD

mkdir -p "${BUILD_DIR}"
mkdir -p "${INSTALL_DIR}"

cd "${BUILD_DIR}" || exit 1

eval "${CONFIG_CMD}"

make --jobs "$(getconf _NPROCESSORS_ONLN)"
make install
