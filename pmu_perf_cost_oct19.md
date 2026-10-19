<!--
 Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 SPDX-License-Identifier: GPL-2.0-or-later
-->

# Hexagon PMU Performance Cost - October 19, 2026

## Conclusion

The always-present system-mode PMU device did not show a measurable host-time
regression for the SDK HVX examples while SYSCFG.PM remained clear. The median
times were the same to within the 10 ms resolution of `/usr/bin/time` for
`sobel` and `boxfilter`; `gaussian` was faster in the PMU-on samples, which is
not a plausible PMU benefit and is treated as normal host/cache/run-order
noise.

Linux-user PMU support was added for this measurement.  With active packet
and HVX event tallying selected by `-cpu v79,pmu=on`, the 5-6 second
guest-internal workloads show no measurable regression after TBs were changed
to collect only the metrics selected by PMU events.  The largest observed
median change is +0.9% for Sobel.

The PMU machine property does not generate counter-update TCG until guest
software also sets `SYSCFG.PM`.  The SDK examples do not set that bit.  This
result therefore measures the cost of leaving the optional PMU device present
but inactive, which is effectively zero for these workloads.  It does not
measure the cost while a guest actively collects PMU events.

Based on this measurement, neither inactive system-mode PMU support nor the
new active linux-user PMU tallying is expensive enough to require opt-in for
performance reasons.  Retaining opt-in controls can still be appropriate to
control the guest-visible device surface and counter semantics.

## Metric Masking

The PMU has 83 programmable control bits: eight 10-bit event selectors in
PMUEVTCFG/PMUEVTCFG1 and PMUCFG, plus the 3-bit PMUCFG thread mask.  Those
selectors do not fit in QEMU's 32-bit TB flags.  The supported events reduce
to five gatherable metric classes: packet, load, store, memop, and HVX packet.
A 5-bit `TB_FLAGS.PMU_METRICS` field carries this derived mask.  A new TB is
generated when event selection changes, and it
emits updates only for selected metric classes.

Unsupported event selections contribute no metric bits and their counters
read as zero.  This avoids both fabricated values and packet-commit work for
events that QEMU cannot model.

## Setup

- QEMU source: `hex-next` at `2070d49ccc64bcf0a279529f8404b440d62947ce`
- QEMU build: fresh `build_pmu_perf`, native x86_64, `-O2 -g`, targets
  `hexagon-linux-user,hexagon-softmmu`
- One unrelated `-Werror=unused-const-variable` diagnostic in
  `hw/hexagon/virt.c` was demoted for the measurement build.  It does not
  change generated code for the measured Hexagon CPU or PMU paths.
- SDK examples: `/local/mnt/workspace/vp/qemu-hexagon-testing/sdk_examples`
  built in Release mode with Hexagon SDK 6.4.0.2 / Tools 19.0.04
- Input: `football1920x1080.bin`, with width 1920 and height 1080
- Timer: `/usr/bin/time -f %e`, reporting elapsed wall-clock seconds
- System mode: `qemu-system-hexagon -M sim -kernel <program>`
- Seven alternating off/on runs were collected per system-mode workload.
  Startup and file data were cached after the initial invocation; the table
  includes all samples for transparency.

## System-Mode Results

| Program | Before PMU device (s) | With PMU device (s) | Before median (s) | With median (s) | Median delta |
| --- | --- | --- | ---: | ---: | ---: |
| gaussian | 0.60, 0.51, 0.53, 0.52, 0.51, 0.53, 0.53 | 0.50, 0.49, 0.51, 0.51, 0.50, 0.50, 0.53 | 0.53 | 0.50 | -5.7% |
| sobel | 0.25, 0.23, 0.24, 0.23, 0.23, 0.24, 0.23 | 0.23, 0.24, 0.23, 0.24, 0.23, 0.24, 0.23 | 0.23 | 0.23 | 0.0% |
| boxfilter | 0.39, 0.39, 0.39, 0.41, 0.39, 0.39, 0.39 | 0.40, 0.40, 0.39, 0.39, 0.39, 0.38, 0.40 | 0.39 | 0.39 | 0.0% |

Negative deltas mean the always-present PMU was faster. Since it cannot make the gated
packet translator cheaper, the 30 ms gaussian difference is measurement
noise, reinforced by the cold first PMU-off sample.  No workload shows a
PMU-on slowdown.

For each workload, the final before/after output files had identical
SHA-256 hashes:

| Program | SHA-256 |
| --- | --- |
| gaussian | `2b0cdc4b5146334edef3edd063b0bdb6cd0a2654806a1b5fd0a886502571df2b` |
| sobel | `d6ef4cc31143ec3f8936751ab456ac65f0d100ffd9d5342dffdcdc872fa27c82` |
| boxfilter | `354a06dd0a4606f3976bf99dc45a04eb2ec5a1ef941f54457f8b8052fb9086df` |

## Linux-User Active PMU Results

Linux-user now accepts `-cpu v79,pmu=on`.  It has no privileged PMU register
configuration interface, so the read-only UPMUCNT aliases use fixed event
mappings: packets, committed loads, committed stores, committed memops, and
HVX packets in UPMUCNT0 through UPMUCNT4 (C20 through C24) respectively.
UPMUCNT5 through UPMUCNT7 read as zero.  PMU-off used `-cpu v79`; PMU-on used
`-cpu v79,pmu=on`.  A dedicated guest driver loaded its input once, invoked
the selected filter in a loop, and wrote its output once.  Five alternating
samples were collected for each workload.

| Program | PMU off samples (s) | PMU on samples (s) | Off median (s) | On median (s) | Median delta |
| --- | --- | --- | ---: | ---: | ---: |
| gaussian, 100 iterations | 5.99, 5.91, 5.93, 5.85, 5.89 | 5.98, 5.95, 5.88, 5.93, 5.85 | 5.91 | 5.93 | +0.3% |
| sobel, 80 iterations | 5.72, 5.72, 5.65, 5.65, 5.61 | 5.83, 5.78, 5.65, 5.66, 5.70 | 5.65 | 5.70 | +0.9% |
| boxfilter, 40 iterations | 5.78, 5.61, 5.73, 5.73, 5.74 | 5.61, 5.80, 5.74, 5.76, 5.70 | 5.73 | 5.74 | +0.2% |

The PMU-on and PMU-off output files matched for every workload:

| Program | SHA-256 |
| --- | --- |
| gaussian | `200d42eb3d5f76e5df94ca6241b7ceea6aaec532ad773680113d1242f05e8295` |
| sobel | `b204759e80a7010c02032d33cdd5ed3e1366a1e82341e98843ea1e8ae79bcf96` |
| boxfilter | `354a06dd0a4606f3976bf99dc45a04eb2ec5a1ef941f54457f8b8052fb9086df` |

The LLVM 22.1.8 linux-user cross compiler targets v79 successfully, but its
integrated assembler rejects both `r0 = c20` and `r0 = upmucnt0`.  A direct
guest UPMUCNT read was instead encoded as `.word 0x6a14c000`, the encoding for
`r0 = c20`.  The smoke test read C20 before and after a 10,000-iteration loop:
PMU-off returned `0 0`, while PMU-on returned `2452 132467`.  This validates
both the C20 dispatch and active packet-counter instrumentation.

## Limitation and Follow-Up

The relevant remaining experiment is a system-mode guest that sets
`SYSCFG.PM` and configures counters for the packet and HVX events.  That
causes `update_pmu_counters()` TCG to be generated at packet commit, just as
the active linux-user measurements did.  These SDK examples do not configure
the system PMU, so a purpose-built standalone PMU-enabled benchmark is still
needed to quantify the corresponding system-mode active-collection cost.
