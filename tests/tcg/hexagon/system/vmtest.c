
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    union {
        struct {
            uint32_t r0;
            uint32_t r1;
        };
        uint64_t r0100;
    };
    union {
        struct {
            uint32_t r2;
            uint32_t r3;
        };
        uint64_t r0302;
    };
    uint32_t gevb;
    uint32_t gelr;
    uint32_t gssr;
    uint32_t gosp;
    uint32_t gbadva;
    uint32_t vmstatus;
    uint64_t totalcycles;
    union {
        uint32_t raw;
    } id;
} VM_thread_context;

static void FAIL(const char *err)
{
    puts(err);
    exit(1);
}

#define VMINST(TRAP_NUM) asm volatile("trap1(#" # TRAP_NUM ")\n\t")

static void vmvpid(VM_thread_context *a)
{
    VMINST(0x11);
}
static void vmyield(VM_thread_context *a)
{
    VMINST(0x11);
}
static void vmgetie(VM_thread_context *a)
{
    VMINST(4);
}
static void vmsetregs(VM_thread_context *a)
{
    VMINST(0x15);
}
static void vmsetie(VM_thread_context *a)
{
    VMINST(3);
}
static void vmgettime(VM_thread_context *a)
{
    VMINST(0xe);
}
static void vmsettime(VM_thread_context *a)
{
    VMINST(0xf);
}
static void vmsetvec(VM_thread_context *a)
{
    VMINST(2);
}

static void vmversion(VM_thread_context *a)
{
    VMINST(0);
}

VM_thread_context a;

int main()
{
/*
    asm volatile(GLOBAL_REG_STR " = %0 " : : "r"(&test_kg));
*/
    uint32_t TH_expected_enable;
    uint32_t TH_expected_disable;
    uint32_t TH_enable_ret;
    uint32_t TH_disable_ret;
    uint32_t TH_expected_yield;


    /* VMVERSION */
    vmversion(&a);
    static const int EXPECTED_VERSION = 0x800;
    if ((a.r0) != EXPECTED_VERSION) {
        FAIL("vmversion");
    }

    /* VMSETVEC */
    a.r0 = 0xabcdef00;
    a.gevb = 0;
    vmsetvec(&a);
    if (a.r0 != 0) {
        FAIL("setvec/return");
    }
    if (a.gevb != 0xabcdef00U) {
        FAIL("setvec/vec");
    }

    /* VMSETIE */
    a.r0 = 0;
    TH_disable_ret = a.vmstatus = 0;
    TH_expected_disable = 1;
    vmsetie(&a);
    if (a.r0 != 0) {
        FAIL("setie/0/0/ret");
    }
    TH_expected_disable = 0; /* could be inlined */
    if (a.vmstatus != 0) {
        FAIL("setie/0/0/vmstatus");
    }

    a.r0 = 0;
    TH_disable_ret = 1;
    /* a.vmstatus = vmSTATUS_IE; */
    TH_expected_disable = 1;
    vmsetie(&a);
    if (a.r0 != 1) {
        FAIL("setie/0/1/ret");
    }
    TH_expected_disable = 0; /* could be inlined */
    if (a.vmstatus != 0) {
        FAIL("setie/0/1/vmstatus");
    }

    a.r0 = 1;
    TH_enable_ret = 1;
    /* a.vmstatus = vmSTATUS_IE; */
    TH_expected_enable = 1;
    vmsetie(&a);
    if (a.r0 != 1) {
        FAIL("setie/1/1/ret");
    }
    /* if (TH_expected_enable) { FAIL("Enable not called, maybe that's OK?"); }
     */
    TH_expected_enable = 0;
/*
    if (a.vmstatus != vmSTATUS_IE) {
        FAIL("setie/1/1/vmstatus");
    }
*/

    a.r0 = 1;
    TH_enable_ret = a.vmstatus = 0;
    TH_expected_enable = 1;
    vmsetie(&a);
    if (a.r0 != 0) {
        FAIL("setie/1/0/ret");
    }
    if (TH_expected_enable) {
        FAIL("Enable not called. Probably bad");
    }
    TH_expected_enable = 0;
/*
    if (a.vmstatus != vmSTATUS_IE) {
        FAIL("setie/1/0/vmstatus");
    }
*/


    /* VMGETIE */
    a.vmstatus = 0;
    vmgetie(&a);
    if (a.r0 != 0) {
        FAIL("getie/0");
    }

    a.vmstatus = 0xff;
    vmgetie(&a);
    if (a.r0 != 1) {
        FAIL("getie/f->1");
    }

    /* GETPCYCLES */
    a.totalcycles = 0x8000000000000000ULL;
    vmgettime(&a);
    if (a.r0100 <= 0x8000000000000000ULL) {
        FAIL("get totalcycles");
    }

    /* SETPCYCLES */
    a.r0100 = 0;
    vmsettime(&a);
    if (a.r0100 != 0x0ULL) {
        FAIL("set totalcycles");
    }

    /* YIELD */
    a.r0 = 0x123;
    TH_expected_yield = 1;
    vmyield(&a);
    if (a.r0 != 0) {
        FAIL("yield/ret");
    }
    if (TH_expected_yield) {
        FAIL("yield");
    }


#if 0
    /* STOP */
    a.r0 = 0x123;
    TH_expected_stop = 1;
    if (setjmp(env) == 0) {
        vmstop(&a);
    }
    if (TH_expected_stop) {
        FAIL("stop");
    }
#endif

    /* PID */
    a.id.raw = 0x1234abcd;
    vmvpid(&a);
    if (a.r0 != 0x1234abcd) {
        FAIL("vmvpid");
    }

    /* SETREGS */
    a.gelr = a.gbadva = a.gssr = a.gosp = 0xdeadbeef;
    a.r0100 = 0x0123456789abcdefULL;
    a.r0302 = 0xfedcba9876543210ULL;
    /* test_gregs_restore(&a); */
    vmsetregs(&a);
    /* test_gregs_save(&a); */
    if (a.gelr != 0x89abcdef) {
        FAIL("set/gelr");
    }
    if (a.gssr != 0x01234567) {
        FAIL("set/gssr");
    }
    if (a.gosp != 0x76543210) {
        FAIL("set/gosp");
    }
    if (a.gbadva != 0xfedcba98) {
        FAIL("set/gbadva");
    }

#if 0
    /* GETREGS */
    a.r0302 = a.r0100 = 0xdeadbeefdeadbeefULL;
    a.gbadva_gosp = 0x0123456789abcdefULL;
    a.gssr_gelr = 0xfedcba9876543210ULL;
    test_gregs_restore(&a);
    vmgetregs(&a);
    test_gregs_save(&a);
    if (a.r0302 != 0x0123456789abcdefULL) {
        FAIL("get/r3:2");
    }
    if (a.r0100 != 0xfedcba9876543210ULL) {
        FAIL("get/r1:0");
    }

    /* START */
    TH_expected_create = 1;
    a.r0 = 0x10203040;
    a.r1 = 0x50607080;
    a.base_prio = 4;
    a.vmblock = (void *)&a;
    TH_create_ret = 0x0a0b0c0d;
    vmstart(&a);
    if (a.r0 != 0x0a0b0c0d) {
        FAIL("start/ret");
    }

    /* WAIT */

    TH_expected_work = 1;
    TH_work_ret = 1;
    a.r0 = 0;
    vmwait(&a);
    if (TH_expected_work != 0) {
        FAIL("no work call");
    }
    if (a.r0 != 1) {
        FAIL("didn't return intno");
    }

    TH_expected_work = 1;
    TH_work_ret = -1;
    a.r0 = -1;
    a.vmblock = &av;
    av.waiting_cpus = 0;
    a.id.cpuidx = 5;
    test_kg.runlist[0] = &a;
    test_kg.runlist_prios[0] = 4;
    a.status = 0;
    TH_expected_dosched = 1;
    if (setjmp(env) == 0) {
        vmwait(&a);
    }
    if (TH_expected_dosched != 0) {
        FAIL("didn't dosched");
    }
    if (a.r0 != -1) {
        FAIL("r0 clobbered");
    }
    if (av.waiting_cpus != (1 << 5)) {
        FAIL("wrong waiting cpus");
    }
    if (a.status != test_STATUS_VMWAIT) {
        FAIL("wrong status");
    }
    if (test_kg.runlist[0] != NULL) {
        FAIL("runlist");
    }
    if (test_kg.runlist_prios[0] != -1) {
        FAIL("runlist_prios");
    }

    /* RETURN */

    a.gelr = 0xCCCCCCCC;
    a.gssr = 0x00000000;
    a.gosp = 0xdeadbeef;
    a.r29 = 0x44444444;
    test_set_elr(0xDEAD0000);
    a.r0100 = 0x1111222233334444ULL;
    a.ssr = ((1 << SSR_GUEST_BIT) | (1 << SSR_SS_BIT));
    test_gregs_restore(&a);
    vmreturn(&a);
    test_gregs_save(&a);
    if (test_get_elr() != 0xCCCCCCCC) {
        FAIL("return/elr");
    }
    if (a.r0100 != 0x1111222233334444ULL) {
        FAIL("return/r0100 clobbered");
    }
    if ((a.ssr & (1 << SSR_SS_BIT)) != 0) {
        FAIL("SS set");
    }
    if ((a.ssr & (1 << SSR_GUEST_BIT)) == 0) {
        FAIL("GUEST not set");
    }
    if (a.gosp != 0xdeadbeef) {
        FAIL("swapped sp");
    }
    if (a.r29 != 0x44444444) {
        FAIL("swapped sp");
    }

    a.gelr = 0xCCCCCCCC;
    a.gssr = 0xE0000000;
    a.gosp = 0xdeadbeef;
    a.r29 = 0x44444444;
    test_set_elr(0xDEAD0000);
    a.r0100 = 0x1111222233334444ULL;
    a.ssr = ((1 << SSR_GUEST_BIT) | (0 << SSR_SS_BIT));
    test_gregs_restore(&a);
    TH_expected_enable = 1;
    vmreturn(&a);
    test_gregs_save(&a);
    if (TH_expected_enable == 1) {
        FAIL("no enable call");
    }
    if (test_get_elr() != 0xCCCCCCCC) {
        FAIL("return/elr");
    }
    if (a.r0100 != 0x1111222233334444ULL) {
        FAIL("return/r0100 clobbered");
    }
    if ((a.ssr & (1 << SSR_SS_BIT)) == 0) {
        FAIL("SS not set");
    }
    if ((a.ssr & (1 << SSR_GUEST_BIT)) != 0) {
        FAIL("GUEST set");
    }
    if (a.r29 != 0xdeadbeef) {
        FAIL("didn't swap sp");
    }
    if (a.gosp != 0x44444444) {
        FAIL("didn't swap sp");
    }

#endif

    puts("TEST PASSED");
    return 0;
}
