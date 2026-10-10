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
import subprocess
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
  s:(reduce vdso alignment)                    several words in the subject
  f:alex.bennee AND rt:2025-01-01..            one person's recent mail

Query traps, measured against this mirror. All of them fail by returning ZERO
hits or junk, never an error, so an empty result often means a malformed query
rather than a topic nobody discussed:
  - ANDing two term prefixes does NOT work: `s:foo AND s:bar`,
    `f:alice AND dfn:hw/arm/virt.c`, even `dfa:x AND NOT s:PULL` all match
    nothing. Use ONE term prefix per query and narrow by reading the hits.
    A term prefix AND a date range (rt:/dt:) is the one combination that works.
  - To require several words in one field, group them under a single prefix with
    spaces: `s:(reduce vdso alignment)`. Do NOT write AND inside the group.
  - `s:"quoted text"` is not a phrase match; it matches loosely and returns
    unrelated mail. Use the s:(...) group form instead.
  - Only plain words are reliable terms. Identifiers (`qxl_phys2virt`) and path
    prefixes (`hw/display/qxl`) act like phrases and will zero out a group;
    search those with dfa:/dfn: instead of s:.
  - nq: is less reliable than b:; if an nq: query is empty, retry with b:.
  - dfn: misses files ADDED by the patch, since there is no a/ side.
If a compound query comes back empty, decompose it into single bare terms before
concluding the discussion never happened.
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
                    "description": "max hits, 1-100 (default 20)",
                    "default": 20,
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
                    "description": "max hits, 1-100 (default 10)",
                    "default": 10,
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
            "hunks). Use this after search_qemu_devel or "
            "find_commit_discussion to see what was actually decided."
        ),
        "inputSchema": {
            "type": "object",
            "properties": {
                "message_id": {
                    "type": "string",
                    "description": (
                        "Message-ID of any message in the thread, with or "
                        "without <>"
                    ),
                },
                "max_messages": {
                    "type": "integer",
                    "description": "max messages to return, 1-100 (default 25)",
                    "default": 25,
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
                        "series"
                    ),
                },
                "outdir": {
                    "type": "string",
                    "description": "directory for the mbox (default: a fresh temp dir)",
                },
            },
            "required": ["message_id"],
        },
    },
]


def log(msg):
    sys.stderr.write("[%s] %s\n" % (SERVER_NAME, msg))
    sys.stderr.flush()


# --- tool implementations ---------------------------------------------------


def tool_search(args):
    query = (args.get("query") or "").strip()
    if not query:
        raise ValueError("query is required")
    limit = max(1, min(int(args.get("limit") or 20), 100))
    hits = lore_lib.search(query, limit=limit, threads=bool(args.get("threads")))
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
    return "\n".join(lines)


def tool_get_thread(args):
    msgs = lore_lib.thread(
        args.get("message_id"),
        max_messages=max(1, min(int(args.get("max_messages") or 25), 100)),
        include_diffs=bool(args.get("include_diffs")),
        max_chars=max(0, int(args.get("max_chars", 2000) or 0)),
        quote_context=max(0, min(int(args.get("quote_context", 3) or 0), 20)),
    )
    if not msgs:
        return (
            "No thread found for that Message-ID. It may predate this mirror's "
            "epochs (only the recent archive is mirrored); try %s"
            % lore_lib.url_for(args.get("message_id"))
        )
    out = []
    for m in msgs:
        if "note" in m:
            out.append("-- %s" % m["note"])
            continue
        out.append(
            "%s\nFrom: %s\nDate: %s\nSubj: %s\nId:   %s\n%s\n%s"
            % (
                "=" * 72,
                m.get("from") or "?",
                m.get("date") or "?",
                m.get("subject") or "(none)",
                m.get("message_id") or "?",
                "-" * 72,
                m.get("body") or "(empty after quote stripping)",
            )
        )
    return "\n".join(out)


def tool_get_patch_series(args):
    msgid = (args.get("message_id") or "").strip().strip("<>")
    if not msgid:
        raise ValueError("message_id is required")
    outdir = args.get("outdir") or tempfile.mkdtemp(prefix="qemu-series-")
    os.makedirs(outdir, exist_ok=True)

    mbox = lore_lib._run_lei("m:%s" % msgid, "mboxrd", threads=True)
    if not mbox.strip():
        return "No thread found for that Message-ID in the local mirror."

    # b4 am reconstructs the series (newest revision, correct order, trailers
    # collected from the review thread) from the mbox we hand it on stdin.
    p = subprocess.run(
        ["b4", "am", "-m", "-", "-o", outdir],
        input=mbox,
        capture_output=True,
    )
    stderr = p.stderr.decode("utf-8", "replace").strip()
    produced = sorted(
        os.path.join(outdir, f)
        for f in os.listdir(outdir)
        if f.endswith((".mbx", ".patch"))
    )
    if not produced:
        return "b4 produced no series (exit %d).\n%s" % (p.returncode, stderr[:1500])

    report = ["Series written to:"]
    for path in produced:
        report.append("  %s  (%d bytes)" % (path, os.path.getsize(path)))
    report.append("\nApply with: git am %s" % produced[0])
    if stderr:
        report.append("\nb4 notes:\n%s" % stderr[:1500])
    return "\n".join(report)


def tool_find_commit_discussion(args):
    res = lore_lib.dig(
        args.get("rev") or "HEAD",
        repo=args.get("repo"),
        limit=max(1, min(int(args.get("limit") or 10), 100)),
    )
    head = "commit  %s\nsubject %s\npatchid %s\n" % (
        res["commit"],
        res["subject"],
        res["patch_id"] or "(none: merge commit)",
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
    best = lore_lib.best_review_hit(res["hits"])
    lines.append(
        "\n%d hit(s), newest first. patchid: matches only mail carrying the "
        "diff, never the replies, so call get_thread to read the review.\n"
        "For the review itself start with the OLDEST posting -- id:%s -- since "
        "later mail with the same diff is resends, pull requests and stable "
        "backports, whose threads are silent or dozens of unrelated patches "
        "deep." % (len(res["hits"]), best.get("message_id") if best else "?")
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
    for line in sys.stdin:
        line = line.strip()
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
