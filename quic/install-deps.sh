#!/usr/bin/env sh

# Copyright(c) 2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
# SPDX-License-Identifier: GPL-2.0-or-later

set -ex

export DEBIAN_FRONTEND=noninteractive

apt-get --yes update
apt-get --yes install bison build-essential cmake expect flex libasound2-dev \
    libcap-ng-dev libepoxy-dev libglib2.0-dev libpng-dev libpulse-dev \
    libudev-dev libvirglrenderer-dev ninja-build pkg-config python3 \
    python3-venv
