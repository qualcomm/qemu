# Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
# SPDX-License-Identifier: GPL-2.0-or-later

"""Performance analysis utilities for QEMU Hexagon perf results.

Stdlib-only module -- no matplotlib, seaborn, or pandas dependency.
"""

import json
import math
import textwrap
from statistics import median as med, mean, stdev


def load_results(path):
    """Load a perf-results JSON file."""
    with open(path) as f:
        return json.loads(f.read())


def drop_timeouts(runs):
    """Filter out runs that timed out (dur_sec is falsy)."""
    return [run for run in runs if run['dur_sec']]


def geomean(xs):
    """Geometric mean of a sequence of positive numbers."""
    return math.exp(math.fsum(math.log(x) for x in xs) / len(xs))


def get_label(case_name, wrap_width=30):
    """Human-readable label from a case name, optionally line-wrapped."""
    label = case_name.replace('.pbn', '').replace('.elf', '') \
                     .replace('_', ' ')
    if wrap_width:
        label = '\n'.join(textwrap.wrap(label, wrap_width))
    return label


def review_suites(old, new):
    """Compare test suites and return (common_success, messages).

    common_success maps case label -> {new: case_dict, base: case_dict}
    for tests that passed in both runs.

    messages is a list of human-readable strings about differences.
    """
    def all_passed(case):
        rc_0 = all(run['rc'] == 0 for run in case['runs'])
        return rc_0 and case['check_passed']

    base_tests = frozenset(res['label'] for res in old['results'])
    new_tests = frozenset(res['label'] for res in new['results'])
    same_tests = base_tests == new_tests
    common = base_tests.intersection(new_tests)
    only_base = frozenset()
    only_new = frozenset()
    messages = []

    if not same_tests:
        only_base = base_tests - new_tests
        only_new = new_tests - base_tests
        messages.append(
            'WARNING, different tests between baseline and new! Omitted:')
        messages.append(
            f'\tunique to baseline: {", ".join(only_base)}')
        messages.append(
            f'\tunique to new     : {", ".join(only_new)}')

    new_by_name = {case['label']: case for case in new['results']}
    old_by_name = {case['label']: case for case in old['results']}

    common_compare = {
        label: {
            'new': all_passed(new_by_name[label]),
            'base': all_passed(old_by_name[label]),
        } for label in common
    }

    common_success = {}
    for case, compare in common_compare.items():
        if compare['new'] and not compare['base']:
            messages.append(f'Test fixed: {case}')
        elif not compare['new'] and compare['base']:
            messages.append(f'Test regression: {case}')
        elif not compare['new'] and not compare['base']:
            messages.append(f'Test (still) failing: {case}')
        else:
            common_success[case] = {
                'new': new_by_name[case],
                'base': old_by_name[case],
            }

    for case_name in only_base:
        if not all_passed(old_by_name[case_name]):
            messages.append(f'failed old: {case_name}')
    for case_name in only_new:
        if not all_passed(new_by_name[case_name]):
            messages.append(f'failed new: {case_name}')

    return common_success, messages


def extract_durs(results):
    """Extract {label: [runs]} dict with timeouts removed."""
    return {res['label']: drop_timeouts(res['runs'])
            for res in results['results']}


def extract_effs(results):
    """Extract {label: perf_ratio} dict."""
    return {res['label']: res['perf_ratio']
            for res in results['results']}


def get_commit_short(results):
    """Return short commit hash from results info, or 'commit unknown'."""
    commit = results['info']['qemu_commit_sysemu']
    return commit[:10] if commit else 'commit unknown'


def sorted_cases(common_success, baseline_durs):
    """Sort case names by descending median baseline duration."""
    def by_median_dur(case):
        runs = baseline_durs[case]
        return med(entry['dur_sec'] for entry in runs) if runs else 0.
    return sorted(common_success.keys(), key=by_median_dur, reverse=True)


def compile_duration_records(cases, base_durs, new_durs,
                             wrap_width=30):
    """Yield {category, case, label, dur_sec} dicts for all runs."""
    for case in cases:
        label = get_label(case, wrap_width)
        for dur in base_durs[case]:
            yield {'category': 'baseline',
                   'case': case,
                   'label': label,
                   'dur_sec': dur['dur_sec']}
        for dur in new_durs[case]:
            yield {'category': 'new',
                   'case': case,
                   'label': label,
                   'dur_sec': dur['dur_sec']}


def compile_efficiency_records(cases, base_eff, new_eff,
                               wrap_width=30):
    """Yield {case, label, eff} dicts where eff = new/baseline ratio."""
    for case in cases:
        if base_eff.get(case) and new_eff.get(case):
            yield {'case': case,
                   'label': get_label(case, wrap_width),
                   'eff': new_eff[case] / base_eff[case]}


def compile_stat_records(common_success, wrap_width=30):
    """Yield hardware counter ratio dicts for cases with stats."""
    def cache_miss_ratio(stats):
        return (float(stats['cache_misses'])
                / float(stats['cache_references']))

    def branch_mispr_ratio(stats):
        return (float(stats['branch_misses'])
                / float(stats['branches']))

    for case, results in common_success.items():
        new_stats = results['new']['stats']
        base_stats = results['base']['stats']

        name = results['new']['case_name']
        label = get_label(results['new']['label'], wrap_width)
        entry = {
            'case': name,
            'label': label,
            'new_host_inst_count': None,
            'baseline_inst_count': None,
            'host_inst_ratio': None,
            'cache_miss_ratio': None,
            'branch_mispr_ratio': None,
        }
        if new_stats and base_stats:
            entry.update({
                'new_host_inst_count':
                    new_stats['insts'] if new_stats else None,
                'baseline_inst_count':
                    base_stats['insts'] if base_stats else None,
                'host_inst_ratio':
                    float(new_stats['insts'])
                    / float(base_stats['insts']),
                'cache_miss_ratio':
                    cache_miss_ratio(new_stats)
                    / cache_miss_ratio(base_stats),
                'branch_mispr_ratio':
                    branch_mispr_ratio(new_stats)
                    / branch_mispr_ratio(base_stats),
            })

        yield entry


def compute_duration_summary(case, base_durs, new_durs):
    """Compute per-test duration statistics.

    Returns dict with keys: case, base_mean, base_std, new_mean, new_std,
    change_pct, speedup.  Returns None if insufficient data.
    """
    base_runs = [r['dur_sec'] for r in base_durs.get(case, [])
                 if r['dur_sec']]
    new_runs = [r['dur_sec'] for r in new_durs.get(case, [])
                if r['dur_sec']]
    if not base_runs or not new_runs:
        return None

    base_m = mean(base_runs)
    new_m = mean(new_runs)
    base_s = stdev(base_runs) if len(base_runs) > 1 else 0.0
    new_s = stdev(new_runs) if len(new_runs) > 1 else 0.0
    change_pct = ((new_m - base_m) / base_m) * 100.0 if base_m else 0.0
    speedup = base_m / new_m if new_m else float('inf')

    return {
        'case': case,
        'base_mean': base_m,
        'base_std': base_s,
        'new_mean': new_m,
        'new_std': new_s,
        'change_pct': change_pct,
        'speedup': speedup,
    }


def format_text_summary(old, new, base_durs, new_durs, base_eff,
                        new_eff, common_success, cases, messages):
    """Produce a full markdown summary string."""
    old_commit = get_commit_short(old)
    new_commit = get_commit_short(new)

    lines = []
    lines.append('## QEMU Hexagon Performance: '
                 f'baseline {old_commit} vs new {new_commit}')
    lines.append('')

    if messages:
        for msg in messages:
            lines.append(f'> {msg}')
        lines.append('')

    # --- Duration table ---
    summaries = []
    for case in cases:
        s = compute_duration_summary(case, base_durs, new_durs)
        if s:
            summaries.append(s)

    if summaries:
        lines.append('### Duration')
        lines.append('')
        lines.append(
            '| Test | Baseline (mean \u00b1 \u03c3) '
            '| New (mean \u00b1 \u03c3) | Change | Speedup |')
        lines.append(
            '|------|'
            '----------------------|'
            '-----------------|'
            '--------|---------|')
        speedups = []
        for s in summaries:
            name = get_label(s['case'], wrap_width=0)
            bm = f"{s['base_mean']:.3f}s \u00b1 {s['base_std']:.3f}s"
            nm = f"{s['new_mean']:.3f}s \u00b1 {s['new_std']:.3f}s"
            ch = f"{s['change_pct']:+.1f}%"
            sp = f"{s['speedup']:.2f}x"
            lines.append(f'| {name} | {bm} | {nm} | {ch} | {sp} |')
            speedups.append(s['speedup'])
        lines.append('')
        if speedups:
            gm = geomean(speedups)
            lines.append(
                f'**Overall geometric mean speedup: {gm:.3f}x**')
            lines.append('')

    # --- Efficiency table ---
    eff_records = list(compile_efficiency_records(
        cases, base_eff, new_eff, wrap_width=0))
    if eff_records:
        lines.append('### Efficiency (host:guest instruction ratio)')
        lines.append('')
        lines.append('| Test | New / Baseline | Verdict |')
        lines.append('|------|----------------|---------|')
        for rec in eff_records:
            name = get_label(rec['case'], wrap_width=0)
            ratio = rec['eff']
            if ratio < 1.0:
                verdict = 'better'
            elif ratio == 1.0:
                verdict = 'same'
            else:
                verdict = 'worse'
            lines.append(
                f'| {name} | {ratio:.3f} | {verdict} |')
        lines.append('')

    # --- Hardware counters table ---
    stat_records = [r for r in compile_stat_records(
                        common_success, wrap_width=0)
                    if r['host_inst_ratio'] is not None]
    if stat_records:
        stat_records.sort(
            key=lambda r: r['new_host_inst_count'] or 0,
            reverse=True)
        lines.append(
            '### Hardware Counters (new / baseline ratio)')
        lines.append('')
        lines.append(
            '| Test | Host Insts | Cache Miss | Branch Mispr |')
        lines.append(
            '|------|-----------|------------|-------------|')
        for rec in stat_records:
            name = get_label(rec['case'], wrap_width=0)
            hi = f"{rec['host_inst_ratio']:.3f}"
            cm = f"{rec['cache_miss_ratio']:.3f}"
            bm = f"{rec['branch_mispr_ratio']:.3f}"
            lines.append(f'| {name} | {hi} | {cm} | {bm} |')
        lines.append('')

    return '\n'.join(lines)
