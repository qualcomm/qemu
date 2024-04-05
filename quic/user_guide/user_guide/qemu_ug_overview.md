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
system via a set of semi- hosted *angel calls*, which are system calls
that the emulator handles to access the file system or console. The
QEMU interface matches the Hexagon simulator interface.

## Host system requirements

Releases of QEMU are provided for Linux and Windows 10 or 11. Starting with
QEMU Hexagon 10.0, the Linux binaries will work on Ubuntu 20.

### Windows dependencies

1. First download and install msys2: [https://www.msys2.org/](https://www.msys2.org/)
2. On a msys2 shell, install the dependencies:
```
pacman -S mingw-w64-x86_64-libwinpthread-git mingw-w64-x86_64-glib2 \
          mingw-w64-x86_64-pixman mingw-w64-x86_64-libpng \
          mingw-w64-x86_64-gettext mingw-w64-x86_64-pcre2 \
          mingw-w64-x86_64-libiconv
```
3. Set up the Windows PATH variable to include the mingw directory when looking for DLLs:
    1. Open the Windows menu and search for "system variables", then click on
       "Edit the system environment variables".
    2. Click on the "Environment Variables" button at the bottom.
    3. At the bottom section, look for the "PATH" entry. Select it and hit "edit"
    4. Add two new entries:
    ```
    C:\msys64\mingw64\bin
    C:\msys64\mingw64\lib
    ```
    And hit "OK" in all the windows to save.

You should now be able to open CMD or PowerShell and run
`qemu-system-hexagon --help`.

When installing dependencies, if you get errors like
`SSL certificate problem: unable to get local issuer certificate`, you may
have a corporate networking environment where all of the secure traffic is
diverted, defeating the trust feature of the msys2 package manager.  See
["How can I make MSYS2/pacman trust my company's custom TLS CA certificate" in the MSYS2 FAQ](https://www.msys2.org/docs/faq/#how-can-i-make-msys2pacman-trust-my-companys-custom-tls-ca-certificate) for more info.

## Coprocessor plugin

Some Hexagon DSP configurations utilize a separate coprocessor plugin.
The coprocessor plugin provides emulation support for additional
instructions that are not a part of the Hexagon core. QEMU will automatically
look for the coprocessor plugin at the directory `../../QEMUCoprocPlugin/`,
relative to the QEMU binary itself. This is the default path where the
coprocessor is installed through QPM. Alternatively, you can use the CLI
arguments `-cpu any,coproc=<path>` to specify where the coprocessor directory
is located, or choose a machine that does not include a coprocessor.

## Required platform libraries

QEMU binaries distributed with the Hexagon Tools require the following
system libraries to be installed:

-   libasound
-   libc
-   libepoxy
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
-   libz
