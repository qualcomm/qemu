#!/usr/bin/env sh

# Copyright(c) 2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
# SPDX-License-Identifier: GPL-2.0-or-later

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

readonly QEMU_CONFIGURABLE_FEATURES_OPTS="
    --enable-vhost-kernel
    --enable-vhost-net
    --enable-vhost-user
    --enable-vhost-crypto
    --enable-vhost-user-blk-server
    --enable-vhost-vdpa
"

readonly QEMU_COMPILATION_OPTS="
    --enable-malloc-trim
"

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

readonly QEMU_CRYPTO_OPTS="
    --enable-keyring
"

readonly QEMU_USER_INTERFACE_OPTS="
    --enable-png
"

readonly QEMU_AUDIO_OPTS="
    --enable-oss
    --enable-pa
    --enable-alsa
"

readonly QEMU_NETWORK_BACKENDS_OPTS="
    --enable-l2tpv3
"

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
"

load_config()
{
    readonly CONFIG_NAME="${1}"
    # shellcheck disable=2155
    readonly CONFIG_FUNCTION="config_$(echo "${CONFIG_NAME}" | tr '-' '_')"

    if ! type "${CONFIG_FUNCTION}" >/dev/null 2>&1; then
        return 1
    fi

    "${CONFIG_FUNCTION}"

    # Clean up CONFIG_CMD by removing extra whitespace
    CONFIG_CMD=$(echo "${CONFIG_CMD}" | tr "\n" " " | tr -s " ")
    readonly CONFIG_CMD
}

# To add a new configuration, simply define a function named "config_<name>" It
# will be automatically discovered by the build system. The function should set
# the CONFIG_CMD variable. Add a "# desc: ..." comment right above the function
# to provide a description that will be shown in the list command.

# desc: Hexagon system and linux-user w/ standard features
config_hexagon()
{
    CONFIG_CMD="
        ${GENERIC_CONFIG_CMD}
        ${DEFAULT_CONFIG_OPTS}
        --enable-fdt
        --enable-slirp
        --extra-ldflags=-lrt
        --target-list=hexagon-softmmu,hexagon-linux-user
    "
}

# desc: Hexagon system only, no optional features
config_hexagon_minimal()
{
    CONFIG_CMD="
        ${GENERIC_CONFIG_CMD}
        --target-list=hexagon-softmmu
        --without-default-features
    "
}

# desc: Same as "hexagon_minimal" but for Windows cross-compilation
config_hexagon_minimal_cross()
{
    config_hexagon_minimal
    CONFIG_CMD="
        ${CONFIG_CMD}
        --cross-prefix=x86_64-w64-mingw32-
    "
}

# desc: Same as "hexagon" but for linux-aarch64 cross-compilation
config_hexagon_linux_aarch64_cross()
{
    config_hexagon
    CONFIG_CMD="
        ${CONFIG_CMD}
        --cross-prefix=aarch64-linux-gnu-
    "
}

# desc: Same as "hexagon" + adress and UB sanitizers
config_hexagon_san()
{
    config_hexagon
    CONFIG_CMD="
        ${CONFIG_CMD}
        --enable-asan
        --enable-ubsan
    "
}

# desc: Same as "hexagon" but w/o idef-parser
config_hexagon_no_idef_parser()
{
    config_hexagon
    CONFIG_CMD="
        ${CONFIG_CMD}
        --disable-hexagon-idef-parser
    "
}

# desc: Hexagon system for macOS
config_hexagon_mac()
{
    CONFIG_CMD="
        ${GENERIC_CONFIG_CMD}
        --target-list=hexagon-softmmu
        --disable-strip
        --disable-pie
        --disable-virglrenderer
        --disable-opengl
    "
}

# desc: AArch64 system and linux-user w/GUI support
config_aarch64()
{
    CONFIG_CMD="
        ${GENERIC_CONFIG_CMD}
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
}
