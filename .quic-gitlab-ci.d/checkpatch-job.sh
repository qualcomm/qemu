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

set -E
trap '[ "$?" -ne 128 ] || exit 128' ERR
fatal() {
    echo "fatal: $@" >&2
    exit 128
}

api_query() {
    local q="$1"
    echo "Querying '$q'" 1>&2
    curl -LsS -H "PRIVATE-TOKEN: $API_TOK" "$q" || fatal "api_query failed!"
}

json_parse() {
    local data="$1"
    local key="$2"
    printf "%s" "$data" | python3 -c \
"import sys, json
data = json.load(sys.stdin)
if type(data) == list:
    val = data[$key] if abs($key) < len(data) else 'none'
elif type(data) == dict:
    val = data['$key'] if '$key' in data else 'none'
else:
    raise(Exception('data is neither a list nor a dict'))
if type(val) in [list, dict]:
    print(json.dumps(val), end='')
else:
    print(val, end='')"
    if test $? -ne 0
    then
        echo "json_parse error:" >&2
        printf "Data: >%s<\n" "$data" >&2
        printf "Key: >%s<\n" "$key" >&2
        fatal "json_parse() failed!"
    fi
}

reply="$(api_query "$API_URL/projects/$PROJ_ID")"

is_fork=yes
repo_json="$(json_parse "$reply" forked_from_project)"
if test "$repo_json" == none
then
    repo_json="$reply"
    is_fork=no
fi

target_repo="$(json_parse "$repo_json" path_with_namespace)"
target_id="$(json_parse "$repo_json" id)"
if test "$target_repo" == none || test "$target_id" == none
then
    echo "Failed to query target repo"
    echo "Reply:"
    printf "%s\n" "$reply"
    exit 1
fi
echo "Found target repo '$target_repo', id #$target_id, is at fork: $is_fork"

remote_url="https://gitlab-ci-token:${JOB_TOKEN}@${GITLAB_URL}/${target_repo}.git"
# Note: we try set-url first because gitlab runners will preserve the repo
# configs in between builds, so `remote add` would fail.
git remote set-url mainrepo $remote_url >/dev/null 2>&1 || git remote add mainrepo $remote_url || exit 1

reply="$(api_query "$API_URL/projects/$target_id/merge_requests?source_branch=$REF_NAME")"
mr_data="$(json_parse "$reply" 0)"
if test "$mr_data" != none
then
    target_branch="$(json_parse "$mr_data" target_branch)"
else
    target_branch=none
fi

if test "$target_branch" != none
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
    echo "DIDN'T FIND TARGET BRANCH. Is this really an MR?"
    printf "API reply is '%s'\n" "$reply"
fi
