/*
 * Hexagon Virtual Machine (HVM) interface, implemented by QEMU itself
 *
 * A guest kernel written for the Hexagon Virtual Machine does not need a
 * hypervisor binary: QEMU is the monitor.  The guest runs in guest mode
 * (SSR:UM and SSR:GM set), its own user programs in user mode, and every
 * event that a hypervisor would field arrives here instead: the virtual
 * instructions (trap1), the hardware configuration call (trap0 #31), page
 * table walks, exceptions and the virtual interrupts.
 *
 * The virtual instructions that the core executes itself in guest mode
 * (vmrte, vmsetie, vmgetie) never get this far; the rest of the interface
 * is in this file.  The state the virtual CPUs share is in
 * hw/hexagon/hexagon_hvm.c.
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "qemu/log.h"
#include "qemu/main-loop.h"
#include "qemu/bitops.h"
#include "cpu.h"
#include "internal.h"
#include "macros.h"
#include "sys_macros.h"
#include "cpu_helper.h"
#include "hex_mmu.h"
#include "hexswi.h"
#include "hvm.h"
#include "exec/cputlb.h"
#include "exec/cpu-interrupt.h"
#include "exec/cpu-common.h"
#include "exec/page-protection.h"
#include "exec/target_page.h"
#include "accel/tcg/cpu-loop.h"
#include "system/cpus.h"
#include "system/memory.h"
#include "hw/core/cpu.h"
#include "hw/hexagon/hexagon_globalreg.h"
#include "hw/hexagon/hexagon_hvm.h"

/* ---- address translation ---- */

/*
 * What a walk of the guest's page tables finds out about a page: the page
 * number it maps to, its size (log4 of the number of 4K pages) and its
 * permissions, as bits (X W R U).
 */
typedef struct HvmTrans {
    uint32_t pn;
    unsigned size;
    unsigned xwru;
} HvmTrans;

#define XWRU_U  1
#define XWRU_R  2
#define XWRU_W  4
#define XWRU_X  8

#define HVM_PAGE_BITS 12
#define HVM_ALL_MMU_IDX (BIT(MMU_USER_IDX) | BIT(MMU_GUEST_IDX) | \
                         BIT(MMU_KERNEL_IDX))
/* The size of a translation that covers the whole address space. */
#define HVM_SIZE_ALL ((32 - HVM_PAGE_BITS) / 2)
#define HVM_MAX_LINEAR_HOPS 4096
#define VARADIX_PN_BITS 22
#define VARADIX_MAX_LEAF_BITS 12

/* The low @n bits; unlike MAKE_64BIT_MASK(), @n may be zero. */
static inline uint32_t low_bits(unsigned n)
{
    return n >= 32 ? ~0u : (1u << n) - 1;
}

static bool hvm_read32(CPUState *cs, uint64_t pa, uint32_t *val)
{
    MemTxResult res;

    *val = address_space_ldl_le(cs->as, pa, MEMTXATTRS_UNSPECIFIED, &res);
    return res == MEMTX_OK;
}

static bool hvm_read64(CPUState *cs, uint64_t pa, uint64_t *val)
{
    MemTxResult res;

    *val = address_space_ldq_le(cs->as, pa, MEMTXATTRS_UNSPECIFIED, &res);
    return res == MEMTX_OK;
}

/* Offset: the physical page is a fixed number of pages from the virtual. */
static bool xlate_offset(const HexagonHVMMap *map, HvmTrans *t)
{
    uint32_t cfg = map->ptb;

    t->pn = (t->pn + (cfg >> 12)) & low_bits(20);
    t->size = MIN(t->size, extract32(cfg, 0, 4));
    t->xwru &= extract32(cfg, 8, 4);
    return true;
}

/* Linear: walk a list of 64-bit descriptors in guest memory. */
static bool xlate_linear(CPUState *cs, const HexagonHVMMap *map, HvmTrans *t)
{
    uint32_t list = map->ptb;
    uint32_t vpn = t->pn;

    for (unsigned hop = 0; hop < HVM_MAX_LINEAR_HOPS; hop++) {
        uint64_t raw;
        uint32_t lo, hi, mask;
        unsigned size;

        if (!hvm_read64(cs, list, &raw) || raw == 0) {
            return false;
        }
        lo = raw;
        hi = raw >> 32;
        if (extract32(hi, 31, 1)) {
            /* A chain entry: the list goes on elsewhere. */
            list = lo & ~7u;
            continue;
        }
        size = extract32(hi, 20, 4);
        mask = ~0u << (size * 2);
        if ((extract32(hi, 0, 20) & mask) == (vpn & mask)) {
            uint32_t page_mask;

            size = MIN(t->size, size);
            page_mask = (1u << (size * 2)) - 1;
            t->pn = (extract32(lo, 0, 24) & ~page_mask) | (vpn & page_mask);
            t->size = size;
            t->xwru &= extract32(lo, 28, 4);
            return true;
        }
        list += 8;
    }
    return false;
}

/*
 * Table: 4M L1 entries, which are leaves for 4M and 16M pages and
 * otherwise point at a table of the smaller pages.
 */
static bool xlate_table(CPUState *cs, const HexagonHVMMap *map, HvmTrans *t)
{
    uint32_t pn = t->pn;
    uint32_t pte;
    unsigned size;
    uint32_t mask;

    if (!hvm_read32(cs, (map->ptb & ~low_bits(HVM_PAGE_BITS)) |
                        ((pn >> 8) & 0xffc), &pte)) {
        return false;
    }
    size = pte & 7;
    if (size == HVM_PTE_PGSIZE_INVALID) {
        return false;
    }
    if (size <= HVM_PTE_PGSIZE_1M) {
        unsigned table_bits = 2 * (HVM_PTE_PGSIZE_4M - size);
        uint32_t index = (pn >> (2 * size)) & low_bits(table_bits);

        if (!hvm_read32(cs, (pte & ~0xfu) | (index << 2), &pte)) {
            return false;
        }
        /* The L1 entry, not the L2 entry, says how big the pages are. */
        pte = (pte & ~7u) | size;
    }
    size = MIN(t->size, size);
    mask = (1u << (size * 2)) - 1;
    t->pn = (pn & mask) | ((pte >> 12) & ~mask);
    t->size = size;
    t->xwru &= (extract32(pte, 9, 3) << 1) | extract32(pte, HVM_PTE_U_BIT, 1);
    return true;
}

/* Varadix: the position of an entry's lowest set bit says what it is. */
static bool varadix_valid_root(uint32_t root, unsigned vpn_bits)
{
    return vpn_bits <= VARADIX_PN_BITS && ctz32(root) <= vpn_bits;
}

static void varadix_leaf(uint32_t vpn, uint32_t entry, unsigned size,
                         HvmTrans *t)
{
    unsigned width = VARADIX_PN_BITS - size;
    uint32_t ppn = (entry >> (size + 1)) & low_bits(width);

    t->pn = (ppn << size) | (vpn & low_bits(size));
    t->size = MIN(size, VARADIX_MAX_LEAF_BITS) / 2;
    t->xwru = entry >> 28;
}

static bool xlate_varadix(CPUState *cs, const HexagonHVMMap *map, HvmTrans *t)
{
    unsigned vpn_bits = map->extra;
    uint32_t root = map->ptb;
    uint32_t vpn = t->pn;
    HvmTrans found;
    unsigned root_size = ctz32(root);

    if (!varadix_valid_root(root, vpn_bits) || (vpn >> vpn_bits) != 0) {
        return false;
    }
    if (root_size == vpn_bits) {
        varadix_leaf(vpn, root, root_size, &found);
    } else {
        uint32_t table = root;
        unsigned bits = root_size + 1;
        unsigned startbit = vpn_bits - (root_size + 1);

        for (;;) {
            uint32_t mask = low_bits(bits);
            uint32_t index = (vpn >> startbit) & mask;
            uint32_t entry;
            unsigned size;

            if (!hvm_read32(cs, (uint64_t)((table & ~mask) | index) << 2,
                            &entry)) {
                return false;
            }
            size = ctz32(entry);
            if (size > startbit) {
                return false;
            }
            if (size == startbit) {
                varadix_leaf(vpn, entry, size, &found);
                break;
            }
            table = entry;
            bits = size + 1;
            startbit -= size + 1;
        }
    }
    t->pn = found.pn;
    t->size = MIN(t->size, found.size);
    t->xwru &= found.xwru;
    return true;
}

static bool hvm_translate(CPUState *cs, const HexagonHVMMap *map,
                          uint32_t va, HvmTrans *t)
{
    bool ok;

    t->pn = va >> HVM_PAGE_BITS;
    t->size = HVM_SIZE_ALL;
    t->xwru = XWRU_X | XWRU_W | XWRU_R | XWRU_U;
    if (map->identity) {
        return true;
    }
    switch (map->type) {
    case HVM_TRANS_LINEAR:
        ok = xlate_linear(cs, map, t);
        break;
    case HVM_TRANS_TABLE:
        ok = xlate_table(cs, map, t);
        break;
    case HVM_TRANS_OFFSET:
        ok = xlate_offset(map, t);
        break;
    case HVM_TRANS_VARADIX:
        ok = xlate_varadix(cs, map, t);
        break;
    default:
        ok = false;
    }
    /* A mapping nobody may access is no mapping. */
    return ok && (t->xwru & (XWRU_X | XWRU_W | XWRU_R));
}

/*
 * Why an access is refused, as the cause the guest sees: the permission
 * the page lacks, or for user mode that it is not a user page.
 */
static int hvm_fault_cause(MMUAccessType access_type, bool user, bool user_page)
{
    switch (access_type) {
    case MMU_INST_FETCH:
        return user && !user_page ? HEX_CAUSE_FETCH_NO_UPAGE :
                                    HEX_CAUSE_FETCH_NO_XPAGE;
    case MMU_DATA_STORE:
        return user && !user_page ? HEX_CAUSE_PRIV_NO_UWRITE :
                                    HEX_CAUSE_PRIV_NO_WRITE;
    default:
        return user && !user_page ? HEX_CAUSE_PRIV_NO_UREAD :
                                    HEX_CAUSE_PRIV_NO_READ;
    }
}

/*
 * Find where @address is for an access of @access_type.  Returns the
 * permissions to map it with in @prot, or the cause of the fault in
 * @cause.
 */
static bool hvm_get_physical_address(CPUState *cs, uint32_t address,
                                     MMUAccessType access_type, int mmu_idx,
                                     hwaddr *phys, int *prot, int *cause)
{
    CPUHexagonState *env = cpu_env(cs);
    bool user = mmu_idx == MMU_USER_IDX;
    HvmTrans t;
    unsigned need = access_type == MMU_INST_FETCH ? XWRU_X :
                    access_type == MMU_DATA_STORE ? XWRU_W : XWRU_R;

    *prot = 0;
    if (!hvm_translate(cs, &env->hvm_map, address, &t)) {
        /* Nothing is mapped: a page fault named after the access. */
        *cause = hvm_fault_cause(access_type, false, true);
        return false;
    }
    if (user && !(t.xwru & XWRU_U)) {
        *cause = hvm_fault_cause(access_type, true, false);
        return false;
    }
    if (t.xwru & XWRU_X) {
        *prot |= PAGE_EXEC;
    }
    if (t.xwru & XWRU_R) {
        *prot |= PAGE_READ;
    }
    if (t.xwru & XWRU_W) {
        *prot |= PAGE_WRITE;
    }
    if (!(t.xwru & need)) {
        *cause = hvm_fault_cause(access_type, user, true);
        return false;
    }
    *phys = ((hwaddr)t.pn << HVM_PAGE_BITS) | (address & ~TARGET_PAGE_MASK);
    return true;
}

bool hexagon_hvm_tlb_fill(CPUState *cs, vaddr address, int size,
                          MMUAccessType access_type, int mmu_idx, bool probe,
                          uintptr_t retaddr)
{
    CPUHexagonState *env = cpu_env(cs);
    hwaddr phys;
    int prot;
    int cause = 0;

    if (hvm_get_physical_address(cs, address, access_type, mmu_idx, &phys,
                                 &prot, &cause)) {
        tlb_set_page(cs, address & TARGET_PAGE_MASK, phys & TARGET_PAGE_MASK,
                     prot, mmu_idx, TARGET_PAGE_SIZE);
        return true;
    }
    if (probe) {
        return false;
    }
    env->t_sreg[HEX_SREG_BADVA] = address;
    env->cause_code = cause;
    cs->exception_index = HEX_EVENT_PRECISE;
    do_raise_exception(env, cs->exception_index, env->gpr[HEX_REG_PC],
                       retaddr);
}

hwaddr hexagon_hvm_get_phys_addr_debug(CPUState *cs, vaddr addr)
{
    hwaddr phys;
    int prot, cause;

    if (hvm_get_physical_address(cs, addr, MMU_DATA_LOAD, MMU_GUEST_IDX,
                                 &phys, &prot, &cause)) {
        return phys;
    }
    return -1;
}

/* ---- events ---- */

static HexagonHVMState *hvm_of(CPUHexagonState *env)
{
    return env_archcpu(env)->hvm;
}

static bool guest_kernel(CPUHexagonState *env)
{
    return GET_FIELD(SSR_GM, env->t_sreg[HEX_SREG_SSR]);
}

static void hvm_set_r0(CPUHexagonState *env, uint32_t val)
{
    env->gpr[HEX_REG_R00] = val;
}

static void hvm_set_r1_r0(CPUHexagonState *env, uint64_t val)
{
    env->gpr[HEX_REG_R00] = val;
    env->gpr[HEX_REG_R01] = val >> 32;
}

/*
 * Deliver an event to the guest: record where it was and what happened in
 * its guest registers, and continue at its event vector, in kernel mode
 * with its interrupts off.
 */
static void hvm_event(CPUHexagonState *env, uint32_t gbadva, uint32_t cause,
                      uint32_t vec_offset, uint32_t elr)
{
    uint32_t gevb = env->t_sreg[HEX_SREG_GEVB];
    uint32_t old_ssr = env->t_sreg[HEX_SREG_SSR];
    uint32_t gsr = deposit32(0, reg_field_info[GSR_CAUSE].offset,
                             reg_field_info[GSR_CAUSE].width, cause);

    if (!gevb) {
        cpu_abort(env_cpu(env), "HVM: event 0x%" PRIx32 " at 0x%" PRIx32
                  " for a guest with no event vector\n", cause, elr);
    }
    if (!guest_kernel(env)) {
        uint32_t new_ssr = old_ssr;
        uint32_t sp = env->gpr[HEX_REG_SP];

        env->gpr[HEX_REG_SP] = env->greg[HEX_GREG_GOSP];
        env->greg[HEX_GREG_GOSP] = sp;
        fSET_FIELD(new_ssr, SSR_GM, 1);
        env->t_sreg[HEX_SREG_SSR] = new_ssr;
        hexagon_modify_ssr(env, new_ssr, old_ssr);
        gsr = deposit32(gsr, reg_field_info[GSR_UM].offset,
                        reg_field_info[GSR_UM].width, 1);
    }
    if (GET_FIELD(CCR_GIE, env->t_sreg[HEX_SREG_CCR])) {
        gsr = deposit32(gsr, reg_field_info[GSR_IE].offset,
                        reg_field_info[GSR_IE].width, 1);
        SET_SYSTEM_FIELD(env, HEX_SREG_CCR, CCR_GIE, 0);
    }
    env->greg[HEX_GREG_GSR] = gsr;
    env->greg[HEX_GREG_GELR] = elr;
    env->greg[HEX_GREG_GBADVA] = gbadva;
    env->gpr[HEX_REG_PC] = gevb + vec_offset;
}

static void hvm_error_event(CPUHexagonState *env, uint32_t gbadva,
                            uint32_t cause, uint32_t elr)
{
    hvm_event(env, gbadva, cause, HEX_GEVB_ERROR, elr);
}

/* ---- virtual interrupts ---- */

/* Whether the guest has interrupts on. */
static bool hvm_ie(CPUHexagonState *env)
{
    return GET_FIELD(CCR_GIE, env->t_sreg[HEX_SREG_CCR]);
}

void hexagon_hvm_irq_recheck(CPUHexagonState *env)
{
    HexagonHVMState *hvm = hvm_of(env);

    g_assert(bql_locked());
    if (hvm && hexagon_hvm_int_peek(hvm, env->threadId) >= 0) {
        cpu_interrupt(env_cpu(env), CPU_INTERRUPT_HARD);
    }
}

void hexagon_hvm_ie_enabled(CPUHexagonState *env)
{
    BQL_LOCK_GUARD();
    hexagon_hvm_irq_recheck(env);
}

bool hexagon_hvm_exec_interrupt(CPUState *cs, int interrupt_request)
{
    CPUHexagonState *env = cpu_env(cs);
    HexagonHVMState *hvm = hvm_of(env);
    unsigned cpu = env->threadId;
    bool woke = false;
    int32_t intno;

    if (!(interrupt_request & CPU_INTERRUPT_HARD)) {
        return false;
    }

    BQL_LOCK_GUARD();
    /*
     * Whatever this interrupt was about, it is looked at now.  If the guest
     * cannot take it yet, whoever lets it will raise this again.
     */
    cpu_reset_interrupt(cs, CPU_INTERRUPT_HARD);
    intno = hexagon_hvm_int_peek(hvm, cpu);
    if (intno < 0) {
        return false;
    }
    if (hvm->waiting & BIT(cpu)) {
        /* vmwait returns the interrupt number, taken or not. */
        hexagon_hvm_set_waiting(hvm, cpu, false);
        cs->halted = false;
        hvm_set_r0(env, intno);
        woke = true;
    }
    if (hvm_ie(env)) {
        intno = hexagon_hvm_int_get(hvm, cpu);
        hvm_event(env, 0, intno, HEX_GEVB_INTERRUPT, env->gpr[HEX_REG_PC]);
        return true;
    }
    return woke;
}

/* ---- virtual CPUs ---- */

typedef struct HvmStart {
    uint32_t pc;
    uint32_t sp;
    uint32_t usr;
    uint32_t ccr;
    uint32_t gevb;
    HexagonHVMMap map;
} HvmStart;

static void hvm_start_cpu(CPUState *cs, run_on_cpu_data data)
{
    CPUHexagonState *env = cpu_env(cs);
    HvmStart *start = data.host_ptr;

    BQL_LOCK_GUARD();
    hexagon_cpu_soft_reset(env);
    hexagon_hvm_cpu_reset(env);
    /* Nothing of what the CPU did before is left in it. */
    memset(env->gpr, 0, sizeof(env->gpr));
    memset(env->pred, 0, sizeof(env->pred));
    env->hvm_map = start->map;
    env->gpr[HEX_REG_PC] = start->pc;
    env->gpr[HEX_REG_SP] = start->sp;
    env->gpr[HEX_REG_USR] = start->usr;
    env->t_sreg[HEX_SREG_CCR] = start->ccr;
    env->t_sreg[HEX_SREG_GEVB] = start->gevb;
    /* It starts with its interrupts off. */
    SET_SYSTEM_FIELD(env, HEX_SREG_CCR, CCR_GIE, 0);
    g_free(start);

    hexagon_enable_thread(env);
    cs->halted = 0;
    cs->exception_index = HEX_EVENT_NONE;
    cpu_resume(cs);
}

/* vmstart: a new virtual CPU at @pc, sharing the caller's address space. */
static uint32_t hvm_start(CPUHexagonState *env, uint32_t pc, uint32_t sp)
{
    HexagonHVMState *hvm = hvm_of(env);
    HvmStart *start;
    int cpu;

    if ((pc & 3) || (sp & 7)) {
        return -1;
    }
    cpu = hexagon_hvm_alloc_cpu(hvm);
    if (cpu < 0) {
        return -1;
    }
    start = g_new0(HvmStart, 1);
    start->pc = pc;
    start->sp = sp;
    start->usr = env->gpr[HEX_REG_USR];
    start->ccr = env->t_sreg[HEX_SREG_CCR];
    start->gevb = env->t_sreg[HEX_SREG_GEVB];
    start->map = env->hvm_map;
    async_safe_run_on_cpu(hvm->cpus[cpu], hvm_start_cpu,
                          RUN_ON_CPU_HOST_PTR(start));
    return (HVM_VMIDX << 26) | (cpu << 10);
}

/* vmstop: the calling virtual CPU is done.  The last one takes the VM. */
static void hvm_stop(CPUHexagonState *env, uint32_t status)
{
    CPUState *cs = env_cpu(env);

    hexagon_hvm_cpu_stopped(hvm_of(env), env->threadId, status);
    hexagon_disable_thread(env);
    cpu_interrupt(cs, CPU_INTERRUPT_HALT);
}

/* vmwait: wait for an interrupt; with them off, just report one. */
static void hvm_wait(CPUHexagonState *env, uint32_t pc)
{
    HexagonHVMState *hvm = hvm_of(env);
    unsigned cpu = env->threadId;
    int32_t intno;

    if (hvm_ie(env)) {
        intno = hexagon_hvm_int_get(hvm, cpu);
        if (intno >= 0) {
            /* The guest sees this return value when its handler returns. */
            hvm_set_r0(env, intno);
            hvm_event(env, 0, intno, HEX_GEVB_INTERRUPT, pc);
            return;
        }
    } else {
        intno = hexagon_hvm_int_peek(hvm, cpu);
        if (intno >= 0) {
            hvm_set_r0(env, intno);
            return;
        }
    }
    hexagon_hvm_set_waiting(hvm, cpu, true);
    cpu_interrupt(env_cpu(env), CPU_INTERRUPT_HALT);
}

/* ---- address spaces ---- */

/* vmnewmap: switch the calling virtual CPU to another address space. */
static uint32_t hvm_newmap(CPUHexagonState *env, uint32_t ptb, uint32_t type,
                           uint32_t extra)
{
    HexagonHVMMap map = { .type = type, .ptb = ptb };

    switch (type) {
    case HVM_TRANS_LINEAR:
    case HVM_TRANS_TABLE:
    case HVM_TRANS_OFFSET:
        break;
    case HVM_TRANS_VARADIX:
        /* One that cannot translate anything fails before the swap. */
        if (!varadix_valid_root(ptb, extra)) {
            return -1;
        }
        map.extra = extra;
        break;
    default:
        return -1;
    }
    env->hvm_map = map;
    /* The old address space's translations are no good any more. */
    tlb_flush(env_cpu(env));
    return 0;
}

/* vmclrmap: the guest changed the translations of [va, va + bytes). */
static void hvm_clrmap(CPUHexagonState *env, uint32_t va, uint32_t bytes)
{
    CPUState *cs = env_cpu(env);
    uint64_t first = va >> HVM_PAGE_BITS;
    uint64_t last = ((uint64_t)va + bytes - 1) >> HVM_PAGE_BITS;

    if (bytes == 0) {
        return;
    }
    /* Every virtual CPU's TLB may hold what the guest changed. */
    tlb_flush_range_by_mmuidx_all_cpus_synced(cs, first << HVM_PAGE_BITS,
                                              (last - first + 1) <<
                                              HVM_PAGE_BITS, HVM_ALL_MMU_IDX,
                                              TARGET_VIRT_ADDR_SPACE_BITS);
}

/* ---- the rest of the virtual instructions ---- */

static uint32_t hvm_getinfo(CPUHexagonState *env, uint32_t code)
{
    HexagonCPU *cpu = env_archcpu(env);

    if (code == HVM_INFO_SYSCFG) {
        return hexagon_globalreg_read(cpu->globalregs, HEX_SREG_SYSCFG,
                                      env->threadId);
    }
    return hexagon_hvm_getinfo(cpu->hvm, code);
}

/* vmcache: caches are not modelled, and QEMU tracks code that was written. */
static uint32_t hvm_cache(uint32_t op)
{
    return op > HVM_CACHE_IDSYNC ? -1 : 0;
}

/*
 * vmpmuconfig: read or write one of the performance monitor's registers,
 * by the numbers the hypervisor gives them: the counters, then the event
 * and configuration registers.
 */
enum {
    PMUCTRL_SETREG = 1,
    PMUCTRL_GETREG = 2,
};

static int hvm_pmu_sreg(int32_t which)
{
    static const uint8_t cfg_regs[] = {
        HEX_SREG_PMUEVTCFG, HEX_SREG_PMUEVTCFG1, HEX_SREG_PMUCFG,
        HEX_SREG_PMUSTID0, HEX_SREG_PMUSTID1,
    };
    /* The counters are not numbered in order in the register file. */
    static const uint8_t cnt_regs[] = {
        HEX_SREG_PMUCNT0, HEX_SREG_PMUCNT1, HEX_SREG_PMUCNT2,
        HEX_SREG_PMUCNT3, HEX_SREG_PMUCNT4, HEX_SREG_PMUCNT5,
        HEX_SREG_PMUCNT6, HEX_SREG_PMUCNT7,
    };

    if (which >= 0 && which < ARRAY_SIZE(cnt_regs)) {
        return cnt_regs[which];
    }
    if (which >= 8 && which < 8 + ARRAY_SIZE(cfg_regs)) {
        return cfg_regs[which - 8];
    }
    return -1;
}

static uint32_t hvm_pmuconfig(CPUHexagonState *env, uint32_t op,
                              uint32_t which, uint32_t val)
{
    int reg = hvm_pmu_sreg(which);

    if (reg < 0) {
        return -1;
    }
    switch (op) {
    case PMUCTRL_SETREG:
        hexagon_pmu_sreg_write(env, reg, val);
        return 0;
    case PMUCTRL_GETREG:
        return hexagon_pmu_sreg_read(env, reg);
    default:
        return -1;
    }
}

static void hvm_trap1(CPUHexagonState *env, uint32_t imm, uint32_t pc)
{
    HexagonHVMState *hvm = hvm_of(env);
    unsigned cpu = env->threadId;
    uint32_t r0 = env->gpr[HEX_REG_R00];
    uint32_t r1 = env->gpr[HEX_REG_R01];
    uint32_t r2 = env->gpr[HEX_REG_R02];
    uint32_t r3 = env->gpr[HEX_REG_R03];
    uint64_t r1r0 = deposit64(r0, 32, 32, r1);
    uint64_t r3r2 = deposit64(r2, 32, 32, r3);
    uint64_t pktcount;

    /* The virtual instructions belong to the guest kernel. */
    if (!guest_kernel(env)) {
        hvm_error_event(env, 0, HVM_CAUSE_PRIVILEGE, pc);
        return;
    }
    env->gpr[HEX_REG_PC] = pc + 4;

    switch (imm) {
    case HVM_TRAP1_VMVERSION:
        hvm_set_r0(env, HVM_VERSION);
        break;
    case HVM_TRAP1_VMSETVEC:
        env->t_sreg[HEX_SREG_GEVB] = r0;
        hvm_set_r0(env, 0);
        break;
    case HVM_TRAP1_VMINTOP:
        hvm_set_r0(env, hexagon_hvm_intop(hvm, cpu, r0, r1, r2));
        break;
    case HVM_TRAP1_VMCLRMAP:
        hvm_clrmap(env, r0, r1);
        hvm_set_r0(env, 0);
        break;
    case HVM_TRAP1_VMNEWMAP:
        hvm_set_r0(env, hvm_newmap(env, r0, r1, r3));
        break;
    case HVM_TRAP1_VMCACHE:
        hvm_set_r0(env, hvm_cache(r0));
        break;
    case HVM_TRAP1_VMGETTIME:
        hvm_set_r1_r0(env, hexagon_hvm_gettime(hvm, &pktcount));
        env->gpr[HEX_REG_R02] = pktcount;
        env->gpr[HEX_REG_R03] = pktcount >> 32;
        break;
    case HVM_TRAP1_VMSETTIME:
        hexagon_hvm_settime(hvm, r1r0, r3r2);
        break;
    case HVM_TRAP1_VMWAIT:
        hvm_wait(env, pc + 4);
        break;
    case HVM_TRAP1_VMYIELD:
        /* Every virtual CPU has a hardware thread to itself. */
        hvm_set_r0(env, 0);
        break;
    case HVM_TRAP1_VMSTART:
        hvm_set_r0(env, hvm_start(env, r0, r1));
        break;
    case HVM_TRAP1_VMSTOP:
        hvm_stop(env, r0);
        break;
    case HVM_TRAP1_VMVPID:
        hvm_set_r0(env, (HVM_VMIDX << 26) | (cpu << 10));
        break;
    case HVM_TRAP1_VMSETREGS:
        env->greg[HEX_GREG_GELR] = r0;
        env->greg[HEX_GREG_GSR] = r1;
        env->greg[HEX_GREG_GOSP] = r2;
        env->greg[HEX_GREG_GBADVA] = r3;
        break;
    case HVM_TRAP1_VMGETREGS:
        hvm_set_r1_r0(env, deposit64(env->greg[HEX_GREG_GELR], 32, 32,
                                     env->greg[HEX_GREG_GSR]));
        env->gpr[HEX_REG_R02] = env->greg[HEX_GREG_GOSP];
        env->gpr[HEX_REG_R03] = env->greg[HEX_GREG_GBADVA];
        break;
    case HVM_TRAP1_VMTIMEROP:
        hvm_set_r1_r0(env, hexagon_hvm_timerop(hvm, cpu, r0, r3r2));
        break;
    case HVM_TRAP1_VMPMUCONFIG:
        hvm_set_r0(env, hvm_pmuconfig(env, r0, r2, r3));
        break;
    case HVM_TRAP1_VMGETINFO:
        hvm_set_r0(env, hvm_getinfo(env, r0));
        break;
    default:
        /* Not a virtual instruction: an error event, as from user mode. */
        hvm_error_event(env, 0, HVM_CAUSE_PRIVILEGE, pc);
        break;
    }
}

/* ---- trap0 #31: the hardware configuration call ---- */

enum {
    HWCONFIG_EXTBITS = 3,
    HWCONFIG_VLENGTH = 4,
    HWCONFIG_HWTHREADS_NUM = 13,
    HWCONFIG_HWTHREADS_MASK = 14,
    HWCONFIG_HMXBITS = 16,
    /* The configuration of the L2, its partitions and the like: ignored. */
    HWCONFIG_MAX = 34,
    HWCONFIG_HLXBITS = 32,
    HWCONFIG_COPROC_BITS = 33,
};

static uint32_t hvm_hwconfig(CPUHexagonState *env)
{
    HexagonCPU *cpu = env_archcpu(env);
    uint32_t op = env->gpr[HEX_REG_R00];
    uint32_t a = env->gpr[HEX_REG_R02];
    uint32_t b = env->gpr[HEX_REG_R03];
    uint32_t ssr = env->t_sreg[HEX_SREG_SSR];
    uint32_t ccr = env->t_sreg[HEX_SREG_CCR];
    uint32_t new;

    switch (op) {
    case HWCONFIG_EXTBITS:
        /* Coprocessor enable, and which extension context this CPU uses. */
        new = ssr;
        fSET_FIELD(new, SSR_XA, a);
        fSET_FIELD(new, SSR_XE, b & 1);
        env->t_sreg[HEX_SREG_SSR] = new;
        hexagon_modify_ssr(env, new, ssr);
        return 0;
    case HWCONFIG_VLENGTH: {
        /* Vector length as log2 bytes; above the native one is double. */
        uint32_t log_length = hexagon_hvm_getinfo(cpu->hvm,
                                                  HVM_INFO_HVX_VLENGTH);

        SET_SYSCFG_FIELD(env, SYSCFG_V2X, a > ctz32(log_length));
        return 0;
    }
    case HWCONFIG_HMXBITS:
        new = ssr;
        fSET_FIELD(new, SSR_XE2, a & 1);
        env->t_sreg[HEX_SREG_SSR] = new;
        hexagon_modify_ssr(env, new, ssr);
        return 0;
    case HWCONFIG_HLXBITS:
        new = ccr;
        fSET_FIELD(new, CCR_XA3, a);
        fSET_FIELD(new, CCR_XE3, b & 1);
        env->t_sreg[HEX_SREG_CCR] = new;
        hexagon_modify_ccr(env, new, ccr);
        return 0;
    case HWCONFIG_HWTHREADS_NUM:
        return ctpop32(hexagon_hvm_getinfo(cpu->hvm, HVM_INFO_HTHREADS));
    case HWCONFIG_HWTHREADS_MASK:
        return hexagon_hvm_getinfo(cpu->hvm, HVM_INFO_HTHREADS);
    default:
        return op < HWCONFIG_MAX ? 0 : -1;
    }
}

static void hvm_trap0(CPUHexagonState *env, uint32_t imm, uint32_t pc)
{
    if (imm == 0) {
        /*
         * A semihosting call, which QEMU takes from any mode as the
         * simulator does, and no event for the guest to ignore.
         */
        env->gpr[HEX_REG_PC] = pc;
        hexagon_semihost_trap0(env);
        env->gpr[HEX_REG_PC] = pc + 4;
    } else if (guest_kernel(env) && imm == HVM_TRAP0_HWCONFIG) {
        hvm_set_r0(env, hvm_hwconfig(env));
        env->gpr[HEX_REG_PC] = pc + 4;
    } else {
        hvm_event(env, 0, imm, HEX_GEVB_TRAP0, pc + 4);
    }
}

bool hexagon_hvm_do_interrupt(CPUState *cs)
{
    CPUHexagonState *env = cpu_env(cs);
    uint32_t pc = env->gpr[HEX_REG_PC];

    g_assert(bql_locked());
    switch (cs->exception_index) {
    case HEX_EVENT_TRAP0:
        hvm_trap0(env, env->cause_code, pc);
        break;
    case HEX_EVENT_TRAP1:
        hvm_trap1(env, env->cause_code, pc);
        break;
    case HEX_EVENT_PRECISE:
        hvm_error_event(env, env->t_sreg[HEX_SREG_BADVA], env->cause_code,
                        pc);
        break;
    case HEX_EVENT_IMPRECISE:
        qemu_log_mask(LOG_GUEST_ERROR,
                      "HVM: ignoring imprecise exception, cause 0x%x\n",
                      env->cause_code);
        break;
    case HEX_EVENT_FPTRAP:
    case HEX_EVENT_TLB_MISS_X:
    case HEX_EVENT_TLB_MISS_RW:
        /* Nothing in the guest's vector table is named for these. */
        hvm_error_event(env, env->t_sreg[HEX_SREG_BADVA], env->cause_code,
                        pc);
        break;
    case HEX_EVENT_DEBUG:
        qemu_log_mask(LOG_UNIMP, "HVM: single-step is not handled\n");
        break;
    default:
        return false;
    }
    cs->exception_index = HEX_EVENT_NONE;
    return true;
}

void hexagon_hvm_cpu_reset(CPUHexagonState *env)
{
    HexagonHVMState *hvm = hvm_of(env);
    uint32_t ssr = 0;

    /*
     * A guest kernel starts with its virtual interrupts off, in guest mode,
     * with the core handling the virtual instructions that it can.  The
     * core's own interrupt enable stays on: the guest's is virtual.
     */
    fSET_FIELD(ssr, SSR_UM, 1);
    fSET_FIELD(ssr, SSR_GM, 1);
    fSET_FIELD(ssr, SSR_IE, 1);
    env->t_sreg[HEX_SREG_SSR] = ssr;
    env->t_sreg[HEX_SREG_CCR] = 0;
    SET_SYSTEM_FIELD(env, HEX_SREG_CCR, CCR_GRE, 1);
    env->t_sreg[HEX_SREG_GEVB] = 0;
    env->hvm_map = (HexagonHVMMap){ .identity = true };
    if (env->threadId == 0) {
        env->gpr[HEX_REG_R00] = hexagon_hvm_getinfo(hvm, HVM_INFO_BOOT_R0);
        env->gpr[HEX_REG_R01] = hexagon_hvm_getinfo(hvm, HVM_INFO_BOOT_R1);
    }
}
