.. SPDX-License-Identifier: GPL-2.0-or-later

.. _Hexagon-Booter:

The H2 Hypervisor ``booter``
=============================

``booter`` is a program from the H2 hypervisor project
(``roms/hexagon-hypervisor``) that initializes the H2 hypervisor and then
loads and starts a second guest ELF executable inside a virtual machine
that H2 creates.  It is distinct from ``loadlinux`` (see
``roms/README.hexagon-loadlinux``): ``loadlinux`` boots a Linux kernel at
a fixed address without any argument passing, while ``booter`` is a
general-purpose loader driven by command-line arguments, normally used to
run H2's own test suite and example guests under the reference simulator.

``booter`` is built for three Hexagon architecture revisions -- v68, v73
and v81 -- producing ``pc-bios/booter_v68``, ``pc-bios/booter_v73`` and
``pc-bios/booter_v81``.  Unlike ``loadlinux``, it is compiled *without*
``NULL_ANGEL_TRAP``, i.e. with the angel/semihosting trap handler compiled
in, since it depends on semihosting to receive its command line and to
load the guest ELF named on that command line.

Building
--------

``booter`` is built from the ``roms/hexagon-hypervisor`` submodule with
the Hexagon SDK toolchain.  From the top of that submodule, build H2
itself, then booter, for each ARCHV in turn::

  make USE_PKW=0 ARCHV=<68|73|81> TARGET=opt

To rebuild the bundled images, ensure ``hexagon-clang`` and
``hexagon-strip`` (from the
`Hexagon SDK <https://softwarecenter.qualcomm.com/catalog/item/Hexagon_SDK>`_)
are on your ``PATH``, then from the top of the QEMU tree run::

  make -C roms hexagon-booter

This updates ``pc-bios/booter_v68``, ``pc-bios/booter_v73`` and
``pc-bios/booter_v81`` in place.  A single variant can be rebuilt with,
e.g., ``make -C roms hexagon-booter-v68``.

Usage under hexagon-sim
------------------------

``booter`` is designed to run under ``hexagon-sim``, the Hexagon
reference simulator, which implements the host side of its semihosting
protocol.  This is the supported and tested invocation, documented in
``roms/hexagon-hypervisor/README.md``::

  hexagon-sim <options> -- install/bin/booter <options> <application executable>
  hexagon-sim <options> -- install/bin/booter --help  # list available booter options

The H2 test harness (``roms/hexagon-hypervisor/scripts/Makefile.inc.test``)
uses exactly this pattern to run its own unit tests: each test builds a
small guest ELF (``test.elf``) and runs it as::

  hexagon-sim <simulator options> --profile -- install/bin/booter <booter options> test.elf

Notable ``booter`` command-line options (see ``booter --help`` for the
full/current list):

* ``--trapmask <int>``, ``--skip_load (0|1)``, ``--guest_base <int>``
  (default ``0x80000000``), ``--hwt_mask <int>`` -- global options.
* ``--new_vm <instances>`` followed by per-VM options such as
  ``--va_angel (0|1)``, ``--page_size [0-6]``, ``--fence_lo``/``--fence_hi``,
  ``--load_offset``, ``--num_vcpus`` (default 32), ``--startprio``,
  ``--error_exit (0|1, default 1)``.

For example, to run the H2 kernel-init self-test bundled in
``roms/hexagon-hypervisor/kernel/init/boot/test``::

  cd roms/hexagon-hypervisor/kernel/init/boot/test
  make ARCHV=81 TARGET=opt test.elf
  hexagon-sim -mv81 -- ../../../../artifacts/v81/opt/install/bin/booter test.elf

A passing run prints ``TEST PASSED`` to the console before exiting.

Running ``booter`` under ``qemu-system-hexagon``
--------------------------------------------------

``booter`` can be used as a ``-kernel`` image under ``qemu-system-hexagon``
(e.g. with ``-M sim``), but only in a limited way.  QEMU's Hexagon
semihosting support (``target/hexagon/hexswi.c``) implements
ARM-compatible semihosting that is invoked when guest code executes
``trap0`` directly.  ``booter``, however, installs its own monitor-mode
exception vector table and services semihosting requests through H2's
``H2K_trap_angel`` handler
(``roms/hexagon-hypervisor/kernel/traps/angel/angel.ref.S``), which
communicates with the simulator via a memory-mapped mailbox at a fixed
virtual address (``ANGEL_VA``, ``0xffd00000``): it writes the request
there and then polls the same location in a loop, waiting for
``hexagon-sim`` to service the request and clear the flag.

QEMU does not implement this mailbox protocol.  Instead, the ``sim``
machine backs the mailbox with a stub device
(``hw/hexagon/hexagon-angel-mbox.c``) that always reads back a cleared
busy flag, unblocking ``booter``'s poll loop without actually servicing
the request: console output text, exit codes, and any other request
content written to the mailbox are discarded.  This is enough for
``booter`` to make progress and run to completion, but any output or
behavior that depends on the angel/semihosting request actually being
serviced (e.g. console text printed via that path) will not appear.

Note that ``ANGEL_VA`` is nested inside a coarser TLB entry that
``booter`` installs for ``Q6_SS_BASE_VA`` (``0xffc00000``), mapped to
the machine's ``csr_base``.  On real hardware and under ``hexagon-sim``,
the more specific ``ANGEL_VA`` entry takes priority, but QEMU's TLB
lookup matches the coarser entry first, so accesses to ``ANGEL_VA``
actually resolve to an offset within the ``csr_base`` window rather than
to physical address ``0`` as ``booter`` intends.  The stub device is
mapped at ``csr_base`` to account for this.  Running, e.g.::

  qemu-system-hexagon -M sim -kernel pc-bios/booter_v81 -append test.elf \
      -semihosting -semihosting-config target=native

now boots and runs ``booter`` instead of hanging.  To run H2's test
suite with full semihosting fidelity (including its own console
output), continue to use ``hexagon-sim`` as described above.

