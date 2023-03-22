#!/usr/bin/env bash

if test $# -ne 6
then
    echo "missing args"
    exit 1
fi

GITLAB_URL="$1"
API_URL="$2"
PROJ_ID="$3"
API_TOK="$4"
JOB_TOK="$5"
REF_NAME="$6"

api_query() {
    local q="$1"
    echo "Querying '$q'" 1>&2
    curl -LsS -H "PRIVATE-TOKEN: $API_TOK" "$q"
}

api_parse() {
    local buf="$1"
    local regex="$2"
    printf "%s" "$buf" | tr -d "[:space:]" | sed -ne "s/$regex/\\1/p"
}

reply="$(api_query "$API_URL/projects/$PROJ_ID")"
target_repo="$(api_parse "$reply" '.*"forked_from_project":{.*"path_with_namespace":"\([^"]*\)".*' )"
target_id="$(api_parse "$reply" '.*"forked_from_project":{[^{]*"id":\([^,]*\),.*')"
remote_url="https://gitlab-ci-token:${JOB_TOKEN}@${GITLAB_URL}/${target_repo}.git"
if test -z "$target_repo"
then
    echo "Failed to query target repo"
    echo "Reply:"
    printf "%s\n" "$reply"
    exit 1
fi

echo "Found target repo '$target_repo', id #$target_id"
# Note: we try set-url first because gitlab runners will preserve the repo
# configs in between builds, so `remote add` would fail.
git remote set-url mainrepo $remote_url >/dev/null 2>&1 || git remote add mainrepo $remote_url || exit 1

reply="$(api_query "$API_URL/projects/$target_id/merge_requests?source_branch=$REF_NAME")"
target_branch=$(api_parse "$reply" '.*"target_branch":"\([^"]*\)".*')

if test -n "$target_branch"
then
    echo "Found target branch '$target_branch'"
    git fetch mainrepo $target_branch || exit 1
    error=0
    echo
    for rev in $(git rev-list --first-parent FETCH_HEAD..HEAD)
    do
        git rev-parse --verify -q $rev^2 >/dev/null && continue
        echo "========= Checking commit $(git log --pretty=reference $rev^!)"
        ./scripts/checkpatch.pl --color=always --no-signoff --branch "$rev^!"
        error=$(($error || $?))
    done
    if test $? -ne 0 || test $error -ne 0
    then
        exit 1
    fi
else
    echo "DIDN'T FIND TARGET BRANCH"
    printf "API reply is '%s'\n" "$reply"
fi
