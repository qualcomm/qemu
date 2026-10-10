# lore — querying qemu-devel with an AI agent

A local, offline-searchable mirror of the [qemu-devel](https://lore.kernel.org/qemu-devel/)
mailing list, plus tools that let an AI agent search it without drowning in quoted
replies.

Two directories, both at the top of the QEMU checkout:

- `lore/` — these scripts, tracked in git.
- `.cache/lore/` — the mirror itself, tens of GB of cloned git epochs. QEMU's
  `.gitignore` already ignores `/.cache/`, so there is no ignore rule of our own
  to maintain and the tracked file stays untouched.

## Why keyword search, not embeddings

Mailing-list questions from a QEMU developer are lexical and structural: a
Message-ID, a function name, an error string, "who reviewed changes to
`accel/tcg/cputlb.c`". Xapian BM25 plus thread reconstruction answers those
exactly; embedding search blurs the identifiers that carry the meaning. The agent
iterates on queries instead of relying on one-shot semantic recall. Add vectors
later only if you hit real "vibes" queries you can't express as a query string.

## Layout

```
qemu/
├── .cache/lore/qemu-devel/ public-inbox v2 mirror + Xapian index (gitignored)
│   └── git/{0,1,2,3}.git   epochs, oldest first
└── lore/
    ├── scripts/
    │   ├── lore_lib.py         shared query/cleanup logic
    │   ├── lore_progress.py    progress bar for the indexing step
    │   ├── lore-install-deps   install public-inbox, lei, b4  (needs root, run once)
    │   ├── lore-bootstrap      create the mirror / backfill epochs + index
    │   ├── lore-search         metadata search  (cheap, call freely)
    │   ├── lore-dig            merged commit -> the review that produced it
    │   ├── lore-thread         read one thread  (quotes trimmed)
    │   └── lore-update         incremental fetch + reindex; bootstraps if needed
    └── mcp/server.py           MCP stdio server wrapping the same operations
```

`.cache/lore/` holds tens of GB of cloned git epochs and is not in version
control — it is **built on demand**, so a fresh machine (or one that lost the
directory) starts from the two scripts below. Epochs 2–3 alone are roughly the
recent years of the list, ~476k messages and ~13 GB; all four epochs are ~25 GB.

Override the location with `EMAIL_LORE_DIR` (parent directory) or `EMAIL_INBOX`
(the inbox itself); every script and the MCP server honour both.

Every `lei`, `git` and `b4` call is killed after `EMAIL_LORE_TIMEOUT` seconds
(default 300), so one stuck query cannot wedge the single-threaded MCP server.

## Bootstrapping the mirror

```sh
cd lore/scripts
./lore-install-deps            # public-inbox + lei + b4, then verifies PATH
./lore-bootstrap --list        # what is present vs. what lore offers
./lore-bootstrap               # clone every missing epoch, then index
```

Epochs come from lore's own `manifest.js.gz`, so new ones (4, 5, …) are picked
up without editing the script. Already-populated epochs are skipped, each clone
lands atomically via a temp directory, and the index pass is incremental — so an
interrupted run is resumed by simply running it again.

Expect several hours and ~25 GB for the full history.

```sh
./lore-bootstrap --epochs 0,1  # just the old history, e.g. to extend a 2–3 mirror
./lore-bootstrap --dry-run     # print the clones it would do
./lore-bootstrap --no-index    # clone now, public-inbox-index later
./lore-bootstrap --force       # re-clone even populated epochs
```

An epoch directory created by `public-inbox-init` has no refs; `--list` reports
it as `placeholder` and bootstrap replaces it rather than mistaking it for a
real clone.

### Progress while indexing

Indexing the full archive is a multi-hour, silent-by-default step, so both
`lore-bootstrap` and `lore-update` run it as `public-inbox-index -v` through
`lore_progress.py`:

```
indexing [==========>                    ]  34% 352,118/1,027,448  310/s eta 0:36
```

`-v` makes public-inbox report `# <done>/<total>` on stderr at every checkpoint
(roughly per `--batch-size`, so updates are chunky, not smooth); the wrapper
turns those into one redrawn line and prints anything else — epoch counts,
warnings — above it.

The rate and ETA are measured from the **first counter seen**, not from zero, so
resuming a half-finished index doesn't claim credit for the messages indexed by
the previous run.

The bar only appears when stderr is a terminal; under cron or CI the output
passes through verbatim, carriage-return-free. Force it either way with
`LORE_PROGRESS=always` / `LORE_PROGRESS=never`.

## CLI usage

```sh
cd lore/scripts

./lore-search --syntax                              # query cheat sheet
./lore-search 'dfn:accel/tcg/cputlb.c AND rt:2024-01-01..'
./lore-search -n 5 's:(tcg plugin cputlb)'
./lore-thread 20240115103000.12345-1-someone@example.org
./lore-thread --diffs --max-chars 0 <msgid>         # full text, real hunks
./lore-thread --quote-context 0 <msgid>             # prose only, cheapest

./lore-dig                                          # why is HEAD like this?
./lore-dig --thread v8.2.0~50                       # and read the review
```

## Finding the review behind a commit

`lore-dig <rev>` is the "why is this code like this" tool: git records what
changed, the list records why it was accepted, reworked or argued over.

```
$ ./lore-dig 8efec0ef8bb
commit  8efec0ef8bbc1e75a7ebf6e325a35806ece9b39f
subject hw/display/qxl: Pass requested buffer size to qxl_phys2virt()
patchid a343c6210270396e82099d827698b59091abc7d1

1. [patch-id]  2022-12-05  Juan Quintela <quintela@redhat.com>
   [PATCH v2 06/51] hw/display/qxl: Pass requested buffer size to qxl_phys2virt()
   id:20221205095228.1314-7-quintela@redhat.com
2. [patch-id]  2022-11-28  Philippe Mathieu-Daudé <philmd@linaro.org>
   [RFC PATCH-for-7.2 v3 3/5] hw/display/qxl: Pass requested buffer size to qxl_phys2virt()
   id:20221128202741.4945-4-philmd@linaro.org
```

The primary lookup is the **git patch-id**, which public-inbox indexes as
`patchid:` (it is literally `git patch-id --stable` output). That is exact: no
false positives, immune to a maintainer rewording the subject, and it finds
every posted revision of the same diff.

The patch-id depends on how the diff was generated, and that was the
poster's git, not yours: myers, histogram and patience all show up on the list,
and each alone misses commits the others find. So `lore-dig` computes one
patch-id per algorithm (that is why a commit can list several) and looks
them all up at once with `patchid:a OR patchid:b`. The diff comes from
`git diff-tree`, which ignores local
settings like `diff.noprefix` or `diff.context` that would also change it.

Measured over 40 consecutive `master` commits, patch-id found all 37 that had
ever been posted. The 3 it missed — two version bumps and "Open 11.2 development
tree" — were applied directly by the maintainer and genuinely never went to the
list, which a direct subject search confirms. So an empty result is usually a
real answer, not a failed lookup.

A **subject search is tried only as a fallback**, for the case where the diff
changed between posting and merge (a rebase, a fixup, a squash), which changes
the patch-id. It requires every word of the subject, prefix and identifiers
included, and its hits are post-filtered locally against the real subject text
before being shown, since the same words can appear in another order or in a
longer subject. Each hit is labelled with the lookup that found it.

`patchid:` only ever matches mail that carries the diff, never the replies, so
`lore-dig --thread` (or `get_thread` on a returned id) is what actually shows
the review.

Hits are listed newest first — the latest state — but `--thread` deliberately
reads the **oldest** posting instead. That is where the review happened; later
mail carrying the identical diff is resends, maintainer pull requests and stable
backports, whose threads are either silent or fifty unrelated patches deep.

`lore-update` fetches new mail and reindexes; it's a no-op when nothing is new.
If the mirror isn't there at all it hands over to `lore-bootstrap`, so a single
command works on a fresh machine — use `--no-bootstrap` from cron, where you
want a missing mirror to page you rather than silently start a 25 GB clone.
Indexing chatters on stderr every run, so log it and let cron mail only a
failure:

```sh
# from the top of the checkout: add a daily run at 04:17 to your crontab
(crontab -l 2>/dev/null
 echo "17 4 * * * $(pwd)/lore/scripts/lore-update --no-bootstrap" \
      ">\$HOME/.cache/lore-update.log 2>&1" \
      "|| echo 'lore-update failed, see ~/.cache/lore-update.log'") | crontab -
```

A mirror whose epochs are all empty placeholders counts as missing, since
`public-inbox-fetch` has nothing to update there.

## Query syntax

Combine with `AND` / `OR` / `NOT`; quote phrases.

| Prefix | Matches | | Prefix | Matches |
|---|---|---|---|---|
| `s:` | subject | | `f:` | `From:` |
| `t:` | `To:` | | `c:` | `Cc:` |
| `b:` | body incl. quotes | | `nq:` | body, quoted text **excluded** |
| `q:` | quoted text only | | `m:` | exact Message-ID |
| `dfn:` | diff touches this file | | `dfhh:` | diff hunk header |
| `dfa:` | line **added** by a diff | | `dfb:` | line **removed** |
| `patchid:` | git patch-id | | `l:` | list address |
| `rt:A..B` | received date range | | `dt:A..B` | `Date:` header range |

Dates are `YYYY-MM-DD`; open-ended ranges work (`rt:2024-06-01..`).

Idioms worth remembering:

```
dfn:hw/arm/virt.c AND rt:2024-01-01..        recent patches to a file
dfa:qemu_log_mask                            who added a call to it
s:(reduce vdso alignment)                    all of these words in one subject
s:"vdso alignment"                           this exact phrase in a subject
f:you@example.org AND dfn:hw/arm/virt.c      your patches to a file
nq:regression                                discussion, ignoring quoted replies
```

A per-patch reply does **not** inherit the cover letter's words: it carries
the subject of the patch it answers. So `s:(single binary)` finds cover letters
and misses the review on individual patches; use `get_thread`/`lore-thread` on
the cover letter for the real status.

### lei and multi-word queries

The scripts hand lei the query on stdin (`lei q --stdin`), not as an argument.
`lei q` expects its query split by the shell, one word per argument, and
quotes any argument containing a space as a phrase: passed as one argument,
`s:(a b)` becomes `s:"(a b)"`, and `s:a AND f:b` a single phrase that matches
nothing. Earlier versions of these scripts did exactly that, and documented
the resulting zero-hit queries as Xapian "traps". If you call lei yourself,
use `--stdin`, or split the query into separate arguments.

## MCP server

Register it project-locally (stored in `~/.claude.json`, not in the repo),
from the top of the checkout:

```sh
claude mcp add qemu-devel-lore -s local -- "$(pwd)/lore/mcp/server.py"
```

Four tools:

- **`search_qemu_devel`** — metadata-only hits; the entry point. The query
  cheat sheet is in the tool description, so the agent writes good queries
  without being told.
- **`find_commit_discussion`** — a merged commit to the mail that produced it,
  via `lore-dig`'s patch-id lookup. The tool for "why is this code like this",
  "was this approach objected to", "who reviewed this".
- **`get_thread`** — one thread, chronological, signatures stripped, quotes
  trimmed to `quote_context` lines around each reply, diffs collapsed to a
  changed-file summary unless `include_diffs` is set.
- **`get_patch_series`** — runs `b4 am` against the local mirror to produce a
  `git am`-able mbox, and returns **the path**, not the diff. Unless told
  otherwise it writes to `.cache/lore/series/<msgid>/`, reused on the next
  call for the same series rather than leaving a temp dir behind. Search and thread
  reading are fully offline; this tool also asks lore for review trailers when
  the network is up (and degrades gracefully when it isn't), so it is the one
  tool that isn't airgapped.

Pure stdlib JSON-RPC over stdio — no pip, no venv.

## Design notes

Everything here optimises for the agent's context budget:

- **Search returns no bodies.** Finding is cheap, reading is explicit.
- **Quoted text is trimmed, not deleted.** Dropping quotes wholesale is a false
  economy: `Right, it looks like QXL device stride is in bytes.` costs almost
  nothing to store and everything to interpret, and the agent has to go fetch
  the parent message to recover what was being answered. So `quote_context`
  (default 3) keeps a few quoted lines hugging each block of new text, plus the
  `On <date>, <name> wrote:` attribution for free, and marks what it dropped
  with `[...]`. Measured over three review-heavy threads that costs **+8%** of
  body text and gives every single reply its anchor; `quote_context=0` restores
  the old strip-everything behaviour.
- **Diffs are summarised** to a changed-file list and hunk count by default.
- **Large payloads become file paths.** `get_patch_series` hands back an mbox on
  disk instead of tens of thousands of tokens of diff.
- **Message-IDs are always returned**, so answers can cite
  `https://lore.kernel.org/qemu-devel/<msgid>/` — a verifiable link beats a
  summary you have to trust.
- **Result counts are capped.** The MCP tools default to 30 search hits, 10
  commit hits and 25 messages per thread, and cap them at 200/100/100; agents
  will cheerfully ask for 10000, and Claude Code rejects a tool result much
  over 25k tokens anyway.
- **So is the size of a thread.** Per-message caps don't bound the total: 100
  messages of a 153-patch PULL thread are ~90k characters at the default
  `max_chars`, over 200k with `max_chars=0`. `get_thread` caps the whole reply
  at 60k characters, filling it in the same order as `max_messages` does —
  the requested message, its replies, its parents, then the rest — and skips
  a message that doesn't fit rather than stopping at it.

## Requirements

`public-inbox` (1.9), `lei`, `b4`, Python 3 — `scripts/lore-install-deps`
installs them.

Two packages, not one: Ubuntu's `public-inbox` ships the server-side tools
(`public-inbox-init`, `-index`, `-fetch`) but **not** the `lei` binary, which is
packaged separately as `lei`. Install only `public-inbox` and the MCP server
fails at runtime with `No such file or directory: 'lei'`.
