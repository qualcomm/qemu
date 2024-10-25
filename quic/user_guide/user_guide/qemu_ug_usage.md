# Use QEMU

QEMU provides many command line arguments to customize how the
emulator is to run. Several important options for the Hexagon
processor are described in this document. These options allow you to
customize how the emulator is to run.

## Command

The QEMU binary is called qemu-system-hexagon. To start the emulator
from the command line:

:::{code-block}
qemu-system-hexagon [option]
:::

Where `option` is one or more of the following arguments for the
Hexagon processor.

## Options

- [**Get help**](#get-help)

  :::{code-block}
  --help
  :::

- [**System configuration**](#system-configuration)

  :::{code-block}
  -cpu any

  -cpu any,count-gcycle-xt=on,usefs=<dir>

  -m 6G
  :::

- [**Timing**](#timing)

  :::{code-block}
  -icount [shift=N|auto]
  :::

- [**Miscellaneous**](#miscellaneous)

  :::{code-block}
  -append 'x y z 1 2 3'

  -gdb tcp::1234 | -s

  -kernel <file>

  --machine V68N_1024 | -M V68N_1024

  -monitor [none|stdio]

  -S

  -accel tcg,thread=multi
:::

(get-help)=
### Get help

- `-help`

   Display command usage information and then exit. For example, to show help on how to specify the system memory:

   :::{code-block}
   qemu-system-hexagon -m help
   :::

(system-configuration)=
### System configuration

- `-cpu any,[option,option,...]`

   System configuration option that launches a system using a generic
   Hexagon CPU.

   QEMU does not have the capability to select different architectures.
   The most recent DSP architecture is enabled, and it will support
   instructions and coprocessors introduced in older architectures.

   Additional options are comma-separated and include the following:

- `count-gcycle-xt=on`

  Enables emulation of the gcycle_nT registers. Emulating this feature
  is expensive enough that it is not enabled by default.

  For more information, see [Section 3.4.1](#cycle-counts).

- `usefs=<dir>`

  If the emulated Hexagon program attempts to open a .so file and it
  cannot be found, prepends the path specified by `<dir>` to the .so
  filename and then tries to open the file again.

- `-m 6G`

   System configuration option that launches a guest Hexagon system with
   6 gigabytes of RAM.

(timing)=
### Timing

Because QEMU is designed differently from a cycle-accurate simulator,
it can result in less predictable execution. The semantics of the
architecture are always preserved, but you might experience results
that do not exactly match target or simulator behavior, especially
when interacting with clock devices. QEMU supports a special mode that
might show more consistency.

- `-icount [shift=N|auto]`

  Dispatch instructions on a specified frequency that corresponds to the
  virtual clock. Using -icount mode will make results more
  deterministic.

  - *shift=N*

    Virtual CPU will execute one instruction every 2N ns of virtual time.

  - *auto*

    Virtual CPU speed is automatically adjusted to keep virtual time
    within a few seconds of real time.

  :::{NOTE}
  The -icount mode typically makes execution significantly slower, so use
  it with caution.
  :::

(miscellaneous)=
### Miscellaneous

- `-append 'x y z 1 2 3'`

  Specify the space-delimited command-line arguments for the Hexagon
  program that is being executed in System mode.

  The Hexagon guest program can access these arguments in the same way
  the simulator accesses them: a SYS_GET_CMDLINE angel call provides the
  access.

- `-gdb tcp::1234`
  </br>
  `-s`

  Launch QEMU with a debug server that is listening to local TCP port
  1234.

- `-kernel <file>`

  Run a specified Hexagon program (for more information, see [](#run-programs)).

- `--machine V68N_1024`
  </br>
  `-M V68N_1024`

   Launch QEMU with a configuration table and memory map that match the
   V68N_1024 core.

   Additional configurations:

   ```{eval-rst}

   .. flat-table::
      :header-rows: 1

      * - Machine
        - CPUs
        - Coprocessor plugin required

      * - virt (default machine)
        - 6
        - No

      * - virt_coproc
        - 6
        - Yes

      * - SA8540P_CDSP0
        - 6
        - Yes

      * - SA8775P_CDSP0
        - 6
        - Yes

      * - V66G_1024
        - 4
        - Yes

      * - V66_Linux
        - 4
        - No

      * - V68N_1024
        - 6
        - Yes

      * - V68_H2
        - 4
        - Yes

      * - V69NA_1024
        - 6
        - Yes

      * - V73NA_1024
        - 6
        - Yes

      * - V73_Linux
        - 6
        - No

      * - V75NA_1024
        - 6
        - Yes

      * - V75_Linux
        - 6
        - No

      * - V79M_1
        - 4
        - No

      * - V79NA_1
        - 8
        - Yes

      * - V79_Linux
        - 8
        - Yes

      * - V81QA_1
        - 12
        - Yes
   ```

- `-monitor [none|stdio]`

  Specify how to set QEMU's interactive monitor.

  - *none*
  - *stdio*

- `-S`

  Disable the interactive monitor.

  When the monitor is enabled, it consumes stdin, which will conflict
  with your guest program's use of stdin.

  Connect the monitor to the console. Use the interactive module to
  investigate and trace the guest program execution.

  Several features of this monitor are also available in hexagon-lldb.

  Halt execution at the startup point. This option is useful when using
  a debugger.

- `-accel tcg,thread=multi`

  Enables host side multi-threading to greatly increase emulator
  performance.


## Usage

### Run programs

The simplest way to run a Hexagon program is to enter the following
command:

:::{code-block}
qemu-system-hexagon -kernel ./test_prog
:::

QEMU will initialize the Hexagon system, load test_prog into memory,
and start executing code at the entry point specified by test_prog.

### Device emulation

The QTimer and L2VIC devices are provided as a part of the overall
Hexagon system emulation. These devices are enabled by default and can
be accessed via memory-mapped I/O using the addresses provided in the
system configuration table.

The generic QEMU system provides the capability for using disk images
as emulated storage devices. Hexagon QEMU does not support these
devices. Instead, use the -kernel option to launch a program.

### Debugging

To debug a simulation, you can use hexagon-lldb to either attach to an
existing QEMU session or launch a QEMU session. Using hexagon-lldb,
you can set watchpoints, breakpoints, step through code, and read and
write registers the same way that you do with the simulator or the
target.

For example, after starting QEMU with the -s -S options, you can
connect to the session as follows:

:::{code-block}
>>> hexagon-lldb
(lldb) file </path/to/executable.elf>
Current executable set to '</path/to/executable.elf>' (hexagon).
(lldb) gdb-remote 1234
Process 1 stopped
* thread #1, stop reason = signal SIGTRAP
    frame #0: 0x00000000
->  0x0: { jump 0x44 }
:::

### Plug-ins

QEMU plug-ins allow you to add new functionality to QEMU. You can
write a plug-in library that can inspect and alter the system state
during translation and translation-block-execution.

The specifics of the interface are described at
https://qemu.readthedocs.io/en/latest/devel/tcg-plugins.html and in
the QEMU source in *./include/qemu/qemu-plugin.h* and
*./docs/devel/tcg-plugins.rst.* Example
plug-ins are included in the source distribution of QEMU in the
*./contrib/plugins/* directory.

The source distribution for qemu-system-hexagon that comes with the
Hexagon Tools is in *Tools/src/qemu_src.tgz*.

You can invoke QEMU and load one or more plug-in libraries as follows:

:::{code-block}
qemu-system-hexagon -kernel ./some-prog \

-plugin libmy_trace.so,arg1=on,arg2=12 \

-plugin libmy_debug.so
:::

## Architecture caveats

Some architecture features cannot be replicated with QEMU. You can
expect to see the following deviations from the architecture.

### Cycle counts

The system architecture defines the following registers that are
intended to count cycles that have elapsed:

-   `pcycleNt`
-   `gpcycleNt` -- Are expensive to model, so they are off by default.
    They can be enabled using a CPU property.
-   `gpcyclelo/gpcyclehi -- Mirror `PCYCLEHI/PCYCLELO` registers in Guest
    mode. The values are only valid when the CE bit in SSR is set.
-   `pcyclelo/pcyclehi` -- Are incremented each time a packet starts. If
    the packet is replayed, `PCYCLEHI/PCYCLELO` is incremented each time.
-   `upcyclelo/upcyclehi` -- Mirror `PCYCLEHI/PCYCLELO` registers in User
    mode. The values are only valid when the CE bit in SSR is set.
-   And so on

These registers are modeled in QEMU as counting single packets,
regardless of the instructions in the packet. Thus the cycle counts
will not exactly match the counts reported by the simulator, but they
should be able to coarsely measure the overall system utilization.

QEMU does not model stalls, so it cannot give an accurate cycle count.
However, you can use the number of packets as a proxy.

### HVX 64-byte mode

The system architecture defines HVX coprocessor support to have
different precisions controlled by the V2X bit in SYSCFG. QEMU does
not support the 64-byte mode (value 0 in the V2X bit).

### Interrupts

The system architecture for interrupt handling changed between Hexagon
V62 and V65.

-   V62 and earlier versions have a different set of registers for
    accessing `ipend` and `iad`. QEMU does not support these registers.
-   QEMU supports only the interrupt registers in V65 and later
    versions.
