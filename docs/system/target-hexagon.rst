.. SPDX-License-Identifier: GPL-2.0-or-later

.. _Hexagon-System-emulator:

Hexagon System emulator
-----------------------

Use the ``qemu-system-hexagon`` executable to simulate a 32-bit Hexagon
machine.

Hexagon Machines
================

Hexagon DSPs are suited to various functions and generally appear in a
"DSP subsystem" of a larger system-on-chip (SoC).

Hexagon DSPs are often included in a subsystem that looks like the diagram
below.  Instructions are loaded into DDR before the DSP is brought out of
reset and the first instructions are fetched from DDR via the EVB/reset vector.

In a real system, a TBU/SMMU would normally arbitrate AXI accesses but
we don't have a need to model that for QEMU.

Hexagon DSP cores use simultaneous multithreading (SMT) with as many as 8
hardware threads.

.. admonition:: Diagram

 .. code:: text

              AHB (local) bus                     AXI (global) bus
                    │                                 │
                    │                                 │
       ┌─────────┐  │       ┌─────────────────┐       │
       │ L2VIC   ├──┤       │                 │       │
       │         ├──┼───────►                 ├───────┤
       └─────▲───┘  │       │   Hexagon DSP   │       │
             │      │       │                 │       │        ┌─────┐
             │      │       │    N threads    │       │        │ DDR │
             │      ├───────┤                 │       │        │     │
        ┌────┴──┐   │       │                 │       ├────────┤     │
        │QTimer ├───┤       │                 │       │        │     │
        │       │   │       │                 │       │        │     │
        └───────┘   │       │   ┌─────────┐   │       │        │     │
                    │       │  ┌─────────┐│   │       │        │     │
        ┌───────┐   │       │  │  HVX xM ││   │       │        │     │
        │QDSP6SS├───┤       │  │         │┘   │       │        │     │
        └───────┘   │       │  └─────────┘    │       │        └─────┘
                    │       │                 │       │
        ┌───────┐   │       └─────────────────┘       │
        │  CSR  ├───┤
        └───────┘   │   ┌──────┐   ┌───────────┐
                    │   │ TCM  │   │   VTCM    │
                        │      │   │           │
                        └──────┘   │           │
                                   │           │
                                   │           │
                                   │           │
                                   └───────────┘

Components
----------
Other than l2vic and HVX, the components below are not implemented in QEMU.

* L2VIC: the L2 vectored interrupt controller.  Supports 1024 input
  interrupts, edge- or level-triggered.  The core ISA has system registers
  ``VID``, ``VID1`` which read through to the L2VIC device.
* QTimer: ARMSSE-based programmable timer device. Its interrupts are
  wired to the L2VIC.  System registers ``TIMER``, ``UTIMER`` read
  through to the QTimer device.
* QDSP6SS: DSP subsystem features, accessible to the entire SoC, including
  DSP NMI, watchdog, reset, etc.
* CSR: Configuration/Status Registers.
* TCM: DSP-exclusive tightly-coupled memory.  This memory can be used for
  DSPs when isolated from DDR and in some bootstrapping modes.
* VTCM: DSP-exclusive vector tightly-coupled memory.  This memory is accessed
  by some HVX instructions.
* HVX: the vector coprocessor supports 64 and 128-byte vector registers.
  64-byte mode is not implemented in QEMU.


Bootstrapping
-------------
Hexagon systems do not generally have access to a block device.  So, for
QEMU the typical use case involves loading a binary or ELF file into memory
and executing from the indicated start address::

    $ qemu-system-hexagon -kernel ./prog -append 'arg1 arg2'

Booting Linux under the H2 hypervisor
-------------------------------------
The ``virt`` machine can boot a guest under the H2 hypervisor.  Pass the
bundled ``hexagon_loadlinux_v68`` firmware with ``-bios``; it initializes H2
and jumps to a kernel loaded at physical address ``0xa0000000``, passing the
device tree address in ``r1:0``.  For the default ``virt`` CPU::

    $ qemu-system-hexagon -M virt -bios hexagon_loadlinux_v68 \
          -kernel vmlinux -initrd rootfs.cpio -append 'console=ttyAMA0'

The firmware is built from the ``roms/hexagon-hypervisor`` submodule, see
:ref:`Hexagon-Booter` and ``roms/README.hexagon-loadlinux``.  See
``Documentation/arch/hexagon/qemu-boot.rst`` in the Linux source tree for
kernel build instructions.

Booting Linux with QEMU as the Hexagon Virtual Machine
-------------------------------------------------------
A kernel written for the Hexagon Virtual Machine (HVM) interface does not
need a hypervisor firmware: with ``hvm=on`` the ``virt`` machine serves the
interface itself.  Leave out ``-bios``::

    $ qemu-system-hexagon -M virt,hvm=on -smp 4 -kernel vmlinux \
          -append 'console=ttyAMA0 root=/dev/vda rw' \
          -drive if=none,file=rootfs.ext2,format=raw,id=hd0 \
          -device virtio-blk-device,drive=hd0

The kernel is loaded at physical address ``0xa0000000`` and entered, in guest
mode, with the device tree address in ``r1:0``.  Every virtual CPU is a
hardware thread of its own, so a guest cannot start more of them than
``-smp`` gives it.  What the guest asks of the VM is done by QEMU:

* The virtual instructions: ``vmversion``, ``vmrte``, ``vmsetvec``,
  ``vmsetie``, ``vmgetie`` (which the core runs itself, see ``CCR.GRE``),
  ``vmintop``, ``vmclrmap``, ``vmnewmap``, ``vmcache``, ``vmgettime``,
  ``vmsettime``, ``vmwait``, ``vmyield``, ``vmstart``, ``vmstop``,
  ``vmvpid``, ``vmsetregs``, ``vmgetregs``, ``vmtimerop``,
  ``vmpmuconfig`` (which needs ``pmu=on`` to count) and ``vmgetinfo``.
  Any other ``trap1`` number is an error event in the guest.
* The hardware configuration call, ``trap0(#31)``.  ``trap0(#0)`` is a
  semihosting call, as it is everywhere on the Hexagon machines.
* Address translation, with the guest's own page tables, for all four
  ``vmnewmap`` types (linear, table, offset and variable-radix).  A fault is
  an error event, whose cause is the permission the access lacked.
* The virtual interrupt controller.  Interrupts 0 to 15 belong to a virtual
  CPU, the rest are shared; the ``virt`` machine's devices arrive as
  interrupts 32 and up, in the same numbering as the device tree gives them
  to a guest under H2.  ``vmtimerop`` timeouts arrive as interrupt 12.

The guest sees QEMU's own timers and not a QTimer, and no L2VIC: it is the
VM's virtual interrupt controller that the devices interrupt.  Nothing of a
hypervisor is in guest memory, so nothing needs protecting from the guest
either: the physical memory a guest may map is all of the machine's.

A freestanding guest of the interface can be loaded where it wants to run,
and started, with the generic loader::

    $ qemu-system-hexagon -M virt,hvm=on -nographic \
          -device loader,file=guest.elf,cpu-num=0

It starts with its virtual interrupts off and virtual addresses equal to
physical ones, until it asks for another map with ``vmnewmap``.

The ``virt`` machine also exposes a QUP GENI UART as its second serial port.
Linux's normal GENI driver uses DMA and registers it as ``/dev/ttyHS1``;
the first serial port, ``/dev/ttyAMA0``, remains the boot console.  A local
integration test exercises transmit, receive and reopening the QUP port with
a BusyBox ext2 root filesystem (the test uses a temporary disk snapshot)::

    $ python3 tests/functional/hexagon_qup_dma.py \
          --qemu ./build/qemu-system-hexagon \
          --bios ./pc-bios/hexagon_loadlinux_v68 \
          --kernel /path/to/vmlinux --rootfs /path/to/rootfs.ext2

The QUP model completes receive DMA when input arrives; it does not model
baud-dependent timing or UART parity, framing and hardware flow control.

Semihosting
-----------
Hexagon supports a semihosting interface similar to other architectures'.
The ``trap0`` instruction can activate these semihosting calls so that the
guest software can access the host console and filesystem. Read the
`Hexagon Semihosting Specification
<https://docs.qualcomm.com/doc/80-N2040-101/>`__
for details. Semihosting is enabled by default on hexagon-sim-compatible
machines. This can be further configured through ``-semihosting-config``.

Hexagon Features
================
.. toctree::
   hexagon/emulation
   hexagon/cdsp
   hexagon/booter

