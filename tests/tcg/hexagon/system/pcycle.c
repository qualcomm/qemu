/*
 *  Copyright(c) 2019-2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>

#define SYSCFG_PCYCLEEN_BIT             6
#define SSR_CE_BIT                      23
#define SSR_PE_BIT                      24
#define MAX_HW_THREADS                  6

int err;
#include "../hex_test.h"


#define DECODE_CYCLES(cycles) cycles
#define DECODE_OPINFO(name, action) \
    static const int CYCLES_ ## name = action;
#include "../../../../target/hexagon/cycle_estimates.h.inc"

static inline void __check_range(uint32_t val, uint32_t min, uint32_t max, int line)
{
    if (val < min || val > max) {
        printf("ERROR at line %d: %" PRIu32 " not in [%" PRIu32 ", %" PRIu32 "]\n",
            line, val, min, max);
        err++;
    }
}

#define check_range(V, MIN, MAX) __check_range(V, MIN, MAX, __LINE__)

#define ENABLE_PCYCLE() do { \
        asm volatile("r2 = syscfg\n\t" \
                     "r2 = clrbit(r2, #%0)\n\t" \
                     "syscfg = r2\n\t" \
                     "isync\n\t" \
                     "r2 = setbit(r2, #%0)\n\t" \
                     "syscfg = r2\n\t" \
                     "isync\n\t" \
                     : \
                     : "i"(SYSCFG_PCYCLEEN_BIT) \
                     : "r2", "r3"); \
    } while (0)

static inline uint64_t combine(uint32_t hi, uint32_t low)
{
    return ((uint64_t)hi << 32) | low;
}

static void test_pcycle(void)
{
    uint64_t pcycle_0, pcycle_1;
    uint32_t pcyclelo_0, pcyclehi_0, pcyclelo_1, pcyclehi_1;

    ENABLE_PCYCLE();
    asm volatile("%0 = pcycle\n\t"
                 "%1 = pcyclehi\n\t"
                 "%2 = pcyclelo\n\t"
                 "r2 = add(r2, #1)\n\t"
                 "r2 = add(r2, #1)\n\t"
                 "r2 = add(r2, #1)\n\t"
                 "r2 = add(r2, #1)\n\t"
                 "r2 = add(r2, #1)\n\t"
                 "%3 = pcycle\n\t"
                 "%4 = pcyclehi\n\t"
                 "%5 = pcyclelo\n\t"
                 : "=r"(pcycle_0),  "=r"(pcyclehi_0),
                   "=r"(pcyclelo_0), "=r"(pcycle_1),
                   "=r"(pcyclehi_1), "=r"(pcyclelo_1)
                 :
                 : "r2", "r3");

    uint64_t cruft = (2 * CYCLES_Y2_tfrscrr) + CYCLES_Y4_tfrscpp;

    check64(pcycle_1 - pcycle_0,
            combine(pcyclehi_1, pcyclelo_1) - combine(pcyclehi_0, pcyclelo_0));
    check64((pcycle_1 - pcycle_0) - cruft, 5 * CYCLES_A2_addi);
}

#define MAX(a, b) ((a) > (b) ? (a) : (b))

/*
 * A bit more precise test, checking the calculations and considering
 * in-packet parallelism.
 */
static void test_pcycle2(void)
{
    uint64_t before, after;

    ENABLE_PCYCLE();
    asm volatile("%0 = pcycle\n\t"
                 "{\n\t"
                 "  p0 = tlbmatch(r1:0, r2)\n\t"
                 "  r2 = add(r0, r1)\n\t"
                 "}\n\t"
                 "r2 = add(r0, r1)\n\t"
                 "%1 = pcycle\n\t"
                 : "=r"(before),  "=r"(after)
                 :
                 : "p0", "r0", "r1", "r2");

    check64(after - before - CYCLES_Y4_tfrscpp,
            MAX(CYCLES_A2_add, CYCLES_A4_tlbmatch) + CYCLES_A2_add);
}

static void test_pcycle_read(void)
{
    uint64_t val0 = 0;
    uint64_t val1 = 0;

    asm volatile (
        "%0 = PCYCLE\n\t"
        "isync\n\t"
        "%1 = PCYCLE\n\t"
        : "=r"(val0), "=r"(val1)
    );

    check32_ne(val0, val1);

    val0 = 0;
    val1 = 0;

    asm volatile (
        "r0 = #0x52\n\t"
        "trap0(#0)\n\t"
        "%0 = r1:0\n\t"
        "r0 = #0x52\n\t"
        "trap0(#0)\n\t"
        "%1 = r1:0\n\t"
        : "=r"(val0), "=r"(val1)
        :: "r0", "r1"
    );

    check32_ne(val0, val1);
}

#define read_upcycle_regs(pcyclelo, pcyclehi, upcyclelo, upcyclehi) \
    asm volatile("%0 = pcyclelo\n\t" \
                 "%1 = pcyclehi\n\t" \
                 "%2 = upcyclelo\n\t" \
                 "%3 = upcyclehi\n\t" \
                 : "=r"(pcyclelo), "=r"(pcyclehi), \
                   "=r"(upcyclelo), "=r"(upcyclehi))

#define read_upcycle_reg_pair(pcycle, upcycle) \
    asm volatile("%0 = pcycle\n\t" \
                 "%1 = upcycle\n\t" \
                 : "=r"(pcycle), "=r"(upcycle)) \


#define read_gpcycle_regs(pcyclelo, pcyclehi, gpcyclelo, gpcyclehi) \
    asm volatile("%0 = pcyclelo\n\t" \
                 "%1 = pcyclehi\n\t" \
                 "%2 = gpcyclelo\n\t" \
                 "%3 = gpcyclehi\n\t" \
                 : "=r"(pcyclelo), "=r"(pcyclehi), \
                   "=r"(gpcyclelo), "=r"(gpcyclehi))

#define read_gpcycle_reg_pair(pcycle, gpcycle) \
    asm volatile("%0 = pcycle\n\t" \
                 "%1 = g25:24\n\t" \
                 : "=r"(pcycle), "=r"(gpcycle)) \

static void clr_ssr_ce(void)
{
    asm volatile("r2 = ssr\n\t"
                 "r2 = clrbit(r2, #%0)\n\t"
                 "ssr = r2\n\t"
                 : : "i"(SSR_CE_BIT));
}

static void set_ssr_ce(void)
{
    asm volatile("r2 = ssr\n\t"
                 "r2 = setbit(r2, #%0)\n\t"
                 "ssr = r2\n\t"
                 : : "i"(SSR_CE_BIT));
}

static void test_upcycle(void)
{
    uint32_t pcyclelo, pcyclehi;
    uint32_t upcyclelo, upcyclehi;
    uint64_t pcycle, upcycle;

    /*
     * Before SSR[CE] is set, upcycle registers should be zero
     */
    clr_ssr_ce();
    read_upcycle_regs(pcyclelo, pcyclehi, upcyclelo, upcyclehi);
    check32(upcyclelo, 0);
    check32(upcyclehi, 0);

    read_upcycle_reg_pair(pcycle, upcycle);
    check32(upcycle, 0);

    /*
     * After SSR[CE] is set, upcycle registers should match pcycle
     */
    set_ssr_ce();
    read_upcycle_regs(pcyclelo, pcyclehi, upcyclelo, upcyclehi);
    check_range(upcyclelo, pcyclelo, pcyclelo * MAX_HW_THREADS);
    check32(upcyclehi, pcyclehi);

    read_upcycle_reg_pair(pcycle, upcycle);
    check_range(upcycle, pcycle, pcycle * MAX_HW_THREADS);
}

static void test_gpcycle(void)
{
    uint32_t pcyclelo, pcyclehi;
    uint32_t gpcyclelo, gpcyclehi;
    uint64_t pcycle, gpcycle;

    /*
     * Before SSR[CE] is set, gpcycle registers should be zero
     */
    clr_ssr_ce();
    read_gpcycle_regs(pcyclelo, pcyclehi, gpcyclelo, gpcyclehi);
    check32(gpcyclelo, 0);
    check32(gpcyclehi, 0);

    read_gpcycle_reg_pair(pcycle, gpcycle);
    check32(gpcycle, 0);

    /*
     * After SSR[CE] is set, gpcycle registers should match pcycle
     */
    set_ssr_ce();
    read_gpcycle_regs(pcyclelo, pcyclehi, gpcyclelo, gpcyclehi);
    check_range(gpcyclelo, pcyclelo, pcyclelo * MAX_HW_THREADS);
    check32(gpcyclehi, pcyclehi);

    read_gpcycle_reg_pair(pcycle, gpcycle);
    check_range(gpcycle, pcycle, pcycle * MAX_HW_THREADS);
}

#define read_gcycle_xt_regs(gcycle_1t, gcycle_2t, gcycle_3t, \
                            gcycle_4t, gcycle_5t, gcycle_6t) \
    asm volatile("%0 = gpcycle1t\n\t" \
                 "%1 = gpcycle2t\n\t" \
                 "%2 = gpcycle3t\n\t" \
                 "%3 = gpcycle4t\n\t" \
                 "%4 = gpcycle5t\n\t" \
                 "%5 = gpcycle6t\n\t" \
                 : "=r"(gcycle_1t), "=r"(gcycle_2t), \
                   "=r"(gcycle_3t), "=r"(gcycle_4t), \
                   "=r"(gcycle_5t), "=r"(gcycle_6t))

static void clr_ssr_pe(void)
{
    asm volatile("r2 = ssr\n\t"
                 "r2 = clrbit(r2, #%0)\n\t"
                 "ssr = r2\n\t"
                 : : "i"(SSR_PE_BIT));
}

static void set_ssr_pe(void)
{
    asm volatile("r2 = ssr\n\t"
                 "r2 = setbit(r2, #%0)\n\t"
                 "ssr = r2\n\t"
                 : : "i"(SSR_PE_BIT));
}

static inline uint32_t read_pcyclelo(void)
{
    uint32_t ret;
    asm volatile("%0 = pcyclelo\n\t" : "=r"(ret));
    return ret;
}

static void test_gcycle_xt(void)
{
    uint32_t gcycle_1t, gcycle_2t, gcycle_3t,
             gcycle_4t, gcycle_5t, gcycle_6t;

    /*
     * Before SSR[PE] is set, gcycleXt registers should be zero
     */
    clr_ssr_pe();
    read_gcycle_xt_regs(gcycle_1t, gcycle_2t, gcycle_3t,
                        gcycle_4t, gcycle_5t, gcycle_6t);
    check32(gcycle_1t, 0);
    check32(gcycle_2t, 0);
    check32(gcycle_3t, 0);
    check32(gcycle_4t, 0);
    check32(gcycle_5t, 0);
    check32(gcycle_6t, 0);

    /*
     * After SSR[PE] is set, gcycleXt registers have real values
     */
    set_ssr_pe();
    read_gcycle_xt_regs(gcycle_1t, gcycle_2t, gcycle_3t,
                        gcycle_4t, gcycle_5t, gcycle_6t);
    /*
     * This is a single-threaded test, so only gcycle_1t should be non-zero
     * The range of values we check lets the test pass on hexagon-sim and
     * gives some ability for the underlying runtime code to change.
     */
    check_range(gcycle_1t, 1850, 7500);
    check32(gcycle_2t, 0);
    check32(gcycle_3t, 0);
    check32(gcycle_4t, 0);
    check32(gcycle_5t, 0);
    check32(gcycle_6t, 0);
}

int main()
{
    puts("Check the pcyclehi/pcyclelo counters in qemu");

    test_gcycle_xt();
    test_pcycle();
    test_pcycle2();
    test_pcycle_read();
    test_gpcycle();
    test_upcycle();

    printf("%s\n", ((err) ? "FAIL" : "PASS"));
    return err;
}
