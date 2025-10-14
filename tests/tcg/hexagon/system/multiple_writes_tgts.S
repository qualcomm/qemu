/*
 *  Copyright(c) 2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

.align 8
scratch_mem:
    .word 0x0

/*
 * A three-word pad guarantees that regardless of how large or
 * small the test packet is, or whether duplexing/compounding is
 * possible in the individual opcodes/operands of the test packet,
 * we will extend to or beyond the ELR_SKIP_BYTES.
 */
.macro PACKET_PAD
    nop
    nop
    nop
.endm

/*
 * Executes a packet that can be statically determined
 * to write multiple times to the same register.
 */
.global multiple_writes_static
multiple_writes_static:
    /*
     *  The assembler would refuse to encode
     *  such a packet, so we use a manually-encoded
     *  one instead:
     *
     *  0: e0 42 02 d3  d30242e0 {  r1:0 = add(r3:2,r3:2)
     *  4: 01 c1 00 f3  f300c101    r1 = add(r0,r1) }
     */
    .word 0xd30242e0
    .word 0xf300c101
    PACKET_PAD
    jumpr r31

/*
 * Executes a packet that writes multiple times
 * to the same register among predicated and
 * non-predicated instructions.
 */
.global multiple_writes_mixed
multiple_writes_mixed:
    /*
     *  The assembler would refuse to encode
     *  such a packet, so we use a manually-encoded
     *  one instead:
     *
     *  0: 21 40 20 7e  7e204021 { if (p1) r1 = #1
     *  4: 21 40 a0 7e  7ea04021   if (!p1) r1 = #1
     *  8: 21 c0 00 78  7800c021   r1 = #1 }
     */
    .word 0x7e204021
    .word 0x7ea04021
    .word 0x7800c021
    PACKET_PAD
    jumpr r31


.macro SetTrue To
    \To = cmp.eq(r0, r0)
.endm

/*
 * Executes a packet that writes multiple times
 * to the same register and performs a store.
 */
.global multiple_writes
multiple_writes:
    r4 = #0xab00ab00 /* initial value */
    r5 = ##scratch_mem
    memw(r5) = r4
    r6 = #0x00fefe00 /* new value */

    SetTrue p1
    SetTrue p2
    {
         if (p1) r3 = #1
         if (p2) r3:2 = r1:0
         memw(r5) = r6
    }
    PACKET_PAD

    r5 = memw(r5)
    p0 = cmp.eq(r4, r5)
    r0 = p0
    jumpr r31

.global multiple_write_legal
multiple_write_legal:
    p0 = cmp.eq(r1, r1)
    {
        if (p0) r1 = add(r2,#0x0)
        if (!p0) r1 = sub(r2,r4)
        r0 = r1
    }
    PACKET_PAD
    jumpr r31
