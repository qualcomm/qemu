/* SPDX-License-Identifier: GPL-2.0-or-later */
ENTRY(_start)

SECTIONS
{
    /* aarch64 cpu reset address */
    . = 0x40000000;
    .text : { *(.text.boot) *(.text*) }
    .rodata : { *(.rodata*) }
    .data : { *(.data*) }
    .bss : { *(.bss*) *(COMMON) }
    . = ALIGN(16);
    . += 0x4000;
    __stack_top = .;
}
