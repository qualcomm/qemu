/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Linker script for Hexagon baremetal programs built with the
 * hexagon-unknown-none-elf toolchain (picolibc + crt0-semihost).
 *
 * All sections are placed in a single contiguous RAM region, which
 * avoids the need for ALIGN_WITH_INPUT (a GNU ld extension not
 * supported by ld.lld).
 */

ENTRY(_start)

MEMORY
{
    ram (rwx) : ORIGIN = 0x00100000, LENGTH = 64M
}

PROVIDE(__stack = ORIGIN(ram) + LENGTH(ram));

SECTIONS
{
    /* crt0-semihost places _start in .text.init.enter */
    .init : {
        KEEP (*(.text.init.enter))
        KEEP (*(.data.init.enter))
        KEEP (*(SORT_BY_NAME(.init) SORT_BY_NAME(.init.*)))
    } >ram

    .text : {
        *(.literal.unlikely .text.unlikely .literal.unlikely.* .text.unlikely.*)
        *(.literal.startup .text.startup .literal.startup.* .text.startup.*)
        *(.literal .text .literal.* .text.* .opd .opd.*)
        *(.gnu.linkonce.t.*)
        KEEP (*(.fini .fini.*))
        __text_end = .;
        PROVIDE (__etext = __text_end);
        PROVIDE (_etext = __text_end);
        PROVIDE (etext = __text_end);
        . = ALIGN(8);
        PROVIDE_HIDDEN (__preinit_array_start = .);
        KEEP (*(.preinit_array))
        PROVIDE_HIDDEN (__preinit_array_end = .);
        PROVIDE_HIDDEN (__init_array_start = .);
        KEEP (*(SORT_BY_INIT_PRIORITY(.init_array.*) SORT_BY_INIT_PRIORITY(.ctors.*)))
        KEEP (*(.init_array .ctors))
        PROVIDE_HIDDEN (__init_array_end = .);
        PROVIDE_HIDDEN (__fini_array_start = .);
        KEEP (*(SORT_BY_INIT_PRIORITY(.fini_array.*) SORT_BY_INIT_PRIORITY(.dtors.*)))
        KEEP (*(.fini_array .dtors))
        PROVIDE_HIDDEN (__fini_array_end = .);
    } >ram

    .rodata : {
        *(.rodata .rodata.* .gnu.linkonce.r.*)
        . = ALIGN(8);
        PROVIDE( __global_pointer$ = . + 0x800 );
        PROVIDE( _gp = . + 0x8000 );
    } >ram

    /*
     * TLS template sections must precede .data so that the
     * TLS-related symbols are computed correctly.
     */
    .tdata : {
        *(.tdata .tdata.* .gnu.linkonce.td.*)
        . = ALIGN(4);
    } >ram

    __tls_base = ADDR(.tdata);

    .tbss (NOLOAD) : {
        *(.tbss .tbss.* .gnu.linkonce.tb.*)
        *(.tcommon)
        . = ALIGN(4);
    } >ram

    __tls_space_size = SIZEOF(.tdata) + SIZEOF(.tbss);

    .data : {
        *(.data .data.* .gnu.linkonce.d.*)
        . = ALIGN(8);
        *(.sdata .sdata.* .sdata2.* .gnu.linkonce.s.*)
        __data_end = .;
        PROVIDE (_edata = __data_end);
        PROVIDE (edata = __data_end);
    } >ram

    __data_source = LOADADDR(.data);
    __data_start = ADDR(.data);
    __data_size = SIZEOF(.data);

    .bss (NOLOAD) : {
        *(.dynbss)
        *(.bss .bss.* .gnu.linkonce.b.*)
        *(.sbss .sbss.* .scommon)
        *(COMMON)
        . = ALIGN(8);
    } >ram

    __bss_start = ADDR(.bss);
    __bss_size = SIZEOF(.bss);

    .got : {
        *(.got.plt)
        *(.got)
    } >ram

    PROVIDE( _end = . );
    PROVIDE( end = . );

    /DISCARD/ : {
        *(.eh_frame .eh_frame.*)
        *(.note .note.*)
        *(.comment)
        *(.plt)
    }
}
