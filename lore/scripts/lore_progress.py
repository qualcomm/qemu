#!/usr/bin/env python3
#
# SPDX-License-Identifier: GPL-2.0-or-later
#
"""Run public-inbox-index and turn its progress counters into a progress bar.

`public-inbox-index -v` reports on stderr, one line per checkpoint, through
PublicInbox::Admin::progress_prepare -- which prefixes every message with
'# '.  The counter lines look like

    # 0.git counting abc..def ... 476123
    #    12345/476123
    # all.git    12345/476123

so the running count and the total can be read straight off them.  Counter
lines become one redrawn bar; everything else is passed through untouched,
printed above the bar.

The bar is only drawn when stderr is a terminal, so cron and CI keep a
complete, carriage-return-free log.  LORE_PROGRESS=always forces it on (handy
under a pty-less test harness), =never forces it off.

Exits with the command's own status.
"""

import os
import re
import shutil
import subprocess
import sys
import time

# "# <done>/<total>", with the zero-padding public-inbox uses and an optional
# phase word in front ("all.git").  A '?' total means it could not count ahead,
# in which case we show throughput but no percentage.
COUNTER = re.compile(r"^#\s*(?:(\S+)\s+)?(\d+)/(\d+|\?)\s*$")

REDRAW_MIN_INTERVAL = 0.1   # checkpoints can arrive in bursts
RATE_MIN_SAMPLE = 2.0       # seconds before quoting a rate and an ETA
BAR_MIN_WIDTH = 10          # below this a bar says nothing; drop it


def fmt_duration(seconds):
    seconds = int(seconds)
    hours, rest = divmod(seconds, 3600)
    minutes, secs = divmod(rest, 60)
    if hours:
        return "%d:%02d:%02d" % (hours, minutes, secs)
    return "%d:%02d" % (minutes, secs)


def fmt_rate(per_second):
    if per_second >= 1000:
        return "%.1fk/s" % (per_second / 1000.0)
    return "%.0f/s" % per_second


class Bar:
    """A single terminal line, redrawn in place."""

    def __init__(self, label, stream):
        self.label = label
        self.stream = stream
        self.start = time.monotonic()
        self.last_draw = 0.0
        self.width = 0          # of what we last drew, so we can erase it
        # Rate is measured from the first counter we see, not from zero: an
        # index run that resumes at 200k/476k has not done 200k messages of
        # work on our watch, and dividing by our own elapsed time would report
        # a wild rate and a useless ETA.
        self.first_done = None
        self.first_time = None

    def clear(self):
        if self.width:
            self.stream.write("\r%s\r" % (" " * self.width))
            self.stream.flush()
            self.width = 0

    def rate(self, done, now):
        """Messages/second over our own sample, or None if not yet meaningful."""
        if self.first_time is None or done <= self.first_done:
            return None
        span = now - self.first_time
        if span < RATE_MIN_SAMPLE:
            return None
        return (done - self.first_done) / span

    def draw(self, phase, done, total, force=False):
        now = time.monotonic()
        if not force and now - self.last_draw < REDRAW_MIN_INTERVAL:
            return
        self.last_draw = now
        if self.first_time is None:
            self.first_done, self.first_time = done, now

        elapsed = now - self.start
        rate = self.rate(done, now)

        left = self.label if phase is None else "%s %s" % (self.label, phase)
        if total:
            right = " %3d%% %s/%s" % (
                done * 100 // total, "{:,}".format(done), "{:,}".format(total))
            if rate:
                right += "  %s eta %s" % (fmt_rate(rate),
                                          fmt_duration((total - done) / rate))
        else:
            # Unknown total: no bar is honest here, just counter and rate.
            right = " %s" % "{:,}".format(done)
            if rate:
                right += "  %s" % fmt_rate(rate)
            right += "  %s elapsed" % fmt_duration(elapsed)

        cols = shutil.get_terminal_size((80, 24)).columns
        line = left + right
        bar_width = cols - len(line) - 4          # ' [' + ']' + a spare column
        if total and bar_width >= BAR_MIN_WIDTH:
            filled = done * bar_width // total
            if filled <= 0:
                bar = ""
            elif filled >= bar_width:
                bar = "=" * bar_width
            else:
                bar = "=" * (filled - 1) + ">"
            line = "%s [%s%s]%s" % (left, bar, " " * (bar_width - len(bar)),
                                    right)

        line = line[:cols - 1]
        # Erase a longer previous line rather than leaving its tail behind.
        pad = " " * max(self.width - len(line), 0)
        self.stream.write("\r%s%s" % (line, pad))
        self.stream.flush()
        self.width = len(line)


def main():
    label = "indexing"
    argv = sys.argv[1:]
    if argv and argv[0] == "--label":
        label = argv[1]
        argv = argv[2:]
    if argv and argv[0] == "--":
        argv = argv[1:]
    if not argv:
        sys.stderr.write("usage: lore_progress.py [--label TEXT] -- CMD...\n")
        return 2

    mode = os.environ.get("LORE_PROGRESS", "auto")
    if mode == "never":
        interactive = False
    elif mode == "always":
        interactive = True
    else:
        interactive = sys.stderr.isatty()

    err = sys.stderr
    proc = subprocess.Popen(argv, stderr=subprocess.PIPE)
    bar = Bar(label, err) if interactive else None
    done = total = 0
    phase = None

    try:
        # Unbuffered readline: checkpoints are minutes apart, so waiting on a
        # block of output would freeze the bar.
        for raw in iter(proc.stderr.readline, b""):
            line = raw.decode("utf-8", "replace")
            match = COUNTER.match(line.rstrip("\n"))
            if match and bar:
                phase = match.group(1) or phase
                done = int(match.group(2))
                total = 0 if match.group(3) == "?" else int(match.group(3))
                bar.draw(phase, done, total)
            elif match:
                err.write(line)
                err.flush()
            else:
                if bar:
                    bar.clear()
                err.write(line)
                err.flush()
    finally:
        rc = proc.wait()
        if bar:
            if done:
                bar.draw(phase, done, total, force=True)
            if bar.width:
                err.write("\n")
                err.flush()

    return rc


if __name__ == "__main__":
    sys.exit(main())
