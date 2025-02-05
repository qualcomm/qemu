SECTIONS
{
    . = 0x1000;
    .mem_test_section : {
        *(.mem_test_section)
    }

    .text : { *(.text) }
    .data : { *(.data) }
    .bss : { *(.bss) }
}
