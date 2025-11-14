#!/usr/bin/env sh

# Copyright(c) 2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
# SPDX-License-Identifier: GPL-2.0-or-later

set -ex

if [ -n "${CI_MERGE_REQUEST}" ]; then
    echo "Error: Not a merge request"
    exit 1
fi

GITLAB_URL="$(echo "${CI_SERVER_URL}" | sed -e 's/^.*:\/\///g' -e 's/:[0-9]*$//g')"
readonly GITLAB_URL

readonly TARGET_REPO="\
https://gitlab-ci-token:${CI_JOB_TOKEN}@\
${GITLAB_URL}/${CI_MERGE_REQUEST_PROJECT_PATH}.git"

# Note: we try set-url first when configuring the source repo because gitlab
# runners will preserve the repo configs in between builds, so `remote add`
# might fail on the second run.
git remote set-url target_repo "${TARGET_REPO}" >/dev/null 2>&1 ||
git remote add target_repo "${TARGET_REPO}"
git fetch target_repo "${CI_MERGE_REQUEST_TARGET_BRANCH_NAME}"

TARGET_REF="$(git rev-parse FETCH_HEAD)"
readonly TARGET_REF

SOURCE_REF="$(git rev-parse HEAD)"
readonly SOURCE_REF

if test -z "${TARGET_REF}" || test -z "${SOURCE_REF}"; then
    echo 'failed to set MR refs';
    exit 1;
fi

echo "TARGET_REF is $(git show --quiet --pretty=reference "${TARGET_REF}")"
echo "SOURCE_REF is $(git show --quiet --pretty=reference "${SOURCE_REF}")"
