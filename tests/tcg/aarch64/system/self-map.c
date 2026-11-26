/*
 * A single 16KB region, identity mapped by a single 16KB page, holding its
 * own translation tables.
 *
 * With a 16KB granule and a 31-bit VA (T0SZ = 33), a walk is level 2 then
 * level 3. A level 3 table is 16KB, so the region itself is used as the
 * level 3 table: the descriptor mapping the region is its first doubleword
 * (the L3 index of 1GB is 0).
 *
 * Code, data and stack are in a 16KB page outside the region, mapped by
 * another entry of the level 3 table. The level 2 table is in the region
 * too. The descriptors are written at runtime, before enabling the MMU,
 * each in its own 4KB chunk of the region. Once the MMU is enabled, the
 * whole region is zeroed through its own mapping, descriptors included.
 *
 * QEMU caches translations per 4KB target page: the first store to the
 * second 4KB chunk walks the zeroed tables, and the test expects a level 3
 * translation fault there, in zero_region().
 *
 * Runs at EL1 (-M virt, no EL2/EL3) and reports through semihosting.
 *
 * Copyright (c) 2026 Pierrick Bouvier
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <stdint.h>
#include <minilib.h>

/* from Linux's include/linux/stringify.h */
#define __stringify_1(x...) #x
#define __stringify(x...)   __stringify_1(x)

#define read_sysreg(r) ({                                           \
            uint64_t __val;                                         \
            asm volatile("mrs %0, " __stringify(r) : "=r" (__val)); \
            __val;                                                  \
})

#define write_sysreg(r, v) do {                     \
        uint64_t __val = (uint64_t)(v);             \
        asm volatile("msr " __stringify(r) ", %x0"  \
                 : : "rZ" (__val));                 \
} while (0)

#define isb() asm volatile("isb" : : : "memory")

/* Must match self-map.ld */
#define REGION_BASE     0x40000000UL
#define REGION_SIZE     0x4000UL
/*
 * QEMU indexes its TLB with the 4KB page number, modulo 64 at least: keep
 * the code page out of the slots used by the region, so that the code
 * translations survive the zeroing. Its L3 index is 513: its descriptor is
 * in the second 4KB chunk of the region, and its 4KB pages are 4 to 7,
 * modulo any power of 2 from 64 up to 2048.
 */
#define CODE_BASE       0x40804000UL
#define CHUNK_SIZE      0x1000UL
#define L2_TABLE        (REGION_BASE + 2 * CHUNK_SIZE)

/* Descriptors */
#define DESC_TABLE      3UL
#define DESC_PAGE       3UL
#define DESC_ATTRIDX0   (0UL << 2)
#define DESC_SH_INNER   (3UL << 8)
#define DESC_AF         (1UL << 10)

#define L2_INDEX(va)    (((va) >> 25) & 0x3f)
#define L3_INDEX(va)    (((va) >> 14) & 0x7ff)

/* TCR_EL1: 31-bit VA, 16KB granule, 32-bit PA, TTBR1 walks disabled */
#define TCR_T0SZ(x)     ((uint64_t)(x) << 0)
#define TCR_IRGN0_WBWA  (1UL << 8)
#define TCR_ORGN0_WBWA  (1UL << 10)
#define TCR_SH0_INNER   (3UL << 12)
#define TCR_TG0_16K     (2UL << 14)
#define TCR_EPD1        (1UL << 23)
#define TCR_TG1_4K      (2UL << 30)

#define MAIR_ATTR0_NORMAL_WB    0xffUL

#define SCTLR_M         (1UL << 0)
#define SCTLR_C         (1UL << 2)
#define SCTLR_I         (1UL << 12)

#define ESR_EC(esr)     (((esr) >> 26) & 0x3f)
#define ESR_EC_DABT_CUR 0x25
#define ESR_DFSC(esr)   ((esr) & 0x3f)
#define DFSC_TRANS_L3   0x07

#define SYS_WRITEC      0x03
#define SYS_EXIT        0x18
#define ADP_Stopped_ApplicationExit 0x20026

#define DESC_PAGE_RW(pa) ((pa) | DESC_AF | DESC_SH_INNER | DESC_ATTRIDX0 | \
                          DESC_PAGE)

/* Each descriptor is in its own 4KB chunk of the region: 0, 1 and 2 */
_Static_assert(L3_INDEX(REGION_BASE) * 8 / CHUNK_SIZE == 0, "");
_Static_assert(L3_INDEX(CODE_BASE) * 8 / CHUNK_SIZE == 1, "");
_Static_assert(L2_INDEX(CODE_BASE) == L2_INDEX(REGION_BASE), "");

/* The region, placed by self-map.ld. The whole region is the level 3 table */
uint64_t region[REGION_SIZE / sizeof(uint64_t)]
    __attribute__((section(".region"), aligned(REGION_SIZE)));

#define STACK_SIZE 4096
uint8_t stack[STACK_SIZE] __attribute__((aligned(16)));

/*
 * Code that cannot be C: the entry point (no stack yet), and the vector
 * table, whose entry turns the MMU off before using the stack, in case the
 * mapping is broken.
 */
asm(
    "    .text\n"
    "    .global _start\n"
    "_start:\n"
    "    adrp    x0, stack + " __stringify(STACK_SIZE) "\n"
    "    add     x0, x0, :lo12:stack + " __stringify(STACK_SIZE) "\n"
    "    mov     sp, x0\n"
    "    adr     x0, vectors\n"
    "    msr     vbar_el1, x0\n"
    "    isb\n"
    "    bl      main\n"
    "    bl      exit_test\n"
    "\n"
    "    .balign 2048\n"
    "    .global vectors\n"
    "vectors:\n"
    "    .rept   16\n"
    "    .balign 128\n"
    "    b       unexpected_entry\n"
    "    .endr\n"
    "\n"
    "unexpected_entry:\n"
    "    mrs     x0, sctlr_el1\n"
    "    bic     x0, x0, #1\n"                  /* SCTLR_M */
    "    msr     sctlr_el1, x0\n"
    "    isb\n"
    "    adrp    x0, stack + " __stringify(STACK_SIZE) "\n"
    "    add     x0, x0, :lo12:stack + " __stringify(STACK_SIZE) "\n"
    "    mov     sp, x0\n"
    "    mrs     x0, esr_el1\n"
    "    mrs     x1, elr_el1\n"
    "    mrs     x2, far_el1\n"
    "    bl      handle_exception\n"
);

/*
 * Address of a symbol defined by self-map.ld. adr is PC-relative, and the
 * code is identity mapped, so this works with the MMU on or off.
 */
#define symbol_addr(sym) ({                                   \
            uint64_t __addr;                                  \
            asm("adr %0, " __stringify(sym) : "=r" (__addr)); \
            __addr;                                           \
})

static void semihosting_call(uint64_t op, uint64_t arg)
{
    register uint64_t x0 asm("x0") = op;
    register uint64_t x1 asm("x1") = arg;

    asm volatile("hlt #0xf000" : "+r" (x0) : "r" (x1) : "memory");
}

static void disable_mmu(void)
{
    write_sysreg(sctlr_el1, read_sysreg(sctlr_el1) & ~SCTLR_M);
    isb();
}

/*
 * Semihosting reads the parameter block with a page walk, which fails once
 * the tables are zeroed: exit with the MMU off.
 */
void __attribute__((noreturn)) exit_test(int code)
{
    disable_mmu();

    uint64_t block[2] = { ADP_Stopped_ApplicationExit, code };

    semihosting_call(SYS_EXIT, (uint64_t)block);
    for (;;) {
    }
}

/* for minilib */
void __sys_outc(char c)
{
    semihosting_call(SYS_WRITEC, (uint64_t)&c);
}

/* Called with the MMU off */
void __attribute__((noreturn)) handle_exception(uint64_t esr, uint64_t elr,
                                                uint64_t far)
{
    if (ESR_EC(esr) == ESR_EC_DABT_CUR &&
        ESR_DFSC(esr) == DFSC_TRANS_L3 &&
        /* zero_region() is placed in its own section by self-map.ld */
        elr >= symbol_addr(__zero_region_start) &&
        elr < symbol_addr(__zero_region_end) &&
        far == REGION_BASE + CHUNK_SIZE) {
        ml_printf("PASS: expected translation fault in zero_region, "
                  "FAR 0x%lx\n", far);
        exit_test(0);
    }
    ml_printf("FAIL: unexpected exception, ESR 0x%lx ELR 0x%lx FAR 0x%lx\n",
              esr, elr, far);
    exit_test(1);
}

static void tlb_flush(void)
{
    asm volatile("dsb ishst; tlbi vmalle1; dsb ish; isb" : : : "memory");
}

/*
 * Write each descriptor once, with the MMU off. The other entries of the
 * level 2 table are never walked.
 */
static void write_tables(void)
{
    /* volatile: the tables are read by the page table walker, not by C */
    volatile uint64_t *l3_table = (volatile uint64_t *)REGION_BASE;
    /* volatile: same as above */
    volatile uint64_t *l2_table = (volatile uint64_t *)L2_TABLE;

    l3_table[L3_INDEX(REGION_BASE)] = DESC_PAGE_RW(REGION_BASE);
    l3_table[L3_INDEX(CODE_BASE)] = DESC_PAGE_RW(CODE_BASE);
    l2_table[L2_INDEX(REGION_BASE)] = REGION_BASE | DESC_TABLE;
}

/*
 * Write 0 to the first 64 bits of each 4KB chunk of the region, through
 * its own mapping. This zeroes the descriptor mapping the region, but the
 * translations stay in the TLB as long as it is not invalidated.
 */
static void __attribute__((noinline, section(".text.zero_region")))
zero_region(void)
{
    for (uint64_t off = 0; off < REGION_SIZE; off += CHUNK_SIZE) {
        /* volatile: every chunk must be written, in order */
        *(volatile uint64_t *)(REGION_BASE + off) = 0;
    }
}

static void enable_mmu(void)
{
    write_sysreg(mair_el1, MAIR_ATTR0_NORMAL_WB);
    write_sysreg(tcr_el1, TCR_T0SZ(33) | TCR_IRGN0_WBWA | TCR_ORGN0_WBWA |
                 TCR_SH0_INNER | TCR_TG0_16K | TCR_EPD1 | TCR_TG1_4K);
    write_sysreg(ttbr0_el1, L2_TABLE);
    isb();
    tlb_flush();
    write_sysreg(sctlr_el1,
                 read_sysreg(sctlr_el1) | SCTLR_M | SCTLR_C | SCTLR_I);
    isb();
}

int main(void)
{
    if (((read_sysreg(CurrentEL) >> 2) & 3) != 1) {
        ml_printf("FAIL: must start at EL1\n");
        return 1;
    }
    /* ID_AA64MMFR0_EL1.TGran16 != 0: 16KB granule supported */
    if (!((read_sysreg(id_aa64mmfr0_el1) >> 20) & 0xf)) {
        ml_printf("SKIP: no 16KB granule\n");
        return 0;
    }

    ml_printf("self-map: 16KB region mapped by a descriptor it contains\n");
    ml_printf("write tables\n");
    write_tables();
    ml_printf("enable MMU\n");
    enable_mmu();
    zero_region();
    /* The tables are zeroed: print with the MMU off */
    disable_mmu();
    ml_printf("FAIL: expected a translation fault in zero_region\n");
    return 1;
}
