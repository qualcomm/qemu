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

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)

# The scripts live in <checkout>/lore; the mirror is tens of GB of cloned git
# epochs, so it lives under <checkout>/.cache/lore, which QEMU's .gitignore
# already covers via /.cache/ -- no ignore rule of our own to maintain.
DATA = os.environ.get("EMAIL_LORE_DIR") or os.path.join(
    os.path.dirname(ROOT), ".cache", "lore")

INBOX = os.environ.get("EMAIL_INBOX") or os.path.join(DATA, "qemu-devel")
LORE_BASE = os.environ.get("EMAIL_LORE_BASE", "https://lore.kernel.org/qemu-devel")

# lei exits 0 with no output on an empty result set, so these are real errors.
class LeiError(RuntimeError):
    pass


def url_for(msgid):
    return "%s/%s/" % (LORE_BASE.rstrip("/"), (msgid or "").strip("<> "))


def _run_lei(query, fmt, limit=None, threads=False, inbox=None, extra=None):
    cmd = ["lei", "q", "--only", inbox or INBOX, "-f", fmt]
    if limit is not None:
        cmd += ["-n", str(limit)]
    if threads:
        cmd.append("--threads")
    if extra:
        cmd += list(extra)
    cmd += ["--", query]
    p = subprocess.run(cmd, capture_output=True)
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


def search(query, limit=20, threads=False, inbox=None):
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
            "url": url_for(msgid) if msgid else None,
        }
        if rec.get("pct") is not None:
            hit["relevance"] = rec["pct"]
        hits.append({k: v for k, v in hit.items() if v not in (None, [], "")})
    return hits


# --- body cleanup -----------------------------------------------------------

_QUOTE = re.compile(r"^\s*(>|\|)")
_ATTRIB = re.compile(
    r"^\s*(On .{0,120}(wrote|said):|.{0,80} writes:|Quoting .{0,80}:)\s*$"
)
# Locale-independent fallback: attribution lines end in ':' and name an address.
# Catches e.g. "Am Mi., 7. Okt. 2026 um 15:27 Uhr schrieb Peter Xu <x@y>:".
_ATTRIB_ADDR = re.compile(r"^\s*\S.{0,200}<[^<>@\s]+@[^<>@\s]+>\s*:\s*$")
_DIFF_START = re.compile(r"^(diff --git |index [0-9a-f]{7,}\.\.|--- a/|\+\+\+ b/|@@ )")
_DIFF_FILE = re.compile(r"^diff --git a/(\S+) b/(\S+)")
_HUNK = re.compile(r"^@@ ")
_SCISSORS = re.compile(r"^(-- |---)\s*$")

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
    sig = False

    for line in lines:
        if _DIFF_START.match(line):
            in_diff = True
        if in_diff:
            m = _DIFF_FILE.match(line)
            if m:
                diff_files.append(m.group(2))
            if _HUNK.match(line):
                hunks += 1
            if include_diffs:
                body_lines.append(line)
                kinds.append("new")
            continue
        if sig:
            continue
        if _SCISSORS.match(line):
            sig = True
            continue
        quoted = bool(_QUOTE.match(line))
        attrib = bool(_ATTRIB.match(line) or _ATTRIB_ADDR.match(line))
        body_lines.append(line)
        kinds.append("quote" if quoted else "attrib" if attrib else "new")

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

    # Truncate the prose FIRST so the diff summary below always survives; it is
    # usually the most useful line in a patch mail.
    truncated = False
    if max_chars and len(body) > max_chars:
        body = body[:max_chars].rstrip()
        truncated = True

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


def _text_part(msg):
    """First text/plain part, decoded best-effort."""
    if msg.is_multipart():
        for part in msg.walk():
            if part.get_content_type() == "text/plain":
                payload = part.get_payload(decode=True)
                if payload:
                    charset = part.get_content_charset() or "utf-8"
                    return payload.decode(charset, "replace")
        return ""
    payload = msg.get_payload(decode=True)
    if payload is None:
        return msg.get_payload() or ""
    charset = msg.get_content_charset() or "utf-8"
    return payload.decode(charset, "replace")


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


def thread(message_id, max_messages=25, include_diffs=False,
           max_chars=2000, quote_context=3, inbox=None):
    """Fetch the whole thread containing message_id, cleaned and ordered by date."""
    msgid = (message_id or "").strip().strip("<>")
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
            "in_reply_to": (_hdr(msg, "In-Reply-To") or "").strip("<> ") or None,
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
    if max_messages and len(out) > max_messages:
        head = out[: max_messages - 1]
        head.append(
            {
                "note": "thread truncated: %d further message(s) omitted; "
                "raise max_messages or read the thread at its lore URL"
                % (len(out) - len(head))
            }
        )
        out = head
    return out


# --- commit -> list discussion ----------------------------------------------

# The QEMU checkout: scripts live in <checkout>/lore/scripts.
REPO = os.environ.get("EMAIL_LORE_REPO") or os.path.dirname(ROOT)


class GitError(RuntimeError):
    pass


def _git(repo, *args):
    p = subprocess.run(
        ["git", "-C", repo or REPO] + list(args), capture_output=True
    )
    if p.returncode != 0:
        err = p.stderr.decode("utf-8", "replace").strip()
        raise GitError("git %s failed: %s" % (args[0], err or "no stderr"))
    return p.stdout


def commit_info(rev, repo=None):
    """Resolve rev to (sha, subject, patch_id); patch_id is None for merges."""
    sha = _git(repo, "rev-parse", "--verify", "%s^{commit}" % rev)
    sha = sha.decode().strip()
    out = _git(repo, "log", "-1", "--format=%s%n%P", sha).decode("utf-8", "replace")
    subject, _, parents = out.partition("\n")
    subject = subject.strip()

    # A merge has no single diff, so no patch-id to match on; subject only.
    patch_id = None
    if len(parents.split()) < 2:
        diff = _git(repo, "show", "--no-color", "--no-ext-diff", "--format=", sha)
        if diff.strip():
            p = subprocess.run(
                ["git", "patch-id", "--stable"], input=diff, capture_output=True
            )
            if p.returncode == 0 and p.stdout.split():
                patch_id = p.stdout.split()[0].decode()
    return sha, subject, patch_id


# Xapian operators would be parsed, not searched, if a subject contained them.
_XAPIAN_OPS = {"and", "or", "not", "xor", "near", "adj"}
_TERM = re.compile(r"[A-Za-z0-9_.]+")
# Plain words only -- see subject_terms for why digits and underscores are out.
_WORD = re.compile(r"[A-Za-z]{3,}")
# Mail subjects carry prefixes a commit subject never has: "Re:", "[PATCH v3 2/5]".
_SUBJ_PREFIX = re.compile(r"^\s*((re|aw|fwd?)\s*:|\[[^\]]*\])\s*", re.I)


def subject_terms(subject, cap=8):
    """Commit subject -> term list for s:(...).

    Three traps, all found by measuring against the mirror rather than reading
    the docs, and all of which silently return zero hits:

    - `s:"quoted"` is not a phrase search here.  It matches loosely and happily
      returns unrelated mail, so it looks like it works until you check.
    - Explicit `AND` inside the group -- `s:(a AND b)` -- matches nothing.  A
      space is the working conjunction.
    - Only plain alphabetic words are reliable terms.  `qxl_phys2virt`,
      `phys2virt` and the `hw/display/qxl` path prefix each behave like phrases
      and poison the whole group, so the conventional `subsystem/path:` prefix
      is dropped and anything with a digit or underscore is skipped.

    Losing the identifiers hurts selectivity, which is why dig() treats this as
    a fallback behind patch-id and post-filters whatever comes back.
    """
    # Drop the conventional "subsystem/path:" prefix; its tokens aren't indexed
    # as standalone subject terms.
    prose = (subject or "").rsplit(":", 1)[-1]
    terms = [
        w.lower() for w in _WORD.findall(prose) if w.lower() not in _XAPIAN_OPS
    ]
    seen = []
    for t in terms:
        if t not in seen:
            seen.append(t)
    return seen[:cap]


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
    return bool(want) and want in got


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
    return min(exact or hits, key=lambda h: h.get("date") or "")


def dig(rev, repo=None, limit=10, inbox=None, subject_fallback=True):
    """Find the list discussion of a merged commit.

    Primary lookup is the git patch-id, which public-inbox indexes as
    `patchid:`.  It is exact: no false positives, immune to a maintainer
    rewording the subject, and it finds every posted revision of the same diff.
    Measured over 40 consecutive qemu master commits it found all 37 that were
    ever posted; the 3 it missed -- version bumps and "Open the tree" -- never
    went to the list at all, so there was nothing to find.

    The subject lookup is a *fallback*, used only when patch-id comes up empty,
    which happens when the diff changed between posting and merge (a rebase,
    a maintainer fixup, a squash) or for a merge commit, which has no patch-id.
    It is fuzzy -- see subject_terms -- so its hits are post-filtered locally
    against the real subject text.

    Note that `patchid:` only ever matches mail carrying the diff, never the
    replies: feed a returned message_id to thread() to read the review.
    """
    sha, subject, patch_id = commit_info(rev, repo)

    found = {}

    def collect(query, how, check_subject=False):
        # Over-fetch: the post-filter rejects some hits, so `limit` has to apply
        # to what survives, not to what the query returned.
        for h in search(query, limit=max(limit * 3, 20), inbox=inbox):
            mid = h.get("message_id")
            if not mid:
                continue
            if check_subject and not _subject_matches(h.get("subject"), subject):
                continue
            cur = found.setdefault(mid, dict(h, matched_by=[]))
            if how not in cur["matched_by"]:
                cur["matched_by"].append(how)

    if patch_id:
        collect("patchid:%s" % patch_id, "patch-id")

    if not found and subject_fallback:
        terms = subject_terms(subject)
        if terms:
            collect("s:(%s)" % " ".join(terms), "subject", check_subject=True)

    # Newest first: the last posting is normally the version that got merged,
    # and relevance ranking is meaningless for an exact patch-id term.
    hits = sorted(
        found.values(), key=lambda h: h.get("date") or "", reverse=True
    )
    return {
        "commit": sha,
        "subject": subject,
        "patch_id": patch_id,
        "hits": hits[:limit] if limit else hits,
    }
