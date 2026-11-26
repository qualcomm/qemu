/*
 * Linker script for self-map: the translation tables live in a 16KB region
 * at the start of RAM on the virt machine, everything else in a 16KB page
 * at RAM + 8MB + 16KB, whose level 3 descriptor is in the second 4KB of the
 * region.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

ENTRY(_start)

SECTIONS {
    . = 1 << 30;
    .region : {
        KEEP(*(.region))
    }
    . = (1 << 30) + 0x804000;
    .code : {
        __zero_region_start = .;
        *(.text.zero_region)
        __zero_region_end = .;
        *(.text .text.*)
        *(.rodata .rodata.*)
        *(.data .data.*)
        *(.got .got.*)
        *(.bss .bss.* COMMON)
    }
    /DISCARD/ : {
        *(.ARM.attributes)
        *(.note .note.*)
        *(.comment)
        *(.eh_frame .eh_frame_hdr)
    }
}

ASSERT(ADDR(.region) == 1 << 30, "region must start at 1GB")
ASSERT(SIZEOF(.region) <= 0x4000, "region does not fit in 16KB")
ASSERT(ADDR(.code) == (1 << 30) + 0x804000,
       "code must be at RAM + 8MB + 16KB")
ASSERT(SIZEOF(.code) <= 0x4000, "code does not fit in a 16KB page")
