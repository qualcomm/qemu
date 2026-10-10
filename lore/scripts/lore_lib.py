# SPDX-License-Identifier: GPL-2.0-or-later
"""Shared helpers for querying a local public-inbox mirror of qemu-devel via lei.

The goal of everything in here is token economy: a mailing-list message is mostly
quoted text, signatures and diff noise, and an agent that reads raw mbox output
burns its context on `> > >` instead of on answers.  So search returns metadata
only, and thread retrieval trims quotes to a few lines of context and collapses
diffs by default.
"""

import json
import os
import re
import subprocess
from datetime import datetime, timezone
from email import message_from_bytes
from email.header import decode_header, make_header
from email.utils import parsedate_to_datetime
from urllib.parse import quote, unquote, urlsplit

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)

# The scripts live in <checkout>/lore; the mirror is tens of GB of cloned git
# epochs, so it lives under <checkout>/.cache/lore, which QEMU's .gitignore
# already covers via /.cache/ -- no ignore rule of our own to maintain.
DATA = os.environ.get("EMAIL_LORE_DIR") or os.path.join(
    os.path.dirname(ROOT), ".cache", "lore")

INBOX = os.environ.get("EMAIL_INBOX") or os.path.join(DATA, "qemu-devel")
LORE_BASE = os.environ.get("EMAIL_LORE_BASE", "https://lore.kernel.org/qemu-devel")

# Seconds before a child process is given up on.  The MCP server answers one
# request at a time, so a single wedged lei, git or b4 would otherwise hang
# every call after it.
TIMEOUT = int(os.environ.get("EMAIL_LORE_TIMEOUT") or 300)


# lei exits 0 with no output on an empty result set, so these are real errors.
class LeiError(RuntimeError):
    pass


def run(cmd, error=RuntimeError, input=None, timeout=TIMEOUT):
    """subprocess.run that never reads our stdin and never waits forever.

    The MCP server's stdin is its JSON-RPC pipe: a child inheriting it could
    swallow protocol frames, so children get /dev/null unless fed `input`.
    """
    kwargs = {"input": input} if input is not None else {"stdin": subprocess.DEVNULL}
    try:
        return subprocess.run(cmd, capture_output=True, timeout=timeout, **kwargs)
    except subprocess.TimeoutExpired:
        raise error("%s timed out after %ds" % (cmd[0], timeout))


def norm_msgid(value):
    """A Message-ID as a user or an agent is likely to hand it over, made bare.

    Besides "<id>", accept what our own output shows: the "id:" prefix of a
    search hit, and a lore URL (whose msgid is percent-escaped).
    """
    s = (value or "").strip()
    if re.match(r"https?://", s):
        for seg in urlsplit(s).path.split("/"):
            seg = unquote(seg)
            if "@" in seg:
                return seg.strip("<> ")
    s = re.sub(r"^(id|mid|m):", "", s)
    return s.strip("<> ")


def url_for(msgid):
    # public-inbox's mid_escape: RFC 3986 pchars stay, '/' and the rest don't.
    mid = quote(norm_msgid(msgid), safe="-._~!$&'()*+,;=:@")
    return "%s/%s/" % (LORE_BASE.rstrip("/"), mid)


def _run_lei(query, fmt, limit=None, threads=False, inbox=None, extra=None):
    cmd = ["lei", "q", "--only", inbox or INBOX, "-f", fmt]
    if limit is not None:
        cmd += ["-n", str(limit)]
    if threads:
        cmd.append("--threads")
    if extra:
        cmd += list(extra)
    # On stdin, not argv: lei expects a shell-split query on its command line
    # and quotes any argument with a space in it as a phrase, so one argv
    # "s:(a b)" became s:"(a b)" and "s:a AND f:b" one long phrase that
    # matched nothing.  --stdin hands the string to Xapian as lore's q= does.
    cmd.append("--stdin")
    p = run(cmd, LeiError, input=query.encode("utf-8"))
    if p.returncode != 0:
        err = p.stderr.decode("utf-8", "replace").strip()
        raise LeiError("lei q failed (%d): %s" % (p.returncode, err or "no stderr"))
    return p.stdout


def _addrs(value, cap=4):
    """lei renders address headers as nested [name, addr] pairs; flatten tolerantly."""
    out = []
    if isinstance(value, str):
        out = [value]
    elif isinstance(value, list):
        for item in value:
            if isinstance(item, str):
                out.append(item)
            elif isinstance(item, list):
                parts = [str(x) for x in item if x]
                # Prefer the address (the part containing '@') plus a name if present.
                addr = next((x for x in parts if "@" in x), None)
                name = next((x for x in parts if "@" not in x), None)
                if addr and name:
                    out.append("%s <%s>" % (name, addr))
                elif addr or name:
                    out.append(addr or name)
    if len(out) > cap:
        out = out[:cap] + ["... +%d more" % (len(out) - cap)]
    return out


def search(query, limit=10000, threads=False, inbox=None):
    """Return compact metadata hits, newest-relevance first. No message bodies."""
    raw = _run_lei(query, "jsonl", limit=limit, threads=threads, inbox=inbox)
    hits = []
    for line in raw.splitlines():
        line = line.strip()
        if not line or line == "[]":
            continue
        try:
            rec = json.loads(line)
        except ValueError:
            continue
        if not isinstance(rec, dict):
            continue
        msgid = rec.get("m")
        if isinstance(msgid, list):
            msgid = msgid[0] if msgid else None
        hit = {
            "message_id": msgid,
            "subject": rec.get("s"),
            "from": _addrs(rec.get("f")),
            "date": rec.get("dt") or rec.get("rt") or rec.get("d"),
            # When the list got it: Date: is the sender's to set, and some
            # patches carry their author date, months before the posting.
            "received": rec.get("rt"),
            "url": url_for(msgid) if msgid else None,
        }
        if rec.get("pct") is not None:
            hit["relevance"] = rec["pct"]
        hits.append({k: v for k, v in hit.items() if v not in (None, [], "")})
    # lei applies -n to the matches, then --threads adds every message of
    # their threads: three hits on PULL mail expand to over a hundred.  The
    # limit is what keeps this out of the agent's context, so it caps the
    # expanded list too.
    if limit and len(hits) > limit:
        hits = hits[:limit]
    return hits


# --- body cleanup -----------------------------------------------------------

# Only '>': lines starting with '|' are far more often register tables and
# monitor output in new text than an exotic quoting style.
_QUOTE = re.compile(r"^\s*>")
_ATTRIB = re.compile(
    r"^\s*(On .{0,120}(wrote|said):|.{0,80} writes:|Quoting .{0,80}:)\s*$"
)
# Locale-independent fallback: attribution lines end in ':' and name an address.
# Catches e.g. "Am Mi., 7. Okt. 2026 um 15:27 Uhr schrieb Peter Xu <x@y>:".
_ATTRIB_ADDR = re.compile(r"^\s*\S.{0,200}<[^<>@\s]+@[^<>@\s]+>\s*:\s*$")
# What a real attribution carries and prose ending in "writes:" rarely does.
_ATTRIB_MARK = re.compile(r"\d|@")
_DIFF_START = re.compile(r"^(diff --git |index [0-9a-f]{7,}\.\.|--- a/|\+\+\+ b/|@@ )")
_DIFF_FILE = re.compile(r"^diff --git a/(\S+) b/(\S+)")
# A diff pasted without its "diff --git" line still names the file here.
_DIFF_NEW = re.compile(r"^\+\+\+ b/(\S+)")
_HUNK = re.compile(r"^@@ -\d+(?:,(\d+))? \+\d+(?:,(\d+))? @@")
# Lines that can sit between hunks without ending the diff.
_DIFF_HEADER = re.compile(
    r"^(diff --git |index |--- |\+\+\+ |(new|deleted) file mode |old mode |"
    r"new mode |(dis)?similarity index |rename (from|to) |copy (from|to) |"
    r"Binary files |GIT binary patch|(literal|delta) \d+$|\\)"
)
_SIG = re.compile(r"^-- \s*$")
# format-patch's "---" ends the commit message, not the mail: below it come
# the version notes ("v2: addressed Peter's review"), then the diffstat.
_NOTES = re.compile(r"^---\s*$")
_DIFFSTAT = re.compile(
    r"^ (\S.* \| +(\d+ ?\+*-*|Bin .*)|\d+ files? changed.*|"
    r"(create|delete) mode \d+ .*|rename .* \(\d+%\)|mode change .*)$"
)

SNIP = "[...]"


def _snip_quotes(kinds, lines, context):
    """Keep every new line, plus `context` quoted lines hugging each new block.

    A bare "Yes, that works." is unreadable without the sentence it answers, so
    dropping quotes wholesale can cost more than it saves: the agent has to fetch
    the parent message to interpret the reply.  Keeping a few quoted lines on
    either side of each run of new text preserves the claim/response adjacency
    that carries the argument, at a bounded cost per reply.

    Returns a keep-mask parallel to `lines`.
    """
    n = len(lines)
    keep = [k == "new" for k in kinds]
    blank = [not ln.strip() for ln in lines]

    i = 0
    while i < n:
        if kinds[i] != "new":
            i += 1
            continue
        # Maximal run of new lines; only runs with real content anchor context,
        # so a blank line between two quote blocks doesn't drag either in.
        j = i
        while j < n and kinds[j] == "new":
            j += 1
        if any(not blank[k] for k in range(i, j)):
            for step, start in ((-1, i - 1), (1, j)):
                budget = context
                k = start
                while 0 <= k < n and budget > 0:
                    if kinds[k] == "attrib" and step < 0:
                        # "On <date>, <name> wrote:" names who is being
                        # answered -- high value per token, and the top of the
                        # quoted block, so take it free and stop.
                        keep[k] = True
                        break
                    if kinds[k] in ("quote", "attrib"):
                        keep[k] = True
                        budget -= 1
                    elif not blank[k]:
                        break  # new text: a different block's business
                    k += step
        i = j

    # The context walk above runs out of budget before reaching the attribution
    # of a long quote block, so pull in the head of every block we kept any of:
    # quoted text whose speaker is unknown invites misattribution.
    for i in range(n):
        if not (keep[i] and kinds[i] == "quote"):
            continue
        k = i - 1
        while k >= 0 and (kinds[k] == "quote" or blank[k]):
            k -= 1
        if k >= 0 and kinds[k] == "attrib":
            keep[k] = True
    return keep


def _is_base85(line):
    """A line of a "GIT binary patch": a length char, then 5 chars per 4 bytes."""
    c = line[:1]
    if "A" <= c <= "Z":
        n = ord(c) - ord("A") + 1
    elif "a" <= c <= "z":
        n = ord(c) - ord("a") + 27
    else:
        return False
    return len(line) == 1 + (n + 3) // 4 * 5


def clean_body(text, include_diffs=False, max_chars=2000, quote_context=3):
    """Clean one message body for reading.

    Signatures go, diffs are kept or summarised, and quoted text is trimmed to
    `quote_context` lines around each block of new text (0 strips quotes
    entirely, as this function originally did).
    """
    lines = text.splitlines()
    kinds = []
    body_lines = []
    diff_files = []
    hunks = 0
    in_diff = False
    binary = False
    old_left = new_left = 0
    git_file = False  # diff --git named the file, not its +++ line
    notes = False
    sig = False

    for line in lines:
        in_hunk = old_left > 0 or new_left > 0
        if in_diff:
            # Hunk headers say how many lines follow, so the diff ends exactly
            # where its last hunk does: prose after an inline diff ("Also,
            # please fix the commit message") and the "-- " signature are not
            # diff.  Mailers strip the space off blank context lines, so
            # anything that isn't '+', '-' or '\' counts as context.
            if in_hunk:
                c = line[:1]
                if c == "-":
                    old_left -= 1
                elif c == "+":
                    new_left -= 1
                elif c != "\\":
                    old_left -= 1
                    new_left -= 1
            elif not (_HUNK.match(line) or _DIFF_HEADER.match(line)
                      or not line.strip() or (binary and _is_base85(line))):
                in_diff = binary = False
        elif _DIFF_START.match(line):
            in_diff = True
        if in_diff:
            m = _DIFF_FILE.match(line)
            if m:
                diff_files.append(m.group(2))
                binary = False
                git_file = True
            m = not in_hunk and _DIFF_NEW.match(line)
            if m:
                if not git_file:
                    diff_files.append(m.group(1))
                git_file = False
            m = _HUNK.match(line)
            if m and old_left <= 0 and new_left <= 0:
                hunks += 1
                old_left = int(m.group(1) or 1)
                new_left = int(m.group(2) or 1)
            if line.startswith("GIT binary patch"):
                binary = True
            if include_diffs:
                body_lines.append(line)
                kinds.append("new")
            continue
        if sig:
            continue
        if _SIG.match(line):
            sig = True
            continue
        if _NOTES.match(line):
            notes = True
        elif notes and _DIFFSTAT.match(line):
            continue
        quoted = bool(_QUOTE.match(line))
        attrib = bool(_ATTRIB.match(line) or _ATTRIB_ADDR.match(line))
        body_lines.append(line)
        kinds.append("quote" if quoted else "attrib" if attrib else "new")

    # The attribution patterns also fit prose ("On reset the guest writes:"),
    # and an attribution is dropped as quote metadata.  A real one names a
    # date or an address, or failing that is followed by the quote itself;
    # not always directly, as in "On <date>, X wrote:\n\nThanks!\n\n> ...".
    nxt = "new"
    for i in range(len(kinds) - 1, -1, -1):
        if (kinds[i] == "attrib" and nxt != "quote"
                and not _ATTRIB_MARK.search(body_lines[i])):
            kinds[i] = "new"
        if body_lines[i].strip():
            nxt = kinds[i]

    if quote_context > 0:
        keep = _snip_quotes(kinds, body_lines, quote_context)
    else:
        keep = [k == "new" for k in kinds]

    kept = []
    elided = False
    for i, line in enumerate(body_lines):
        if keep[i]:
            # With no context budget the caller asked for prose only, so don't
            # spend tokens marking the holes.
            if elided and quote_context > 0:
                kept.append(SNIP)
            elided = False
            kept.append(line)
        else:
            elided = True

    body = "\n".join(kept)
    body = re.sub(r"\n{3,}", "\n\n", body).strip()
    # A "---" with no version notes under it separates nothing.
    body = re.sub(r"(^|\n)---$", "", body).strip()

    # Truncate the prose FIRST so the diff summary below always survives; it is
    # usually the most useful line in a patch mail.
    truncated = False
    if max_chars and len(body) > max_chars:
        body = body[:max_chars].rstrip()
        truncated = True

    # A reviewer's suggestion is often a bare hunk, with no file to name;
    # dropping it unannounced would leave "something like:" answering nothing.
    if diff_files and not include_diffs:
        shown = diff_files[:12]
        more = len(diff_files) - len(shown)
        summary = "[diff omitted: %d file(s), %d hunk(s)] %s%s" % (
            len(diff_files),
            hunks,
            ", ".join(shown),
            " ... +%d more" % more if more > 0 else "",
        )
        body = (body + "\n\n" + summary).strip()
    elif hunks and not include_diffs:
        summary = "[diff omitted: %d hunk(s)]" % hunks
        body = (body + "\n\n" + summary).strip()

    return body, truncated


def _hdr(msg, name):
    val = msg.get(name)
    if not val:
        return None
    try:
        return str(make_header(decode_header(val))).replace("\n", " ").strip()
    except Exception:
        return str(val).replace("\n", " ").strip()


_EPOCH = datetime(1970, 1, 1, tzinfo=timezone.utc)


def _parse_date(raw):
    """RFC822 Date: -> (sort_key, ISO-8601 UTC string). Both tolerant of junk."""
    if not raw:
        return _EPOCH, None
    try:
        dt = parsedate_to_datetime(raw)
    except (TypeError, ValueError):
        return _EPOCH, raw
    if dt is None:
        return _EPOCH, raw
    if dt.tzinfo is None:
        dt = dt.replace(tzinfo=timezone.utc)
    return dt, dt.astimezone(timezone.utc).strftime("%Y-%m-%d %H:%M:%SZ")


def _decode(payload, charset):
    # Old list mail declares charsets Python has never heard of
    # ("unknown-8bit", "x-unknown"); one of those must not sink a whole thread.
    try:
        return payload.decode(charset or "utf-8", "replace")
    except LookupError:
        return payload.decode("utf-8", "replace")


def _text_part(msg):
    """First text/plain part, decoded best-effort."""
    if msg.is_multipart():
        for part in msg.walk():
            if part.get_content_type() == "text/plain":
                payload = part.get_payload(decode=True)
                if payload:
                    return _decode(payload, part.get_content_charset())
        return ""
    payload = msg.get_payload(decode=True)
    if payload is None:
        return msg.get_payload() or ""
    return _decode(payload, msg.get_content_charset())


# Gnus appends a comment: "<id> (X's message of "Thu, 13 Feb 2025 ...")".
_MSGID = re.compile(r"<([^<>\s]+)>")


def _first_msgid(value):
    """The first <msgid> in a header, or its first word if it has no <>."""
    m = _MSGID.search(value or "")
    if m:
        return m.group(1)
    words = (value or "").split()
    return words[0] if words else None


def _split_mboxrd(raw):
    """Yield per-message byte blobs, undoing mboxrd '>From ' escaping."""
    msgs = []
    cur = None
    for line in raw.split(b"\n"):
        if line.startswith(b"From ") and (cur is None or cur and cur[-1] == b""):
            if cur:
                msgs.append(cur)
            cur = []
            continue
        if cur is None:
            continue
        if re.match(rb"^>+From ", line):
            line = line[1:]
        cur.append(line)
    if cur:
        msgs.append(cur)
    return [b"\n".join(m) for m in msgs]


def thread(message_id, max_messages=10000, include_diffs=False,
           max_chars=2000, quote_context=3, inbox=None, budget=None):
    """Fetch the whole thread containing message_id, cleaned and ordered by date.

    `budget` caps the characters of all bodies and headers together.  The
    per-message caps don't bound the total: 100 messages of a PULL thread are
    90k characters at the default max_chars and 212k with max_chars=0, and a
    reply that large is rejected outright by the agent reading it.
    """
    msgid = norm_msgid(message_id)
    if not msgid:
        raise ValueError("message_id is required")
    raw = _run_lei("m:%s" % msgid, "mboxrd", threads=True, inbox=inbox)
    if not raw.strip():
        return []

    out = []
    for blob in _split_mboxrd(raw):
        if not blob.strip():
            continue
        msg = message_from_bytes(blob)
        body, truncated = clean_body(
            _text_part(msg), include_diffs=include_diffs, max_chars=max_chars,
            quote_context=quote_context,
        )
        mid = (_hdr(msg, "Message-ID") or "").strip("<> ")
        sort_key, iso = _parse_date(_hdr(msg, "Date"))
        rec = {
            "message_id": mid,
            "subject": _hdr(msg, "Subject"),
            "from": _hdr(msg, "From"),
            "date": iso,
            "in_reply_to": _first_msgid(_hdr(msg, "In-Reply-To")),
            "url": url_for(mid) if mid else None,
            "body": body,
        }
        if truncated:
            rec["body_truncated"] = True
        rec = {k: v for k, v in rec.items() if v is not None}
        out.append((sort_key, rec))

    # lei emits in index order; thread reading order is chronological. Sorting on
    # the raw Date: string would be lexicographic nonsense, hence the parse above.
    out.sort(key=lambda pair: pair[0])
    out = [rec for _, rec in out]

    # max_messages counts messages, so max_messages=1 must still return one,
    # and the requested message is always one of them: if it alone is over
    # budget, its body is cut to fit rather than the reply being empty.  A
    # message that doesn't fit is skipped, not the end of the walk: one
    # 100k-character reply would otherwise crowd out every short one after it.
    keep = []
    used = 0
    over_budget = False
    for i in _focus(out, msgid):
        if max_messages and len(keep) >= max_messages:
            break
        cost = _cost(out[i])
        if budget and used + cost > budget:
            over_budget = True
            if keep:
                continue
            rec = out[i]
            room = max(budget - (cost - len(rec["body"])), 0)
            rec["body"] = rec["body"][:room].rstrip()
            rec["body_truncated"] = "budget"
            cost = _cost(rec)
        keep.append(i)
        used += cost
    omitted = len(out) - len(keep)
    out = [out[i] for i in sorted(keep)]
    if omitted:
        # The note is extra, on top of max_messages.
        if over_budget:
            how = ("the reply is capped at %d characters, so lower max_chars "
                   "or quote_context, or drop include_diffs" % budget)
        else:
            how = "raise max_messages"
        out.append(
            {
                "note": "thread truncated: %d further message(s) omitted, "
                "keeping those nearest the requested one; %s, or read the "
                "thread at its lore URL" % (omitted, how)
            }
        )
    return out


def _cost(rec):
    """Characters a message takes in a reply: its fields plus framing."""
    return sum(len(str(v)) for v in rec.values()) + 200


def _focus(msgs, msgid):
    """Indices of a chronological thread's messages, most relevant first.

    Oldest first would do for a thread's root, but find_commit_discussion
    hands out patches sitting deep in 30-message series, and the head of
    such a thread is the cover letter and other patches' review.  So rank,
    in this order of priority, the requested message, the replies under it,
    the messages it answers, then everything else oldest first.
    """
    ids = [m.get("message_id") for m in msgs]
    if msgid not in ids:
        return list(range(len(msgs)))
    parent = {m.get("message_id"): m.get("in_reply_to") for m in msgs}

    # Walk the reply tree rather than trust dates: a sender's clock can put
    # a reply before the message it answers.
    children = {}
    for mid in ids:
        children.setdefault(parent[mid], []).append(mid)
    rank = {msgid: 0}
    todo = [msgid]
    while todo:
        for child in children.get(todo.pop(), []):
            if child not in rank:
                rank[child] = 1
                todo.append(child)
    up = parent.get(msgid)
    while up in parent and up not in rank:
        rank[up] = 2
        up = parent[up]

    return sorted(range(len(msgs)), key=lambda i: (rank.get(ids[i], 3), i))


# --- commit -> list discussion ----------------------------------------------

# The QEMU checkout: scripts live in <checkout>/lore/scripts.
REPO = os.environ.get("EMAIL_LORE_REPO") or os.path.dirname(ROOT)


class GitError(RuntimeError):
    pass


def _git(repo, *args):
    p = run(["git", "-C", repo or REPO] + list(args), GitError)
    if p.returncode != 0:
        err = p.stderr.decode("utf-8", "replace").strip()
        raise GitError("git %s failed: %s" % (args[0], err or "no stderr"))
    return p.stdout


# The posted diff was produced by its author's git, so its patch-id depends on
# their diff algorithm; measured over qemu master, all three are in use and
# each alone misses commits the others find.
_DIFF_ALGORITHMS = ("myers", "histogram", "patience")


def commit_info(rev, repo=None):
    """Resolve rev to (sha, subject, patch_ids).

    patch_ids is None for a merge, which has no single diff to match on, and
    empty for a commit with no diff at all.

    One patch-id per diff algorithm, deduplicated.  The diff comes from
    diff-tree rather than show: plumbing ignores the local user's diff.noprefix,
    diff.context, textconv and friends, any of which changes the patch-id and
    turns an exact lookup into a silent miss.
    """
    sha = _git(repo, "rev-parse", "--verify", "%s^{commit}" % rev)
    sha = sha.decode().strip()
    out = _git(repo, "log", "-1", "--format=%s%n%P", sha).decode("utf-8", "replace")
    subject, _, parents = out.partition("\n")
    subject = subject.strip()

    # A merge has no single diff, so no patch-id to match on.
    if len(parents.split()) > 1:
        return sha, subject, None

    patch_ids = []
    for algo in _DIFF_ALGORITHMS:
        diff = _git(repo, "diff-tree", "-p", "-M", "--root", "--no-color",
                    "--no-ext-diff", "--no-textconv",
                    "--diff-algorithm=%s" % algo, "--format=", sha)
        if not diff.strip():
            break
        p = run(["git", "patch-id", "--stable"], GitError, input=diff)
        if p.returncode == 0 and p.stdout.split():
            pid = p.stdout.split()[0].decode()
            if pid not in patch_ids:
                patch_ids.append(pid)
    return sha, subject, patch_ids


# Xapian operators would be parsed, not searched, if a subject contained them.
_XAPIAN_OPS = {"and", "or", "not", "xor", "near", "adj"}
_TERM = re.compile(r"[A-Za-z0-9_.]+")
# Mail subjects carry prefixes a commit subject never has: "Re:", "[PATCH v3 2/5]".
_SUBJ_PREFIX = re.compile(r"^\s*((re|aw|fwd?)\s*:|\[[^\]]*\])\s*", re.I)


def subject_terms(subject):
    """Commit subject -> term list for s:(...), every word of it.

    Mail subjects carry the same words with "[PATCH v3 2/5]" in front, so
    requiring all of them -- the space in s:(a b) is an AND -- is selective,
    and the subsystem prefix and identifiers are the most selective words of
    all.  Over 200 commits of 2025-2026 this finds the posting for 195;
    dropping the prefix and anything with a digit or underscore found 114.
    """
    seen = []
    for t in _TERM.findall(_norm_subject(subject)):
        if t not in _XAPIAN_OPS and t not in seen:
            seen.append(t)
    return seen


def _norm_subject(text):
    out = _SUBJ_PREFIX.sub("", text or "")
    while True:
        stripped = _SUBJ_PREFIX.sub("", out)
        if stripped == out:
            break
        out = stripped
    return " ".join(_TERM.findall(out.lower()))


def _subject_matches(mail_subject, commit_subject):
    """True if the mail is about this commit, prefixes and spacing aside."""
    want = _norm_subject(commit_subject)
    got = _norm_subject(mail_subject)
    # Whole words only: "fix foo" must not match "fix foobar".
    return bool(want) and (" %s " % want) in (" %s " % got)


def best_review_hit(hits):
    """Pick the hit whose thread most likely holds the actual review.

    The *oldest* posting of a diff is the one that got reviewed; later mail
    carrying the identical diff is resends, maintainer pull requests and stable
    backports, whose threads are either silent or 50 unrelated patches deep.
    Prefer a patch-id match, since a subject match may be a near-miss.
    """
    if not hits:
        return None
    exact = [h for h in hits if "patch-id" in (h.get("matched_by") or [])]
    # An undated hit would sort as the oldest of all; rank it last instead.
    return min(exact or hits, key=lambda h: (not _posted(h), _posted(h)))


def _posted(hit):
    """When a hit reached the list, as a sortable string ("" if unknown)."""
    return hit.get("received") or hit.get("date") or ""


def describe_patch_ids(patch_ids):
    if patch_ids is None:
        return "(none: merge commit)"
    return " ".join(patch_ids) or "(none: empty commit)"


def dig(rev, repo=None, limit=10000, inbox=None, subject_fallback=True):
    """Find the list discussion of a merged commit.

    Primary lookup is the git patch-id, which public-inbox indexes as
    `patchid:`.  It is exact: no false positives, immune to a maintainer
    rewording the subject, and it finds every posted revision of the same diff.
    Measured over 40 consecutive qemu master commits it found all 37 that were
    ever posted; the 3 it missed -- version bumps and "Open the tree" -- never
    went to the list at all, so there was nothing to find.

    The subject lookup is a *fallback*, used only when patch-id comes up empty,
    which happens when the diff changed between posting and merge (a rebase,
    a maintainer fixup, a squash).  Its words also match other subjects that
    contain them, so its hits are post-filtered locally against the real
    subject text.

    Merge commits get neither lookup.  Their subject is "Merge tag 'pull-x' of
    <url> into staging", which shares no words with the "[PULL 00/NN] x queue"
    cover letter, so the subject search could only ever return noise.

    Note that `patchid:` only ever matches mail carrying the diff, never the
    replies: feed a returned message_id to thread() to read the review.
    """
    sha, subject, patch_ids = commit_info(rev, repo)

    found = {}

    def collect(query, how, fetch, check_subject=False):
        for h in search(query, limit=fetch, inbox=inbox):
            mid = h.get("message_id")
            if not mid:
                continue
            if check_subject and not _subject_matches(h.get("subject"), subject):
                continue
            cur = found.setdefault(mid, dict(h, matched_by=[]))
            if how not in cur["matched_by"]:
                cur["matched_by"].append(how)

    # Fetch every match, not `limit` of them: the oldest posting is the one
    # best_review_hit wants, and a series reposted twenty times has twenty.
    if patch_ids:
        collect(" OR ".join("patchid:%s" % pid for pid in patch_ids),
                "patch-id", 10000)

    if not found and subject_fallback and patch_ids is not None:
        terms = subject_terms(subject)
        if terms:
            # Over-fetch: the post-filter rejects some hits, so `limit` has to
            # apply to what survives, not to what the query returned.
            collect("s:(%s)" % " ".join(terms), "subject",
                    max(limit * 3, 20), check_subject=True)

    # Newest first: the last posting is normally the version that got merged,
    # and relevance ranking is meaningless for an exact patch-id term.
    hits = sorted(found.values(), key=_posted, reverse=True)
    # Pick before truncating, and keep the pick in the list: cutting to the
    # newest `limit` drops exactly the early postings that carry the review.
    best = best_review_hit(hits)
    total = len(hits)
    best_appended = False
    if limit and total > limit:
        hits = hits[:limit]
        if best not in hits:
            hits[-1] = best
            best_appended = True
    return {
        "commit": sha,
        "subject": subject,
        "patch_ids": patch_ids,
        "hits": hits,
        "total_hits": total,
        "best": best,
        "best_appended": best_appended,
    }


def describe_hit_count(res):
    """How many of dig()'s hits are shown, and in what order."""
    shown, total = len(res["hits"]), res["total_hits"]
    if not res.get("best_appended"):
        if total > shown:
            return "%d of %d hit(s), newest first" % (shown, total)
        return "%d hit(s), newest first" % shown
    if shown == 1:
        return "the oldest posting of %d hit(s)" % total
    return ("%d of %d hit(s), newest first, plus the oldest posting last"
            % (shown - 1, total))
