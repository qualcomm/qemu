/*
 * Purpose: test pmpwy, a instruction that historically caused QEMU-hexagon
 * to overflow the TCG_MAX_TEMPS limit and fail with:
 * "tb_gen_code: Assertion `max_insns > 1' failed"
 */

    .text
    .globl _start

_start:
    r1 = #0xbabebeef
    r0 = #0xdeadbeef
    r1:0 = pmpyw(r1, r0)
    /* Reference from the simulator */
    {
        r2 = #0xf4955455
        r3 = #0x7e5ca3e1
    }
    {
        p0 = cmp.eq(r1:0, r3:2); if (p0.new) jump:t pass
        jump fail
    }
