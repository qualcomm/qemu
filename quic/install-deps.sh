#!/usr/bin/env sh

# Copyright (c) 2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
# SPDX-License-Identifier: GPL-2.0-or-later

# Install build dependencies for libqemu.
# This script is idempotent and safe to run multiple times.

unsupported_operating_system() {
    echo "Error: Unsupported operating system"
    exit 1
}

if { [ -e /etc/os-release ] && OS_RELEASE="/etc/os-release"; } || \
   { [ -e /usr/lib/os-release ] && OS_RELEASE="/usr/lib/os-release"; }; then
    # shellcheck source=/dev/null
    . "${OS_RELEASE}"

    if [ "${ID}" = "ubuntu" ]; then
        export DEBIAN_FRONTEND=noninteractive

        apt-get --yes update
        apt-get --yes install \
            bison build-essential cmake expect flex \
            libasio-dev libc++1 libc++abi1 libcap-ng-dev libcbor-dev libepoxy-dev \
            libgcrypt20-dev libglib2.0-dev libgtk-3-dev libgnutls28-dev libpng-dev \
            libpixman-1-dev libpulse-dev libseccomp-dev libssh-dev \
            libudev-dev liburing-dev libunwind8 libvirglrenderer-dev \
            libsdl2-dev libsdl2-image-dev \
            ninja-build pkg-config python3 python3-venv
    elif [ "${ID}" = "msys2" ]; then
        pacman -S --noconfirm --needed \
            base-devel msys2-runtime-devel pactoys \
            make binutils bison flex diffutils git

        # pacboy handles MSYS2 subsystem package prefixes automatically
        pacboy -S --noconfirm --needed \
            toolchain cmake ninja meson pkgconf \
            glib2 pixman libslirp SDL2 SDL2_image libepoxy \
            gnutls libgcrypt libcbor libssh \
            python python-pexpect
    else
        unsupported_operating_system
    fi
else
    case "$(uname)" in
        Darwin*)
            brew tap quic/quic https://github.com/quic/homebrew-quic.git
            brew trust quic/quic
            brew install \
                pkg-config ninja cmake bison flex \
                glib pixman sdl2 sdl2_image \
                libgcrypt gnutls libcbor libssh \
                quic/quic/virglrenderer
            ;;
        *)
            unsupported_operating_system
            ;;
    esac
fi
