#!/usr/bin/env sh

# Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
# SPDX-License-Identifier: GPL-2.0-or-later

QEMU_VERSION="${CI_COMMIT_TAG:-}"
QEMU_VERSION="${QEMU_VERSION#qemu-hexagon-}"
readonly QEMU_VERSION
