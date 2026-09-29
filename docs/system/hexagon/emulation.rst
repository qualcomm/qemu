.. SPDX-License-Identifier: GPL-2.0-or-later

.. _Hexagon Emulation:

Hexagon CPU architecture support
================================

QEMU's TCG emulation includes support for v65, v66, v67, v68, v69, v71, v73,
v75, v77, v79, v81, v83, v85.
It also has support for the following architecture extensions:

- HVX (Hexagon Vector eXtensions)
- `HVX floating point <https://docs.qualcomm.com/doc/80-N2040-61/topic/hvx-floating-point.html>`_.

For information on the specifics of the HVX extension, please refer
to the `Qualcomm Hexagon V79 HVX Programmer's Reference Manual
<https://docs.qualcomm.com/doc/80-N2040-61/>`_.

Performance Monitoring Unit (PMU)
----------------------------------

The Hexagon PMU is modeled as an optional device, off by default. Enable it
with a machine property::

    $ qemu-system-hexagon -M <machine>,pmu=on ...

When enabled, every vCPU gets an optional link to a shared ``hexagon-pmu``
device holding the 8 configurable event counters (``PMUCNT0``-``PMUCNT7``)
and their event-select registers (``PMUEVTCFG``, ``PMUEVTCFG1``,
``PMUCFG``, ``PMUSTID0``, ``PMUSTID1``), plus the ``GPMUCNT0``-``GPMUCNT7``
global-monitor aliases (readable when ``SSR.CE`` is set, like ``GPCYCLE``).
The read-only ``UPMUCNT0``-``UPMUCNT7`` user-mode aliases are also
implemented.
Guest software configures a counter's event through
``PMUEVTCFG``/``PMUEVTCFG1`` (event id bits 7:0) and ``PMUCFG`` (event id
bits 9:8, plus the thread mask), and reads it back through the matching
``PMUCNTn``. The whole unit is additionally gated by ``SYSCFG.PM``: with
the machine property off, or with ``SYSCFG.PM`` clear, no counter advances
and no extra code is generated for the packet translator, so leaving PMU
support enabled at the machine level costs nothing until a guest actually
turns it on.

Events implemented
^^^^^^^^^^^^^^^^^^^

QEMU's TCG emulation is a functional simulator: it has no cache, branch
predictor, bus, or pipeline timing model. Only PMU events derivable from
packet decode and commit are implemented:

================================== ========= ==========================================
Event                              Event id  Notes
================================== ========= ==========================================
``COMMITTED_PKT_ANY``              0x003     Packets committed, summed across all
                                              hardware threads
``COMMITTED_PKT_T0``-``T7``        0x00c-    Packets committed by one specific hardware
                                    0x016     thread
``COMMITTED_LD``                   0x030     Load instructions committed
``COMMITTED_ST``                   0x031     Store instructions committed
``COMMITTED_MEMOP``                0x032     ``memop``-attributed instructions committed
``HVX_PKT``                        0x111     HVX packets committed, summed across all
                                              hardware threads
``HVX_PKT_THREAD``                 0x112     HVX packets committed by the requesting
                                              hardware thread
``HVXPIPE_ALU``                    0x128     HVX ALU-class instructions committed
``HVXPIPE_MPY``                    0x129     HVX multiply-class instructions committed
``HVXPIPE_SHIFT``                  0x12a     HVX shift-class instructions committed
``HVXPIPE_PERM``                   0x12b     HVX permute-class instructions committed
================================== ========= ==========================================

Any other event id a guest selects is accepted -- the counter simply does
not advance -- and is logged once via ``LOG_UNIMP``, rather than silently
returning a fabricated count. This covers the majority of the architected
PMU event list: cache and TLB events, AXI/AHB bus transaction events,
branch-predictor events, and pipeline stall/replay/throttle events all
require a timing model this emulation does not have. The
``CYCLES_1_HVX_CONTEXTS_RUNNING`` through ``CYCLES_6_HVX_CONTEXTS_RUNNING``
events are excluded for the same reason: an accurate count needs a
cycle-level model of concurrently-running HVX contexts, which is not
meaningful under QEMU's unsynchronized multi-threaded TCG execution.

Modeling notes
^^^^^^^^^^^^^^

- All hardware threads on a core share one physical bank of 8 counters,
  the same way this emulation already models other cross-thread system
  registers such as ``MODECTL`` and ``SYSCFG``, rather than giving each
  hardware thread independent PMU state.
- Events with no ``_ANY``/``_THREAD`` or ``_T<n>`` variant
  (``COMMITTED_LD``, ``COMMITTED_ST``, ``COMMITTED_MEMOP``,
  ``HVXPIPE_ALU``/``MPY``/``SHIFT``/``PERM``) are read as the requesting
  hardware thread's own tally.
- HVX instructions that use two functional pipes in one op (for example
  a combined permute/shift or shift/multiply) are not attributed to any
  single ``HVXPIPE_*`` bucket: which physical pipe would execute them is
  not derivable from decode attributes alone.

For the full PMU event list and register layout, refer to the
`Hexagon V81 PMU Events reference
<https://docs.qualcomm.com/doc/80-N2040-60/topic/pmu-events.html>`_ and the
`Hexagon V81 HVX PMU Events reference
<https://docs.qualcomm.com/doc/80-N2040-61/topic/hvx-pmu-events.html>`_.
