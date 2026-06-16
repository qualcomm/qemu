/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Representative bare-metal system-emulation test for Hexagon.
 *
 * It exercises a property of the translator that is specific to system
 * emulation: a packet that writes certain system registers (imask, ssr,
 * ipendad, ...) ends the translation block, yet is *not* a change of flow.
 * The translator must therefore advance the PC to the next packet itself
 * (see need_next_PC() / gen_start_packet() in target/hexagon/translate.c).
 * If it does not, the first such packet re-executes forever and the program
 * hangs before producing any output.
 *
 * The test interleaves TB-ending system-register writes with plain
 * arithmetic in a loop and checks that the loop made the expected amount of
 * progress.  On success it writes "PASS\n" to the PL011 UART; on failure it
 * writes "FAIL\n".  A regressed translator produces no output at all (hang),
 * which the harness also treats as a failure.
 */

#include <stdint.h>

/* PL011 UART data register on the Hexagon virt and *-cdsp DSP machines. */
#define UART_DR ((volatile uint32_t *)0x10000000)

static void uart_puts(const char *s)
{
    while (*s) {
        *UART_DR = (uint8_t)(unsigned char)*s++;
    }
}

/*
 * Write @v to a system register that ends the translation block (these are
 * the registers in target/hexagon/translate.c:has_sreg_write_to_global()).
 * Each helper is its own packet and is not a change of flow.
 */
static inline void write_imask(uint32_t v)
{
    asm volatile("imask = %0\n\t" : : "r"(v));
}

static inline void write_stid(uint32_t v)
{
    asm volatile("stid = %0\n\t" : : "r"(v));
}

#define ITERS 64

int main(void)
{
    volatile int count = 0;

    /*
     * Two TB-ending system-register writes per iteration.  'count' only
     * reaches 2 * ITERS if execution advances correctly past every one of
     * them; a translator that fails to update the PC would spin on the
     * first write and never finish the loop.
     */
    for (int i = 0; i < ITERS; i++) {
        write_imask(0);
        count++;
        write_stid(0);
        count++;
    }

    if (count == 2 * ITERS) {
        uart_puts("PASS\n");
        return 0;
    }

    uart_puts("FAIL\n");
    return 1;
}
