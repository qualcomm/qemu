#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later

set -euo pipefail

die()
{
    echo 1>&2 "$@"
    exit 1
}

# download latest emails
fetch_mails()
{
    out=$1
    rm -rf $out
    mkdir -p $out
    rm -rf lore

    # https://lore.kernel.org/qemu-devel/_/text/mirror/
    num_messages=5000
    # current archive is 3, 4 may appear one day
    if git clone https://lore.kernel.org/qemu-devel/4 lore --depth $num_messages; then
        echo "new git archive available, need to update this script"
        exit 1
    fi
    git clone https://lore.kernel.org/qemu-devel/3 lore --depth $num_messages
    for i in $(seq 0 $((num_messages - 1)))
    do
        echo "git -C lore show HEAD~$i:m > $out/$i"
    done | parallel -j$(nproc)
}

find_series()
{
    mail_dir=$1
    # find all message id, for mails without a reference (i.e. first in series).
    grep -Lri '^References: .' $mail_dir | sort | while read m
    do
        # skip messages replying to thread
        if grep -qi '^In-Reply-to: .' $m; then
            continue
        fi
        msg_id=$(grep -i '^message-id: ' $m | head -n1 |
                 sed -e 's/.*<//' -e 's/>$//')
        date=$(grep -i '^date: ' $m | head -n1 | sed -e 's/^date: //I')
        echo "$msg_id|$date"
    done
}

fetch_repositories()
{
    git remote remove upstream || true
    git remote add upstream -f https://gitlab.com/qemu-project/qemu
    git fetch -a origin -p
}

push_one_series()
{
    s="$1"; shift
    apply_revision="$1"; shift
    echo "-----------------------------------------------------------------"
    msg_id=$(echo "$s" | cut -f 1 -d '|')
    date=$(echo "$s" | cut -f 2 -d '|')
    echo "$msg_id | $date"
    if git rev-parse "remotes/origin/$msg_id" >& /dev/null; then
        return
    fi

    # find git commit on master close to date of series
    base_git_revision=$(git log -n1 --format=format:%H --before="$date" \
                        --first-parent upstream/master)
    echo "push this new series, applied from $base_git_revision"
    git checkout "$base_git_revision" >& /dev/null
    git branch -D new_series >& /dev/null || true
    git checkout -b new_series

    # apply series
    b4_params="--add-link --allow-unicode-control-chars"
    if ! b4 shazam $b4_params $msg_id |& tee shazam.log; then
        git am --abort || true
        git add shazam.log
        git commit -m 'b4 shazam failed'
    fi

    if grep -qi 'message-id is not known' shazam.log; then
        echo "no thread found for: $msg_id"
        return
    fi

    if grep -qi 'no patches found' shazam.log; then
        echo "no patches found in series: $msg_id"
        return
    fi

    if grep -qi 'Server returned an error: 503' shazam.log; then
        echo "server returned an error 503, try again later: $msg_id"
        return
    fi

    if grep -qi 'Server returned an error: 502' shazam.log; then
        echo "server returned an error 502, try again later: $msg_id"
        return
    fi

    if grep -qi 'OSError: [Errno 101] Network is unreachable' shazam.log; then
        echo "network error: $msg_id"
        return
    fi

    if grep -qi 'ERROR: missing' shazam.log; then
        # try again next time, let time for lore server to receive all patches
        echo "missing patches in series: $msg_id"
        return
    fi

    rm -f ./*.mbx
    b4 mbox "$msg_id" --single-message
    subject=$(grep -i '^Subject:' *.mbx | sed -e 's/Subject:\s*//')

    if echo "$subject" | grep -i "^Re:"; then
        echo "this message has no reference but is an answer: $msg_id"
        return
    fi

    git push --set-upstream origin "new_series:$msg_id"
    cat > commit_msg << EOF
$subject

https://lore.kernel.org/qemu-devel/$msg_id

---

$(grep -A 100000 -i '^From:' *.mbx)
EOF
    # apply a specific patch
    if [ "$apply_revision" != "" ]; then
        git cherry-pick "$apply_revision" --no-commit
        git commit -a -F commit_msg --signoff
    fi
    # let some time to GitHub to order branches
    sleep 5
    git push --set-upstream origin "new_series:${msg_id}_ci"
}

fetch_repositories
apply_range=$(git merge-base origin/ci upstream/master)
[ "$apply_range" != "" ] || die "can't find revisions to apply to series"
# apply all commits on ci branch
apply_range=$apply_range..origin/ci

fetch_mails mails
find_series mails | while read s; do push_one_series "$s" "$apply_range"; done
