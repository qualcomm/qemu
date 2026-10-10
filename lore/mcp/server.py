#!/usr/bin/env python3
#
# SPDX-License-Identifier: GPL-2.0-or-later
#
"""MCP stdio server exposing the local qemu-devel mirror to an AI agent.

Deliberately stdlib-only JSON-RPC: no pip install, no venv, nothing to keep in
sync with the rest of the system.  Four tools:

  search_qemu_devel        cheap metadata search (the entry point)
  find_commit_discussion   merged commit -> the review that produced it
  get_thread               full discussion for one Message-ID, quotes trimmed
  get_patch_series         turn a series into a git-am-able mbox on disk

stdout carries protocol frames only; all logging goes to stderr.
"""

import json
import os
import re
import sys
import tempfile

sys.path.insert(
    0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "scripts")
)

import lore_lib  # noqa: E402

SERVER_NAME = "qemu-devel-lore"
SERVER_VERSION = "0.1.0"
DEFAULT_PROTOCOL = "2024-11-05"
SUPPORTED_PROTOCOLS = {"2024-11-05", "2025-03-26", "2025-06-18"}

# Claude Code rejects tool output past ~25k tokens, and a reply it rejects is
# worth less than a short one.  Defaults fit comfortably; the caps stop an
# agent that asks for "all of them" from blowing its own context.
SEARCH_DEFAULT, SEARCH_MAX = 30, 200
DIG_DEFAULT, DIG_MAX = 10, 100
THREAD_DEFAULT, THREAD_MAX = 25, 100
# The caps above are per message, and 100 messages of a PULL thread still
# come to ~90k characters.  Mail runs ~3.5 characters a token, diffs fewer,
# so bound the whole get_thread reply well under the 25k-token limit.
THREAD_BUDGET = 60000

SEARCH_DESC = """\
Search the qemu-devel mailing list archive (local mirror, offline, fast).
Returns metadata only - no message bodies - so it is cheap to call repeatedly.
Follow up with get_thread on a returned message_id to read the discussion.

Query syntax (public-inbox/Xapian). Combine terms with AND / OR / NOT and
quote phrases. Prefixes:
  s:TEXT   subject            f:ADDR   From:          t:ADDR  To:   c:ADDR  Cc:
  b:TEXT   body incl. quotes  nq:TEXT  body EXCLUDING quoted text
  q:TEXT   quoted text only   m:MSGID  exact Message-ID
  dfn:PATH diff touches file  dfa:TEXT line ADDED by diff
  dfb:TEXT line REMOVED       dfhh:TEXT diff hunk header
  rt:A..B  received date      dt:A..B  Date: header   (dates are YYYY-MM-DD)

Prefer nq: over b: for discussion (skips quoted replies), and dfn:/dfa: to
find patches by what code they touch. Examples:
  dfn:accel/tcg/cputlb.c AND rt:2024-01-01..   patches to a file, since a date
  dfa:qemu_log_mask                            who added a call to it
  s:(reduce vdso alignment)                    all of these words in the subject
  s:"vdso alignment"                           this exact phrase in the subject
  f:alex.bennee AND dfn:linux-user/elfload.c   one person's patches to a file

A reply to one patch of a series carries that patch's subject, not the cover
letter's, so s: on the series title finds the cover letter and misses most of
the review: get_thread the cover letter instead.
"""

TOOLS = [
    {
        "name": "search_qemu_devel",
        "description": SEARCH_DESC,
        "inputSchema": {
            "type": "object",
            "properties": {
                "query": {
                    "type": "string",
                    "description": (
                        "public-inbox search query, e.g. "
                        "'dfn:hw/arm/virt.c AND rt:2024-01-01..'"
                    ),
                },
                "limit": {
                    "type": "integer",
                    "description": "max hits, 1-%d (default %d)"
                    % (SEARCH_MAX, SEARCH_DEFAULT),
                    "default": SEARCH_DEFAULT,
                },
                "threads": {
                    "type": "boolean",
                    "description": (
                        "expand each hit to its whole thread (still metadata only)"
                    ),
                    "default": False,
                },
            },
            "required": ["query"],
        },
    },
    {
        "name": "find_commit_discussion",
        "description": (
            "Given a merged QEMU commit, find the qemu-devel mail that produced "
            "it: every posted revision of the patch, and from there its review. "
            "Use this to answer 'why is this code like this', 'was this "
            "approach objected to', 'who reviewed this' -- git records what "
            "changed, the list records why. Matching is by git patch-id, which "
            "is exact and survives the subject being reworded, with a fuzzy "
            "subject fallback for commits whose diff changed between posting "
            "and merge. Returns metadata only; feed a returned message_id to "
            "get_thread to read the discussion. A commit with no hits was "
            "probably never posted (version bumps, pull merges)."
        ),
        "inputSchema": {
            "type": "object",
            "properties": {
                "rev": {
                    "type": "string",
                    "description": "git revision: SHA, tag, HEAD~3, etc (default HEAD)",
                    "default": "HEAD",
                },
                "limit": {
                    "type": "integer",
                    "description": "max hits, 1-%d (default %d)"
                    % (DIG_MAX, DIG_DEFAULT),
                    "default": DIG_DEFAULT,
                },
                "repo": {
                    "type": "string",
                    "description": (
                        "git repository to resolve rev in "
                        "(default: the QEMU checkout)"
                    ),
                },
            },
        },
    },
    {
        "name": "get_thread",
        "description": (
            "Read a full qemu-devel thread by Message-ID, in chronological order. "
            "Signatures are stripped, quoted text is trimmed to a few lines "
            "around each reply so the claim being answered stays visible, and "
            "diffs are summarised by default (set include_diffs for the real "
            "hunks). The whole reply is capped at %d characters: a long "
            "thread keeps the requested message, its replies and its "
            "parents first, and says how many messages it left out. Use "
            "this after search_qemu_devel or find_commit_discussion to see "
            "what was actually decided." % THREAD_BUDGET
        ),
        "inputSchema": {
            "type": "object",
            "properties": {
                "message_id": {
                    "type": "string",
                    "description": (
                        "Message-ID of any message in the thread; <>, an "
                        "id: prefix or a lore URL are fine"
                    ),
                },
                "max_messages": {
                    "type": "integer",
                    "description": "max messages to return, 1-%d (default %d)"
                    % (THREAD_MAX, THREAD_DEFAULT),
                    "default": THREAD_DEFAULT,
                },
                "include_diffs": {
                    "type": "boolean",
                    "description": (
                        "include diff hunks verbatim instead of a changed-file "
                        "summary"
                    ),
                    "default": False,
                },
                "max_chars": {
                    "type": "integer",
                    "description": (
                        "per-message body truncation, 0 for unlimited "
                        "(default 2000)"
                    ),
                    "default": 2000,
                },
                "quote_context": {
                    "type": "integer",
                    "description": (
                        "quoted lines to keep around each block of new text "
                        "(default 3). 0 strips quotes entirely: cheapest, but "
                        "short replies like 'Yes, that works' lose the point "
                        "they answer. Raise it when a reply reads ambiguously."
                    ),
                    "default": 3,
                },
            },
            "required": ["message_id"],
        },
    },
    {
        "name": "get_patch_series",
        "description": (
            "Assemble the complete patch series containing a Message-ID into a "
            "git-am-able mbox using b4. The thread is read from the local mirror; "
            "b4 additionally queries lore for review trailers when the network is "
            "reachable, and degrades gracefully when it is not. Returns the file "
            "path plus a per-patch summary rather than the diff text, so it is "
            "safe to call on large series: apply it with "
            "'git am <path>' or inspect it with normal tools."
        ),
        "inputSchema": {
            "type": "object",
            "properties": {
                "message_id": {
                    "type": "string",
                    "description": (
                        "Message-ID of any patch or the cover letter in the "
                        "series; <>, an id: prefix or a lore URL are fine"
                    ),
                },
                "outdir": {
                    "type": "string",
                    "description": (
                        "directory for the mbox (default: one per series "
                        "under the mirror's .cache/lore/series)"
                    ),
                },
            },
            "required": ["message_id"],
        },
    },
]


def int_arg(args, name, default, lo, hi=None):
    """args[name] clamped to [lo, hi]; only absent or null means default.

    `args.get(name) or default` would turn an explicit 0 into the default.
    """
    val = args.get(name)
    val = default if val is None else int(val)
    val = max(lo, val)
    return val if hi is None else min(val, hi)


def bool_arg(args, name):
    """args[name] as a boolean, reading the string "false" as false.

    Clients that stringify arguments send "false", which bool() calls true.
    """
    val = args.get(name)
    if isinstance(val, str):
        val = val.strip().lower()
        if val in ("", "false", "0", "no", "off"):
            return False
        if val in ("true", "1", "yes", "on"):
            return True
        raise ValueError("%s must be a boolean, not %r" % (name, args[name]))
    return bool(val)


def log(msg):
    sys.stderr.write("[%s] %s\n" % (SERVER_NAME, msg))
    sys.stderr.flush()


# --- tool implementations ---------------------------------------------------


def tool_search(args):
    query = (args.get("query") or "").strip()
    if not query:
        raise ValueError("query is required")
    limit = int_arg(args, "limit", SEARCH_DEFAULT, 1, SEARCH_MAX)
    threads = bool_arg(args, "threads")
    hits = lore_lib.search(query, limit=limit, threads=threads)
    if not hits:
        return (
            "No matches for: %s\n\n"
            "Try broadening: fewer AND terms, or b: instead of nq:." % query
        )
    lines = []
    for i, h in enumerate(hits, 1):
        pct = "[%s%%] " % h["relevance"] if "relevance" in h else ""
        lines.append(
            "%d. %s%s  %s\n   %s\n   id:%s"
            % (
                i,
                pct,
                (h.get("date") or "")[:10],
                ", ".join(h.get("from") or []) or "?",
                h.get("subject") or "(no subject)",
                h.get("message_id") or "?",
            )
        )
    lines.append(
        "\n%d hit(s). Use get_thread on an id to read the discussion." % len(hits)
    )
    if threads and len(hits) >= limit:
        lines.append(
            "Thread expansion stopped at limit=%d; raise it, or search without "
            "threads and get_thread the hit you want." % limit
        )
    return "\n".join(lines)


TRUNCATED = {
    True: "\n[... body truncated; raise max_chars, 0 = no limit ...]",
    "budget": "\n[... body truncated to fit the reply; lower quote_context "
    "or drop include_diffs ...]",
}


def tool_get_thread(args):
    msgs = lore_lib.thread(
        args.get("message_id"),
        max_messages=int_arg(args, "max_messages", THREAD_DEFAULT, 1, THREAD_MAX),
        include_diffs=bool_arg(args, "include_diffs"),
        max_chars=int_arg(args, "max_chars", 2000, 0),
        quote_context=int_arg(args, "quote_context", 3, 0, 20),
        budget=THREAD_BUDGET,
    )
    if not msgs:
        return (
            "No thread found for that Message-ID. It may predate the epochs "
            "this mirror holds, or postdate its last update; try %s"
            % lore_lib.url_for(args.get("message_id"))
        )
    out = []
    for m in msgs:
        if "note" in m:
            out.append("-- %s" % m["note"])
            continue
        out.append(
            "%s\nFrom: %s\nDate: %s\nSubj: %s\nId:   %s\n%s\n%s%s"
            % (
                "=" * 72,
                m.get("from") or "?",
                m.get("date") or "?",
                m.get("subject") or "(none)",
                m.get("message_id") or "?",
                "-" * 72,
                m.get("body") or "(empty after quote stripping)",
                TRUNCATED.get(m.get("body_truncated"), ""),
            )
        )
    return "\n".join(out)


ANSI = re.compile(r"\x1b\[[0-9;]*m")


def _snapshot(outdir):
    return {
        f: os.stat(os.path.join(outdir, f)).st_mtime_ns for f in os.listdir(outdir)
    }


def tool_get_patch_series(args):
    msgid = lore_lib.norm_msgid(args.get("message_id"))
    if not msgid:
        raise ValueError("message_id is required")
    # By default one directory per series under the mirror's cache: a fresh
    # temp dir per call was never cleaned up, and the snapshot below makes a
    # reused directory safe.  Absolute either way, since a relative path would
    # be resolved against this server's cwd, not the agent's.
    name = re.sub(r"[^\w.@+=-]", "_", msgid).strip(".") or "_"
    outdir = os.path.abspath(
        args.get("outdir") or os.path.join(lore_lib.DATA, "series", name)
    )
    os.makedirs(outdir, exist_ok=True)
    # outdir may be reused; only what this run writes belongs in the report.
    before = _snapshot(outdir)

    mbox = lore_lib._run_lei("m:%s" % msgid, "mboxrd", threads=True)
    if not mbox.strip():
        return "No thread found for that Message-ID in the local mirror."

    # b4 am reconstructs the series (newest revision, correct order, trailers
    # collected from the review thread).  Hand it the thread as a file rather
    # than on stdin: with -m - b4 ignores the msgid and keys everything off the
    # first message in the mbox, which need not be the series asked for.
    with tempfile.TemporaryDirectory(prefix="qemu-series-mbox-") as tmp:
        path = os.path.join(tmp, "thread.mbox")
        with open(path, "wb") as fh:
            fh.write(mbox)
        p = lore_lib.run(["b4", "am", "-m", path, "-o", outdir, msgid])
    # b4 colours its attestation checkmarks even into a pipe.
    stderr = ANSI.sub("", p.stderr.decode("utf-8", "replace")).strip()
    after = _snapshot(outdir)
    produced = sorted(
        os.path.join(outdir, f)
        for f, mtime in after.items()
        if f.endswith((".mbx", ".cover", ".patch")) and before.get(f) != mtime
    )
    if not produced:
        return "b4 produced no series (exit %d).\n%s" % (p.returncode, stderr[:1500])

    report = ["Series written to:"]
    for path in produced:
        report.append("  %s  (%d bytes)" % (path, os.path.getsize(path)))
    apply = [f for f in produced if not f.endswith(".cover")]
    if apply:
        report.append("\nApply with: git am %s" % apply[0])
    if stderr:
        report.append("\nb4 notes:\n%s" % stderr[:1500])
    return "\n".join(report)


def tool_find_commit_discussion(args):
    res = lore_lib.dig(
        args.get("rev") or "HEAD",
        repo=args.get("repo"),
        limit=int_arg(args, "limit", DIG_DEFAULT, 1, DIG_MAX),
    )
    head = "commit  %s\nsubject %s\npatchid %s\n" % (
        res["commit"],
        res["subject"],
        lore_lib.describe_patch_ids(res["patch_ids"]),
    )
    if not res["hits"]:
        return head + (
            "\nNo list discussion found. Most likely this commit was never "
            "posted to qemu-devel -- version bumps, tree-opening and pull "
            "merges are applied directly. It may also predate the mirrored "
            "epochs. Check the subject with search_qemu_devel before "
            "concluding there was no review."
        )
    lines = [head]
    for i, h in enumerate(res["hits"], 1):
        lines.append(
            "%d. [%s]  %s  %s\n   %s\n   id:%s"
            % (
                i,
                ",".join(h.get("matched_by") or []),
                (h.get("date") or "")[:10],
                ", ".join(h.get("from") or []) or "?",
                h.get("subject") or "(no subject)",
                h.get("message_id") or "?",
            )
        )
    best = res["best"]
    count = lore_lib.describe_hit_count(res)
    lines.append(
        "\n%s. patchid: matches only mail carrying the "
        "diff, never the replies, so call get_thread to read the review.\n"
        "For the review itself start with the OLDEST posting -- id:%s -- since "
        "later mail with the same diff is resends, pull requests and stable "
        "backports, whose threads are silent or dozens of unrelated patches "
        "deep." % (count, best.get("message_id") if best else "?")
    )
    return "\n".join(lines)


HANDLERS = {
    "search_qemu_devel": tool_search,
    "find_commit_discussion": tool_find_commit_discussion,
    "get_thread": tool_get_thread,
    "get_patch_series": tool_get_patch_series,
}


# --- JSON-RPC plumbing ------------------------------------------------------


def send(msg):
    sys.stdout.write(json.dumps(msg) + "\n")
    sys.stdout.flush()


def reply(req_id, result):
    send({"jsonrpc": "2.0", "id": req_id, "result": result})


def reply_error(req_id, code, message):
    send({"jsonrpc": "2.0", "id": req_id, "error": {"code": code, "message": message}})


def handle(req):
    method = req.get("method")
    req_id = req.get("id")
    params = req.get("params") or {}
    is_notification = "id" not in req

    if method == "initialize":
        want = params.get("protocolVersion")
        version = want if want in SUPPORTED_PROTOCOLS else DEFAULT_PROTOCOL
        reply(
            req_id,
            {
                "protocolVersion": version,
                "capabilities": {"tools": {"listChanged": False}},
                "serverInfo": {"name": SERVER_NAME, "version": SERVER_VERSION},
            },
        )
        return

    if is_notification:
        return  # notifications/initialized, cancellations, etc.

    if method == "ping":
        reply(req_id, {})
    elif method == "tools/list":
        reply(req_id, {"tools": TOOLS})
    elif method == "tools/call":
        name = params.get("name")
        fn = HANDLERS.get(name)
        if fn is None:
            reply_error(req_id, -32602, "unknown tool: %s" % name)
            return
        try:
            text = fn(params.get("arguments") or {})
            reply(req_id, {"content": [{"type": "text", "text": text}]})
        except Exception as e:  # surface as a tool error, not a protocol error
            log("tool %s failed: %r" % (name, e))
            reply(
                req_id,
                {
                    "content": [{"type": "text", "text": "error: %s" % e}],
                    "isError": True,
                },
            )
    else:
        reply_error(req_id, -32601, "method not found: %s" % method)


def main():
    if not os.path.isdir(os.path.join(lore_lib.INBOX, "git")):
        log("warning: no mirror found at %s" % lore_lib.INBOX)
    # Bytes, not text: one undecodable byte would otherwise raise out of the
    # loop and take the server down with it.
    for line in sys.stdin.buffer:
        line = line.decode("utf-8", "replace").strip()
        if not line:
            continue
        try:
            req = json.loads(line)
        except ValueError as e:
            log("bad JSON on stdin: %s" % e)
            continue
        try:
            handle(req)
        except Exception as e:
            log("handler crashed: %r" % e)
            if isinstance(req, dict) and "id" in req:
                reply_error(req["id"], -32603, "internal error: %s" % e)


if __name__ == "__main__":
    main()
