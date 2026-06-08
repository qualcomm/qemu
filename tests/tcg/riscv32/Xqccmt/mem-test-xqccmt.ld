/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Linker script for Xqccmt hand-written assembly tests.
 *
 * Identical to mem-test-page.ld except that .data is page-aligned to
 * start on the next 4 KB boundary after .text.  This ensures the writable
 * JVT table lives in a separate 4 KB page from the executable .text,
 * avoiding conflicting PT_LOAD permissions that would cause QEMU user-mode
 * to SIGSEGV when mapping the binary.
 */
SECTIONS
{
    . = 0x1000;
    .mem_test_section : {
        *(.mem_test_section)
    }

    .text : { *(.text) }

    /* page-align .data so it never shares a 4 KB page with .text (QEMU user-mode enforces per-page R/X vs R/W perms) */
    . = ALIGN(0x1000);
    .data : { *(.data) }
    .bss  : { *(.bss)  }
}
