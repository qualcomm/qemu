#!/usr/bin/env python3
#
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Run meson test, then retry only the failed tests up to N times.
# Usage: meson-test-retry.py <retries> <meson test args...>
#
# Must be run from QEMU source folder, with build folder in ./build.

import glob
import json
import os
import re
import subprocess
import sys

MESON = ['./build/pyvenv/bin/meson', 'test', '-C', 'build']


def options_with_value():
    # all meson test options take zero or one value, and help lists those
    # taking one as "OPTION METAVAR"
    out = subprocess.run(MESON + ['--help'], check=True,
                         capture_output=True, text=True).stdout
    return set(re.findall(r'(?<![\w-])(--?[\w-]+) [A-Z][A-Z_/]*\b', out))


def remove_test_names(args):
    # remove positional args (test names), so they can be replaced by the
    # failed ones. Failed tests are a subset of the ones selected anyway.
    with_value = options_with_value()
    res = []
    skip_next = False
    for a in args:
        if skip_next:
            skip_next = False
        elif a.startswith('-'):
            skip_next = a in with_value
        else:
            continue
        res.append(a)
    return res


def failed_tests():
    # log name depends on --setup (testlog-<setup>.json): pick latest one
    logs = glob.glob(os.path.join('build', 'meson-logs', 'testlog*.json'))
    if not logs:
        return []
    names = set()
    with open(max(logs, key=os.path.getmtime)) as f:
        for line in f:
            t = json.loads(line)
            if not t['is_fail']:
                continue
            # name is "[suites - ]project:test"
            name = t['name'].split(' - ', 1)[-1]
            project, test = name.split(':', 1)
            # test names are matched as fnmatch patterns
            names.add(project + ':' + re.sub(r'([*?[])', r'[\1]', test))
    return sorted(names)


def main():
    retries = int(sys.argv[1])
    args = sys.argv[2:]

    ret = subprocess.run(MESON + args).returncode
    if ret == 0 or retries == 0:
        return ret
    retry_args = remove_test_names(args) + ['--no-rebuild']
    for i in range(1, retries + 1):
        failed = failed_tests()
        # nothing to retry (e.g. build failure)
        if not failed:
            break
        print(f'=== retry {i}/{retries} for failed tests:', *failed,
              sep='\n', flush=True)
        ret = subprocess.run(MESON + retry_args + failed).returncode
        if ret == 0:
            break
    return ret


if __name__ == '__main__':
    sys.exit(main())
