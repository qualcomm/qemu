#!/usr/bin/env sh

# Copyright(c) 2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
# SPDX-License-Identifier: GPL-2.0-or-later

# Creates a libqemu tag and pushes it to the correct remotes.
# Tag format: libqemu-{QEMU_VERSION}-{LIBQEMU_VERSION}

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd -P)"
readonly SCRIPT_DIR

. "${SCRIPT_DIR}/help.sh"

readonly DEFAULT_REMOTES="origin quic-hub"

HELP_MESSAGE=$(cat << EOF
Usage: $(basename "${0}") [OPTIONS]

Creates a libqemu tag on the current commit and pushes the current branch
and tag to remotes. Tag format: libqemu-{QEMU_VERSION}-{LIBQEMU_VERSION}

By default, shows a preview of what would be created without making any
changes. Pass -y to actually create the tag and push.

Options:
    -q    QEMU version prefix (e.g. v10.1)
          (default: auto-detect from latest local libqemu tag)
    -l    libqemu release version (e.g. v0.12)
          (default: auto-increment from latest tag with same prefix)
    -r    space-separated list of remotes to push to
          (default: ${DEFAULT_REMOTES})
    -y    create the tag and push (default: preview only)
    -h    print this help

Examples:
    $(basename "${0}")
        preview the tag that would be created
    $(basename "${0}") -y
        auto-detect prefix, increment version, create and push
    $(basename "${0}") -q v10.1 -l v0.12 -y
        create libqemu-v10.1-v0.12 and push
EOF
)

run()
{
    PREVIEW="${1}"
    shift
    if "${PREVIEW}"; then
        echo "[preview] $*"
    else
        "$@"
    fi
}

readonly OPTIONS=":hq:l:r:y"
while getopts "${OPTIONS}" option; do
    case "${option}" in
        "q") readonly QEMU_VERSION="${OPTARG}";;
        "l") readonly LIBQEMU_VERSION="${OPTARG}";;
        "r") readonly REMOTES="${OPTARG}";;
        "y") readonly PREVIEW=false;;
        "h") print_help;;
        "?") print_help_error "Unknown option: -${OPTARG}";;
        ":") print_help_error "Option -${OPTARG} requires an argument";;
    esac
done

if [ -z "${PREVIEW}" ]; then
    readonly PREVIEW=true
fi

if [ -z "${REMOTES}" ]; then
    readonly REMOTES="${DEFAULT_REMOTES}"
fi

BRANCH="$(git branch --show-current)"
readonly BRANCH

if [ -z "${BRANCH}" ]; then
    print_help_error "not on a branch (detached HEAD); check out a branch first"
fi

case "${BRANCH}" in
    libqemu*) ;;
    *) print_help_error "current branch '${BRANCH}' is not a libqemu branch";;
esac

# Auto-detect QEMU version prefix from latest local libqemu tag
if [ -z "${QEMU_VERSION}" ]; then
    LATEST_QEMU_TAG="$(git tag --list 'libqemu-v*' \
        | grep -E '^libqemu-v[0-9]+\.[0-9]+-v[0-9]+\.[0-9]+$' \
        | sort -V | tail -1)"
    readonly LATEST_QEMU_TAG
    if [ -z "${LATEST_QEMU_TAG}" ]; then
        print_help_error \
            "no existing libqemu tags found; specify -q to set the QEMU version"
    fi

    # Extract "vX.Y" from "libqemu-vX.Y-vA.B"
    QEMU_VERSION="$(echo "${LATEST_QEMU_TAG}" \
        | sed 's/^libqemu-\(v[0-9]*\.[0-9]*\)-.*$/\1/')"
    readonly QEMU_VERSION

    echo "Auto-detected QEMU version: ${QEMU_VERSION}"
fi

# Auto-increment libqemu version from the latest tag with the same prefix
if [ -z "${LIBQEMU_VERSION}" ]; then
    readonly PREFIX="libqemu-${QEMU_VERSION}-"

    LATEST_LIBQEMU_TAG="$(git tag --list "${PREFIX}v*" \
        | grep -E "^${PREFIX}v[0-9]+\.[0-9]+$" \
        | sort -V | tail -1)"
    readonly LATEST_LIBQEMU_TAG
    if [ -z "${LATEST_LIBQEMU_TAG}" ]; then
        readonly NEXT_MINOR=1
    else
        CURRENT_MINOR="$(echo "${LATEST_LIBQEMU_TAG}" | sed "s/^${PREFIX}v0\.//")"
        readonly CURRENT_MINOR
        readonly NEXT_MINOR=$((CURRENT_MINOR + 1))
    fi

    readonly LIBQEMU_VERSION="v0.${NEXT_MINOR}"

    echo "Auto-incremented libqemu version: ${LIBQEMU_VERSION}"
fi

readonly TAG_NAME="libqemu-${QEMU_VERSION}-${LIBQEMU_VERSION}"

# Check the tag does not already exist
if git rev-parse --verify "refs/tags/${TAG_NAME}" > /dev/null 2>&1; then
    print_help_error "tag '${TAG_NAME}' already exists locally"
fi

run "${PREVIEW}" git tag --annotate "${TAG_NAME}" --message "${TAG_NAME}"

for REMOTE in ${REMOTES}; do
    run "${PREVIEW}" git push "${REMOTE}" "${BRANCH}"
    run "${PREVIEW}" git push "${REMOTE}" "refs/tags/${TAG_NAME}"
done

if "${PREVIEW}"; then
    echo "Run with -y to create the tag and push."
else
    echo "Done."
fi
