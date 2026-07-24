# Overview

This document describes the Qualcomm® Hexagon™ QEMU utility, which
is an emulator that decodes and translates the instructions of a DSP
program into instructions for a host architecture on a virtual
platform.

QEMU is designed differently from the Hexagon simulator. Instead of
simulating processor cycles, the instructions of a DSP program are
decoded and dynamically translated into instructions for the host
architecture. The execution will accurately emulate the architectural
behavior of the Hexagon DSP. Instructions are translated
just-in-time---as they are encountered during execution. This
translation cost is only paid once; executing the same code does not
require additional translation. Thus, the translation gives QEMU a
performance advantage over the simulator.

With QEMU, the default relationship between guest instruction
execution and guest system clocks is different from the simulator or
the target. Some DSP instructions require more host instructions than
others, which can cause some differences in how those clocks appear to
elapse during guest code emulation.

The software on the emulated DSP (the guest) can access the host
system via a set of semi-hosted *angel calls*, which are system calls
that the emulator handles to access the file system or console. The
QEMU interface matches the Hexagon simulator interface.

## Emulated Hardware

Like the Hexagon simulator, some hardware components of the DSP subsystem
are modeled.  Instead of being provided externally as co-simulation libraries,
they are included in QEMU Hexagon.  The specific components and their layout
in memory are designed to match the real device on which they're based.  These
emulated DSP subsystem models are called "machines" in the QEMU vocabulary.
The machines without real Qualcomm part numbers are not based on real devices
but instead synthetic ones that are intended to match a corresponding one from
the Hexagon simulator.

## Host system requirements

Releases of QEMU are provided for Linux and Windows 10 or 11. Starting with
QEMU Hexagon 10.0, the Linux binaries will work on Ubuntu 22.

### Linux dependencies

QEMU binaries distributed with the Hexagon Tools require the following
system libraries to be installed:

-   libasound
-   libc
-   libepoxy
-   libfdt
-   libgcc_s
-   libgio
-   libglib
-   libgmodule
-   libgobject
-   libm
-   libpixman
-   libpthread
-   libpulse
-   libutil
-   libvirglrenderer
-   libz

### Windows dependencies

QEMU Hexagon includes a PowerShell script to automatically download and install
the required dependencies for Windows. The script automatically detects your
system architecture (x86_64 or ARM64) and installs the appropriate dependencies.

To install dependencies, run the following command from PowerShell:

```powershell
.\quic\install_win_deps.ps1
```

The script will download the required DLL files to the current directory. You
can optionally specify a different installation directory using the `-InstallDir`
parameter:

```powershell
.\quic\install_win_deps.ps1 -InstallDir "C:\path\to\install"
```

After running the script, you should be able to open CMD or PowerShell and run
`qemu-system-hexagon --help`.

**Note**: If you encounter download errors like
`SSL certificate problem: unable to get local issuer certificate`, you may
have a corporate networking environment where all of the secure traffic is
diverted, defeating the trust feature of the package manager. See
["How can I make MSYS2/pacman trust my company's custom TLS CA certificate" in the MSYS2 FAQ](https://www.msys2.org/docs/faq/#how-can-i-make-msys2pacman-trust-my-companys-custom-tls-ca-certificate) for more info.

**Note**: If you encounter errors related to missing libraries (QEMU
immediately exits with or without an error message), you can use a [dependency
walker](https://github.com/lucasg/Dependencies) to identify which dependency
couldn't be resolved properly.

## HMX (Hexagon Matrix Extensions)

HMX (Hexagon Matrix Extensions) provides hardware acceleration for matrix
operations and is natively implemented in QEMU. HMX instructions are
available on Hexagon v75 and later CPU models when using machines that
support coprocessor2 (such as `sim_coproc`).

No external plugin or additional configuration is required for HMX support.
The implementation is fully integrated into QEMU and provides:

- Matrix multiply operations (FXP and FP modes)
- Weight loading from VTCM
- Accumulator management
- Convert and store operations
- Support for various data types (int8, fp16, f8)
