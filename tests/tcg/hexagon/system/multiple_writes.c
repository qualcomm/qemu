/*
 *  Copyright(c) 2022-2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "hexagon_standalone.h"
#define NO_DEFAULT_EVENT_HANDLES
#include "mmu.h"


#define HEX_CAUSE_REG_WRITE_CONFLICT 0x01d

/*
 * Test packets below are intended to trigger exceptions.  In order
 * to handle the exceptions uniformly, the test cases are padded to
 * four instruction words.
 */
static int ELR_SKIP_BYTES = 4 * sizeof(int32_t);

/*
 * A three-word pad guarantees that regardless of how large or
 * small the test packet is, or whether duplexing/compounding is
 * possible in the individual opcodes/operands of the test packet,
 * we will extend to or beyond the ELR_SKIP_BYTES.
 */
#define PACKET_PAD \
    "nop\n\t"      \
    "nop\n\t"      \
    "nop\n\t"

/*
 * Executes a packet that can be statically determined
 * to write multiple times to the same register.
 */
void multiple_writes_static(void)
{
    /*
     *  The assembler would refuse to encode
     *  such a packet, so we use a manually-encoded
     *  one instead:
     *
     *  0: e0 42 02 d3  d30242e0 {  r1:0 = add(r3:2,r3:2)
     *  4: 01 c1 00 f3  f300c101    r1 = add(r0,r1) }
     */
    asm volatile(".word 0xd30242e0\n\t"
                 ".word 0xf300c101\n\t"
                 PACKET_PAD
                 :
                 :
                 : "r1", "r0");
}

/*
 * Executes a packet that writes multiple times
 * to the same register among predicated and
 * non-predicated instructions.
 */
void multiple_writes_mixed(void)
{
    /*
     *  The assembler would refuse to encode
     *  such a packet, so we use a manually-encoded
     *  one instead:
     *
     *  0: 21 40 20 7e  7e204021 { if (p1) r1 = #1
     *  4: 21 40 a0 7e  7ea04021   if (!p1) r1 = #1
     *  8: 21 c0 00 78  7800c021   r1 = #1 }
     */
    asm volatile(".word 0x7e204021\n\t"
                 ".word 0x7ea04021\n\t"
                 ".word 0x7800c021\n\t"
                 PACKET_PAD
                 :
                 :
                 : "r1", "p1");
}

/*
 * Executes a packet that writes multiple times
 * to the same register and performs a store.
 */
void multiple_writes(void)
{
    static uint32_t init_val = 0xab00ab00;
    uint32_t var = init_val;
    asm volatile("p1 = cmp.eq(r1, r1)\n\t"
                 "p2 = p1\n\t"
                 "{\n\t"
                 "    if (p1) r3 = #1\n\t"
                 "    if (p2) r3:2 = r1:0\n\t"
                 "    memw(%0) = %1\n\t"
                 "}\n\t"
                 PACKET_PAD
                 "\n\t"
                 :
                 : "r"(&var), "r"(0x00fefe00)
                 : "r3", "r2", "p1", "p2", "memory");
    check32(var, init_val);
}

void my_err_handler_helper(uint32_t ssr)
{
    uint32_t cause = GET_FIELD(ssr, SSR_CAUSE);

    if (cause < 64) {
        *my_exceptions |= 1LL << cause;
    } else {
        *my_exceptions = cause;
    }

    switch (cause) {
    case HEX_CAUSE_REG_WRITE_CONFLICT:
        /* We don't want to replay this instruction, just note the exception */
        inc_elr(ELR_SKIP_BYTES);
        break;
    default:
        do_coredump();
        break;
    }
}

void multiple_write_legal()
{
    asm volatile("p0 = cmp.eq(r1, r1)\n\t"
                 "{ if (p0) r5 = add(r2,#0x0)\n\t"
                 "  if (!p0) r5 = sub(r2,r4)\n\t"
                 "  r0 = r5\n\t"
                 "}\n\t"
                 PACKET_PAD
                 :
                 :
                 : "p0", "r0", "r5");
}

MAKE_ERR_HANDLER(my_err_handler, my_err_handler_helper)

int main()
{
    puts("Hexagon multiple writes to the same reg test");

    multiple_write_legal();

    INSTALL_ERR_HANDLER(my_err_handler);

    multiple_writes_static();
    check32(*my_exceptions, 1 << HEX_CAUSE_REG_WRITE_CONFLICT);
    *my_exceptions &= ~(1 << HEX_CAUSE_REG_WRITE_CONFLICT);

    multiple_writes_mixed();
    check32(*my_exceptions, 1 << HEX_CAUSE_REG_WRITE_CONFLICT);
    *my_exceptions &= ~(1 << HEX_CAUSE_REG_WRITE_CONFLICT);

    multiple_writes();
    check32(*my_exceptions, 1 << HEX_CAUSE_REG_WRITE_CONFLICT);
    *my_exceptions &= ~(1 << HEX_CAUSE_REG_WRITE_CONFLICT);

    printf("%s\n", ((err) ? "FAIL" : "PASS"));
    return err;
}
