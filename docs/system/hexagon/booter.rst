.. SPDX-License-Identifier: GPL-2.0-or-later

.. _Hexagon-Booter:

The H2 Hypervisor ``booter``
=============================

``booter`` is a program from the H2 hypervisor project
(``roms/hexagon-hypervisor``) that initializes the H2 hypervisor and then
loads and starts a second guest ELF executable inside a virtual machine
that H2 creates.  It is distinct from ``loadlinux`` (see :doc:`cdsp`):
``loadlinux`` boots a Linux kernel at a fixed address without any argument
passing, while ``booter`` is a
general-purpose loader driven by command-line arguments, normally used to
run H2's own test suite and example guests under the reference simulator.

``booter`` is built for each of the board machines, producing
``pc-bios/hexagon_booter_<suffix>`` (see :doc:`cdsp` for the list).  Like
``loadlinux``, it is built with ``NULL_ANGEL_TRAP=1``, so that the
semihosting requests it makes to receive its command line and to load the
guest ELF named on that command line are handled by QEMU.

Building
--------

``booter`` is built from the ``roms/hexagon-hypervisor`` submodule with
the Hexagon SDK toolchain.  To rebuild the bundled images, ensure
``hexagon-clang`` and ``hexagon-strip`` (from the
`Hexagon SDK <https://softwarecenter.qualcomm.com/catalog/item/Hexagon_SDK>`_)
are on your ``PATH``, then from the top of the QEMU tree run, for example::

  make -C roms hexagon-qcs6490-cdsp

This updates ``pc-bios/hexagon_booter_qcs6490_cdsp`` and
``pc-bios/hexagon_loadlinux_qcs6490_cdsp`` in place.  A single image can be
rebuilt with, e.g., ``make -C roms hexagon-booter-qcs6490-cdsp``.  To build
H2 and ``booter`` by hand, run, from the top of the submodule::

  make USE_PKW=0 ARCHV=<68|73|81> TARGET=opt

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

``booter`` runs as a ``-kernel`` image under ``qemu-system-hexagon``, e.g.
with ``-M sim``, and as ``-bios`` on the board machines.  Its command line
is given with ``-append`` when it is the ``-kernel``, for example::

  qemu-system-hexagon -M sim -cpu v73 -display none -kernel booter \
      -append 'pickle_driver.elf --pickle_in model.pickle'

``booter`` installs its own monitor-mode exception vector table and services
semihosting requests through H2's ``H2K_trap_angel`` handler
(``roms/hexagon-hypervisor/kernel/traps/angel/angel.ref.S``).  That handler
talks to the simulator through a memory-mapped mailbox at a fixed virtual
address (``ANGEL_VA``, ``0xffd00000``), which QEMU does not implement, so a
``booter`` built with the angel handler (the default for H2 itself) waits
there forever.  The ``booter`` images that run under QEMU are built with
``NULL_ANGEL_TRAP=1``.  To run H2's own test suite, use ``hexagon-sim`` as
described above.

Note also that with H2 built for a 4MB ``DEVICE_PAGE_SIZE``, ``ANGEL_VA``
falls *inside* the coarser TLB entry that ``booter`` installs for
``Q6_SS_BASE_VA`` (``0xffc00000``).  Two valid entries then match the
same virtual address, which is a multi-TLB match, not a most-specific-
wins lookup: ``hexagon-sim`` reports ``0x44, Multiple TLB match`` for a
fine entry nested inside a coarse one, and QEMU raises the same
architectural imprecise exception
(``HEX_CAUSE_IMPRECISE_MULTI_TLB_MATCH``).  The fix belongs on the H2
side -- either keep the device page small enough that it does not reach
``ANGEL_VA``, or move ``ANGEL_VA``/``Q6_SS_BASE_VA`` apart.
