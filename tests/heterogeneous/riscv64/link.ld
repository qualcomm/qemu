/* SPDX-License-Identifier: GPL-2.0-or-later */
ENTRY(_start)

SECTIONS
{
    /* riscv64 cpu reset address */
    . = 0x48000000;
    .text : { *(.text.boot) *(.text*) }
    .rodata : { *(.rodata*) *(.srodata*) }
    .data : { *(.data*) *(.sdata*) }
    .bss : { *(.bss*) *(.sbss*) *(COMMON) }
    . = ALIGN(16);
    . += 0x4000;
    __stack_top = .;
}
