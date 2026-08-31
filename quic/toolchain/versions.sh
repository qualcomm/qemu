# Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
# SPDX-License-Identifier: GPL-2.0-or-later

# shellcheck disable=2034
readonly BAREMETAL_BRANCH_VERSION=23.0
readonly BAREMETAL_BRANCH="branch-${BAREMETAL_BRANCH_VERSION}"
readonly BAREMETAL_TOOLSET_VERSION=8300
readonly BAREMETAL_TOOLSET="toolset-${BAREMETAL_TOOLSET_VERSION}"
readonly BAREMETAL_VERSION="${BAREMETAL_BRANCH_VERSION}\
-${BAREMETAL_TOOLSET_VERSION}"
readonly BAREMETAL_DST_PATH="./baremetal-${BAREMETAL_VERSION}"
readonly MUSL_VERSION=22.1.8
readonly MUSL_DST_PATH="./musl-${MUSL_VERSION}"
readonly EXTENDED_TARBALL="hexagon-toolchain-${BAREMETAL_VERSION}\
-extended.tar.zst"
readonly EXTENDED_REVISION="rev1"
