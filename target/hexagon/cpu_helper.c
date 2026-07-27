/*
 * Copyright(c) 2019-2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "cpu.h"
#include "cpu_helper.h"
#include "system/cpus.h"
#ifdef CONFIG_USER_ONLY
#include "qemu.h"
#include "exec/helper-proto.h"
#else
#include "hw/core/boards.h"
#include "hw/hexagon/hexagon.h"
#include "hw/hexagon/hexagon_globalreg.h"
#include "hex_interrupts.h"
#include "hex_mmu.h"
#endif
#include "exec/cpu-interrupt.h"
#include "exec/target_page.h"
#include "accel/tcg/cpu-ldst.h"
#include "exec/cputlb.h"
#include "qemu/log.h"
#include "tcg/tcg-op.h"
#include "internal.h"
#include "macros.h"
#include "sys_macros.h"
#include "arch.h"
#include "pmu.h"


#ifndef CONFIG_USER_ONLY

static bool hexagon_read_memory_small(CPUHexagonState *env, target_ulong addr,
                                      int byte_count, unsigned char *dstbuf,
                                      int mmu_idx, uintptr_t retaddr)
 {
    /* handle small sizes */
    switch (byte_count) {
    case 1:
        *dstbuf = cpu_ldub_mmuidx_ra(env, addr, mmu_idx, retaddr);
        return true;

    case 2:
        if (QEMU_IS_ALIGNED(addr, 2)) {
            *(unsigned short *)dstbuf =
                cpu_lduw_mmuidx_ra(env, addr, mmu_idx, retaddr);
            return true;
        }
        break;

    case 4:
        if (QEMU_IS_ALIGNED(addr, 4)) {
            *(uint32_t *)dstbuf =
                cpu_ldl_mmuidx_ra(env, addr, mmu_idx, retaddr);
            return true;
        }
        break;

    case 8:
        if (QEMU_IS_ALIGNED(addr, 8)) {
            *(uint64_t *)dstbuf =
                cpu_ldq_mmuidx_ra(env, addr, mmu_idx, retaddr);
            return true;
        }
        break;

    default:
        /* larger request, handle elsewhere */
        return false;
    }

    /* not aligned, copy bytes */
    for (int i = 0; i < byte_count; ++i) {
        *dstbuf++ = cpu_ldub_mmuidx_ra(env, addr++, mmu_idx, retaddr);
    }
    return true;
}

void hexagon_read_memory(CPUHexagonState *env, target_ulong vaddr, int size,
                         void *retptr, uintptr_t retaddr)
{
    BQL_LOCK_GUARD();
    CPUState *cs = env_cpu(env);
    unsigned mmu_idx = cpu_mmu_index(cs, false);
    if (!hexagon_read_memory_small(env, vaddr, size, retptr, mmu_idx,
                                   retaddr)) {
        cpu_abort(cs, "%s: ERROR: bad size = %d!\n", __func__, size);
    }
}

static bool hexagon_write_memory_small(CPUHexagonState *env, target_ulong addr,
                                       int byte_count, unsigned char *srcbuf,
                                       int mmu_idx, uintptr_t retaddr)
{
    /* handle small sizes */
    switch (byte_count) {
    case 1:
        cpu_stb_mmuidx_ra(env, addr, *srcbuf, mmu_idx, retaddr);
        return true;

    case 2:
        if (QEMU_IS_ALIGNED(addr, 2)) {
            cpu_stw_mmuidx_ra(env, addr, *(uint16_t *)srcbuf, mmu_idx, retaddr);
            return true;
        }
        break;

    case 4:
        if (QEMU_IS_ALIGNED(addr, 4)) {
            cpu_stl_mmuidx_ra(env, addr, *(uint32_t *)srcbuf, mmu_idx, retaddr);
            return true;
        }
        break;

    case 8:
        if (QEMU_IS_ALIGNED(addr, 8)) {
            cpu_stq_mmuidx_ra(env, addr, *(uint64_t *)srcbuf, mmu_idx, retaddr);
            return true;
        }
        break;

    default:
        /* larger request, handle elsewhere */
        return false;
    }

    /* not aligned, copy bytes */
    for (int i = 0; i < byte_count; ++i) {
        cpu_stb_mmuidx_ra(env, addr++, *srcbuf++, mmu_idx, retaddr);
    }

    return true;
}

void hexagon_write_memory(CPUHexagonState *env, target_ulong vaddr,
                          int size, uint64_t data, uintptr_t retaddr)
{
    CPUState *cs = env_cpu(env);
    unsigned mmu_idx = cpu_mmu_index(cs, false);
    if (!hexagon_write_memory_small(env, vaddr, size, (unsigned char *)&data,
                                   mmu_idx, retaddr)) {
        cpu_abort(cs, "%s: ERROR: bad size = %d!\n", __func__, size);
    }
}

static inline uint32_t page_start(uint32_t addr)
{
    uint32_t page_align = ~(TARGET_PAGE_SIZE - 1);
    return addr & page_align;
}

void hexagon_touch_memory(CPUHexagonState *env, uint32_t start_addr,
                          uint32_t length, uintptr_t retaddr)
{
    unsigned int warm;
    uint32_t first = page_start(start_addr);
    uint32_t last = page_start(start_addr + length - 1);
    for (uint32_t page = first; page <= last; page += TARGET_PAGE_SIZE) {
        hexagon_read_memory(env, page, 1, &warm, retaddr);
    }
}

/*
 * Aggregate the raw event counter across all threads for a given event.
 * The PMU counters use a "synthesize on read" model: lightweight per-thread
 * counters are incremented during execution, and the full counter value is
 * computed on demand here.
 */
static uint32_t hexagon_get_pmu_event_stats(CPUHexagonState *env, int event)
{
    CPUState *cs;
    uint32_t total = 0;

    CPU_FOREACH(cs) {
        CPUHexagonState *e = cpu_env(cs);
        switch (event) {
        case COMMITTED_PKT_ANY:
            total += e->pmu.num_packets;
            break;
        case HVX_PKT:
            total += e->pmu.hvx_packets;
            break;
        default:
            if (event >= COMMITTED_PKT_T0 && event <= COMMITTED_PKT_T7) {
                int tid = event - COMMITTED_PKT_T0;
                if ((int)e->threadId == tid) {
                    total += e->pmu.num_packets;
                }
            }
            break;
        }
    }
    return total;
}

uint32_t hexagon_get_pmu_counter(CPUHexagonState *env, int index)
{
    uint8_t event;

    g_assert(index >= 0 && index < NUM_PMU_CTRS);

    if (!env->pmu.g_events || !env->pmu.g_ctrs_off) {
        return 0;
    }

    event = env->pmu.g_events[index];
    if (event == PMU_NO_EVENT) {
        return env->pmu.g_ctrs_off[index];
    }

    return env->pmu.g_ctrs_off[index] +
           hexagon_get_pmu_event_stats(env, event);
}

void hexagon_reset_pmu_event_stats(CPUHexagonState *env, int event)
{
    CPUState *cs;

    CPU_FOREACH(cs) {
        CPUHexagonState *e = cpu_env(cs);
        switch (event) {
        case COMMITTED_PKT_ANY:
            e->pmu.num_packets = 0;
            break;
        case HVX_PKT:
            e->pmu.hvx_packets = 0;
            break;
        default:
            if (event >= COMMITTED_PKT_T0 && event <= COMMITTED_PKT_T7) {
                int tid = event - COMMITTED_PKT_T0;
                if ((int)e->threadId == tid) {
                    e->pmu.num_packets = 0;
                }
            }
            break;
        }
    }
}

void hexagon_set_pmu_counter(CPUHexagonState *env, int index, uint32_t val)
{
    uint8_t event;

    g_assert(index >= 0 && index < NUM_PMU_CTRS);

    if (!env->pmu.g_events || !env->pmu.g_ctrs_off) {
        return;
    }

    event = env->pmu.g_events[index];
    if (event != PMU_NO_EVENT) {
        hexagon_reset_pmu_event_stats(env, event);
    }
    env->pmu.g_ctrs_off[index] = val;
}

uint32_t arch_get_system_reg(CPUHexagonState *env, uint32_t reg)
{
    if (reg == HEX_SREG_PCYCLELO) {
        return hexagon_get_sys_pcycle_count_low(env);
    } else if (reg == HEX_SREG_PCYCLEHI) {
        return hexagon_get_sys_pcycle_count_high(env);
    }

    g_assert(reg < NUM_SREGS);
    if (reg < HEX_SREG_GLB_START) {
        return env->t_sreg[reg];
    } else {
#ifndef CONFIG_USER_ONLY
        HexagonCPU *cpu = env_archcpu(env);
        return cpu->globalregs ?
               hexagon_globalreg_read(cpu->globalregs, reg) : 0;
#else
        return 0;
#endif
    }
}

void arch_set_system_reg(CPUHexagonState *env, uint32_t reg, uint32_t val)
{
    g_assert(reg < NUM_SREGS);
    if (reg < HEX_SREG_GLB_START) {
        env->t_sreg[reg] = val;
    } else {
#ifndef CONFIG_USER_ONLY
        HexagonCPU *cpu = env_archcpu(env);
        if (cpu->globalregs) {
            hexagon_globalreg_write(cpu->globalregs, reg, val);
        }
#endif
    }
}

void arch_set_system_reg_masked(CPUHexagonState *env, uint32_t reg,
                                 uint32_t val)
{
    g_assert(reg < NUM_SREGS);
    if (reg < HEX_SREG_GLB_START) {
        env->t_sreg[reg] = val;
    } else {
#ifndef CONFIG_USER_ONLY
        HexagonCPU *cpu = env_archcpu(env);
        if (cpu->globalregs) {
            hexagon_globalreg_write_masked(cpu->globalregs, reg, val);
        }
#endif
    }
}

/*
 * Sum the per-thread cycle counters without taking the BQL.
 *
 * The lock would not buy us a coherent snapshot anyway: the counters are
 * TCG globals bumped straight from generated code (see
 * gen_pcycle_counters()), which never takes the BQL.  What the relaxed
 * atomics do give us is well-defined, tear-free reads of each 64-bit
 * counter while its owning thread is running.
 *
 * The result stays monotonic despite the lock-free walk: every counter
 * only ever increases between explicit resets, so a later sum reads each
 * term at a value no smaller than an earlier sum did.
 */
uint64_t hexagon_get_sys_pcycle_count(CPUHexagonState *env)
{
    uint64_t cycles = 0;
    CPUState *cs;
    CPU_FOREACH(cs) {
        CPUHexagonState *thread_env = cpu_env(cs);
        cycles += qatomic_read(&thread_env->t_cycle_count);
    }
#ifndef CONFIG_USER_ONLY
    HexagonCPU *cpu = env_archcpu(env);
    uint64_t base = cpu->globalregs ?
                    hexagon_globalreg_get_pcycle_base(cpu->globalregs) : 0;
    return base + cycles;
#else
    return cycles;
#endif
}

uint32_t hexagon_get_sys_pcycle_count_high(CPUHexagonState *env)
{
    return hexagon_get_sys_pcycle_count(env) >> 32;
}

uint32_t hexagon_get_sys_pcycle_count_low(CPUHexagonState *env)
{
    return extract64(hexagon_get_sys_pcycle_count(env), 0, 32);
}

void hexagon_set_sys_pcycle_count_high(CPUHexagonState *env,
        uint32_t cycles_hi)
{
    uint64_t cur_cycles = hexagon_get_sys_pcycle_count(env);
    uint64_t cycles =
        ((uint64_t)cycles_hi << 32) | extract64(cur_cycles, 0, 32);
    hexagon_set_sys_pcycle_count(env, cycles);
}

void hexagon_set_sys_pcycle_count_low(CPUHexagonState *env,
        uint32_t cycles_lo)
{
    uint64_t cur_cycles = hexagon_get_sys_pcycle_count(env);
    uint64_t cycles = extract64(cur_cycles, 32, 32) | cycles_lo;
    hexagon_set_sys_pcycle_count(env, cycles);
}

void hexagon_set_sys_pcycle_count(CPUHexagonState *env, uint64_t cycles)
{
    BQL_LOCK_GUARD();
#ifndef CONFIG_USER_ONLY
    HexagonCPU *cpu = env_archcpu(env);
    if (cpu->globalregs) {
        hexagon_globalreg_set_pcycle_base(cpu->globalregs, cycles);
    }
#endif

    CPUState *cs;
    CPU_FOREACH(cs) {
        CPUHexagonState *thread_env = cpu_env(cs);
        qatomic_set(&thread_env->t_cycle_count, 0);
    }
}

static void set_wait_mode(CPUHexagonState *env)
{
    g_assert(bql_locked());

    const uint32_t modectl = arch_get_system_reg(env, HEX_SREG_MODECTL);
    uint32_t thread_wait_mask = GET_FIELD(MODECTL_W, modectl);
    thread_wait_mask |= 0x1 << env->threadId;
    SET_SYSTEM_FIELD(env, HEX_SREG_MODECTL, MODECTL_W, thread_wait_mask);
}

void hexagon_wait_thread(CPUHexagonState *env, target_ulong PC)
{
    g_assert(bql_locked());

    if (qemu_loglevel_mask(LOG_GUEST_ERROR) &&
        (env->k0_lock_state != HEX_LOCK_UNLOCKED ||
         env->tlb_lock_state != HEX_LOCK_UNLOCKED)) {
        qemu_log("WARNING: executing wait() with acquired lock"
                 "may lead to deadlock\n");
    }
    g_assert(get_exe_mode(env) != HEX_EXE_MODE_WAIT);

    CPUState *cs = env_cpu(env);
    /*
     * The addtion of cpu_has_work is borrowed from arm's wfi helper
     * and is critical for our stability
     */
    if ((cs->exception_index != HEX_EVENT_NONE) ||
        (cpu_has_work(cs))) {
        qemu_log_mask(CPU_LOG_INT,
            "%s: thread %d skipping WAIT mode, have some work\n",
            __func__, env->threadId);
        return;
    }
    set_wait_mode(env);
    env->wait_next_pc = PC + 4;

    /*
     * Before halting, check if any pending interrupts can wake us
     * immediately via the WAIT mode delivery path (GIE-bypass).
     * This handles the case where an interrupt arrived while the
     * thread was running with GIE=0 and couldn't be delivered,
     * but now in WAIT mode it can be.
     */
    if (arch_get_system_reg(env, HEX_SREG_IPEND) != 0) {
        cpu_interrupt(cs, CPU_INTERRUPT_SWI);
        return;
    }

    cpu_interrupt(cs, CPU_INTERRUPT_HALT);
}

static void hexagon_resume_thread(CPUHexagonState *env)
{
    CPUState *cs = env_cpu(env);
    clear_wait_mode(env);
    /*
     * The wait instruction keeps the PC pointing to itself
     * so that it has an opportunity to check for interrupts.
     *
     * When we come out of wait mode, adjust the PC to the
     * next executable instruction.
     */
    env->gpr[HEX_REG_PC] = env->wait_next_pc;
    cs->halted = false;
    cs->exception_index = HEX_EVENT_NONE;
    qemu_cpu_kick(cs);
}

void hexagon_resume_threads(CPUHexagonState *current_env, uint32_t mask)
{
    CPUState *cs;
    CPUHexagonState *env;

    g_assert(bql_locked());
    CPU_FOREACH(cs) {
        env = cpu_env(cs);
        g_assert(env->threadId < THREADS_MAX);
        if ((mask & (0x1 << env->threadId))) {
            if (get_exe_mode(env) == HEX_EXE_MODE_WAIT) {
                hexagon_resume_thread(env);
            }
        }
    }
}

static MMVector VRegs[VECTOR_UNIT_MAX][NUM_VREGS];
static MMQReg QRegs[VECTOR_UNIT_MAX][NUM_QREGS];

/*
 *                            EXT_CONTEXTS
 * SSR.XA   2              4              6              8
 * 000      HVX Context 0  HVX Context 0  HVX Context 0  HVX Context 0
 * 001      HVX Context 1  HVX Context 1  HVX Context 1  HVX Context 1
 * 010      HVX Context 0  HVX Context 2  HVX Context 2  HVX Context 2
 * 011      HVX Context 1  HVX Context 3  HVX Context 3  HVX Context 3
 * 100      HVX Context 0  HVX Context 0  HVX Context 4  HVX Context 4
 * 101      HVX Context 1  HVX Context 1  HVX Context 5  HVX Context 5
 * 110      HVX Context 0  HVX Context 2  HVX Context 2  HVX Context 6
 * 111      HVX Context 1  HVX Context 3  HVX Context 3  HVX Context 7
 */
static int parse_context_idx(CPUHexagonState *env, uint8_t XA)
{
    int ret;
    HexagonCPU *cpu = env_archcpu(env);
    if (cpu->hvx_contexts == 6 && XA >= 6) {
        ret = XA - 6 + 2;
    } else {
        ret = XA % cpu->hvx_contexts;
    }
    g_assert(ret >= 0 && ret < VECTOR_UNIT_MAX);
    return ret;
}

static void check_overcommitted_hvx(CPUHexagonState *env, uint32_t ssr)
{
    if (!GET_FIELD(SSR_XE, ssr)) {
        return;
    }

    uint8_t XA = GET_SSR_FIELD(SSR_XA, ssr);

    CPUState *cs;
    CPU_FOREACH(cs) {
        CPUHexagonState *thread_env = cpu_env(cs);
        if (thread_env == env) {
            continue;
        }
        /* Check if another thread has the XE bit set and same XA */
        uint32_t thread_ssr = arch_get_system_reg(thread_env, HEX_SREG_SSR);
        if (GET_SSR_FIELD(SSR_XE, thread_ssr) &&
            GET_FIELD(SSR_XA, thread_ssr) == XA) {
            qemu_log_mask(LOG_GUEST_ERROR,
                    "setting SSR.XA '%d' on thread %d but thread"
                    " %d has same extension active\n", XA, env->threadId,
                    thread_env->threadId);
        }
    }
}

void hexagon_modify_ssr(CPUHexagonState *env, uint32_t new, uint32_t old)
{
    g_assert(bql_locked());

    bool old_EX = GET_SSR_FIELD(SSR_EX, old);
    bool old_UM = GET_SSR_FIELD(SSR_UM, old);
    bool old_GM = GET_SSR_FIELD(SSR_GM, old);
    bool old_IE = GET_SSR_FIELD(SSR_IE, old);
    uint8_t old_XA = GET_SSR_FIELD(SSR_XA, old);
    bool new_EX = GET_SSR_FIELD(SSR_EX, new);
    bool new_UM = GET_SSR_FIELD(SSR_UM, new);
    bool new_GM = GET_SSR_FIELD(SSR_GM, new);
    bool new_IE = GET_SSR_FIELD(SSR_IE, new);
    uint8_t new_XA = GET_SSR_FIELD(SSR_XA, new);

    if ((old_EX != new_EX) ||
        (old_UM != new_UM) ||
        (old_GM != new_GM)) {
        hex_mmu_mode_change(env);
    }

    uint8_t old_asid = GET_SSR_FIELD(SSR_ASID, old);
    uint8_t new_asid = GET_SSR_FIELD(SSR_ASID, new);
    if (new_asid != old_asid) {
        CPUState *cs = env_cpu(env);
        tlb_flush(cs);
    }

    if (old_XA != new_XA) {
        int old_unit = parse_context_idx(env, old_XA);
        int new_unit = parse_context_idx(env, new_XA);

        check_overcommitted_hvx(env, new);

        /* Ownership exchange */
        memcpy(VRegs[old_unit], env->VRegs, sizeof(env->VRegs));
        memcpy(QRegs[old_unit], env->QRegs, sizeof(env->QRegs));
        memcpy(env->VRegs, VRegs[new_unit], sizeof(env->VRegs));
        memcpy(env->QRegs, QRegs[new_unit], sizeof(env->QRegs));
    }

    /* See if the interrupts have been enabled or we have exited EX mode */
    if ((new_IE && !old_IE) ||
        (!new_EX && old_EX)) {
        hex_interrupt_update(env);
    }
}

void clear_wait_mode(CPUHexagonState *env)
{
    g_assert(bql_locked());

    HexagonCPU *cpu = env_archcpu(env);
    if (cpu->globalregs) {
        const uint32_t modectl = arch_get_system_reg(env, HEX_SREG_MODECTL);
        uint32_t thread_wait_mask = GET_FIELD(MODECTL_W, modectl);
        thread_wait_mask &= ~(0x1 << env->threadId);
        SET_SYSTEM_FIELD(env, HEX_SREG_MODECTL, MODECTL_W, thread_wait_mask);
    }
}

void hexagon_ssr_set_cause(CPUHexagonState *env, uint32_t cause)
{
    g_assert(bql_locked());

    const uint32_t old = arch_get_system_reg(env, HEX_SREG_SSR);
    SET_SYSTEM_FIELD(env, HEX_SREG_SSR, SSR_EX, 1);
    SET_SYSTEM_FIELD(env, HEX_SREG_SSR, SSR_CAUSE, cause);
    const uint32_t new = arch_get_system_reg(env, HEX_SREG_SSR);

    hexagon_modify_ssr(env, new, old);
}


int get_exe_mode(CPUHexagonState *env)
{
    g_assert(bql_locked());

    target_ulong modectl = arch_get_system_reg(env, HEX_SREG_MODECTL);
    uint32_t thread_enabled_mask = GET_FIELD(MODECTL_E, modectl);
    bool E_bit = thread_enabled_mask & (0x1 << env->threadId);
    uint32_t thread_wait_mask = GET_FIELD(MODECTL_W, modectl);
    bool W_bit = thread_wait_mask & (0x1 << env->threadId);
    target_ulong isdbst = arch_get_system_reg(env, HEX_SREG_ISDBST);
    uint32_t debugmode = GET_FIELD(ISDBST_DEBUGMODE, isdbst);
    bool D_bit = debugmode & (0x1 << env->threadId);

    if (!D_bit && !W_bit && !E_bit) {
        return HEX_EXE_MODE_OFF;
    }
    if (!D_bit && !W_bit && E_bit) {
        return HEX_EXE_MODE_RUN;
    }
    if (!D_bit && W_bit && E_bit) {
        return HEX_EXE_MODE_WAIT;
    }
    if (D_bit && !W_bit && E_bit) {
        return HEX_EXE_MODE_DEBUG;
    }
    qemu_log("get_exe_mode: thread %u invalid state: "
             "D=%d W=%d E=%d modectl=0x%x isdbst=0x%x\n",
             env->threadId, D_bit, W_bit, E_bit,
             (uint32_t)modectl, (uint32_t)isdbst);
    g_assert_not_reached();
}

static void set_enable_mask(CPUHexagonState *env)
{
    g_assert(bql_locked());

    const uint32_t modectl = arch_get_system_reg(env, HEX_SREG_MODECTL);
    uint32_t thread_enabled_mask = GET_FIELD(MODECTL_E, modectl);
    thread_enabled_mask |= 0x1 << env->threadId;
    SET_SYSTEM_FIELD(env, HEX_SREG_MODECTL, MODECTL_E, thread_enabled_mask);
}

static uint32_t clear_enable_mask(CPUHexagonState *env)
{
    g_assert(bql_locked());

    const uint32_t modectl = arch_get_system_reg(env, HEX_SREG_MODECTL);
    uint32_t thread_enabled_mask = GET_FIELD(MODECTL_E, modectl);
    thread_enabled_mask &= ~(0x1 << env->threadId);
    SET_SYSTEM_FIELD(env, HEX_SREG_MODECTL, MODECTL_E, thread_enabled_mask);
    return thread_enabled_mask;
}
static void do_start_thread(CPUState *cs, run_on_cpu_data tbd)
{
    BQL_LOCK_GUARD();

    CPUHexagonState *env = cpu_env(cs);

    hexagon_cpu_soft_reset(env);

    set_enable_mask(env);

    cs->halted = 0;
    cs->exception_index = HEX_EVENT_NONE;
    cpu_resume(cs);
}

void hexagon_start_threads(CPUHexagonState *current_env, uint32_t mask)
{
    CPUState *cs;
    CPU_FOREACH(cs) {
        CPUHexagonState *env = cpu_env(cs);
        if (!(mask & (0x1 << env->threadId))) {
            continue;
        }

        if (current_env->threadId != env->threadId) {
            async_safe_run_on_cpu(cs, do_start_thread, RUN_ON_CPU_NULL);
        }
    }
}

/*
 * When we have all threads stopped, the return
 * value to the shell is register 2 from thread 0.
 */
static target_ulong get_thread0_r2(void)
{
    CPUState *cs;
    CPU_FOREACH(cs) {
        CPUHexagonState *thread = cpu_env(cs);
        if (thread->threadId == 0) {
            return thread->gpr[2];
        }
    }
    g_assert_not_reached();
}

void hexagon_stop_thread(CPUHexagonState *env)

{
    BQL_LOCK_GUARD();

    clear_wait_mode(env);
    uint32_t thread_enabled_mask = clear_enable_mask(env);
    CPUState *cs = env_cpu(env);
    cpu_interrupt(cs, CPU_INTERRUPT_HALT);
    if (!thread_enabled_mask) {
        /* All threads are stopped, exit */
        exit(get_thread0_r2());
    }
}

static int sys_in_monitor_mode_ssr(uint32_t ssr)
{
    if ((GET_SSR_FIELD(SSR_EX, ssr) != 0) ||
       ((GET_SSR_FIELD(SSR_EX, ssr) == 0) && (GET_SSR_FIELD(SSR_UM, ssr) == 0)))
        return 1;
    return 0;
}

static int sys_in_guest_mode_ssr(uint32_t ssr)
{
    if ((GET_SSR_FIELD(SSR_EX, ssr) == 0) &&
        (GET_SSR_FIELD(SSR_UM, ssr) != 0) &&
        (GET_SSR_FIELD(SSR_GM, ssr) != 0))
        return 1;
    return 0;
}

int sys_in_guest_mode(CPUHexagonState *env)
{
    uint32_t ssr = arch_get_system_reg(env, HEX_SREG_SSR);
    return sys_in_guest_mode_ssr(ssr);
}

static int sys_in_user_mode_ssr(uint32_t ssr)
{
    if ((GET_SSR_FIELD(SSR_EX, ssr) == 0) &&
        (GET_SSR_FIELD(SSR_UM, ssr) != 0) &&
        (GET_SSR_FIELD(SSR_GM, ssr) == 0))
        return 1;
   return 0;
}

int get_cpu_mode(CPUHexagonState *env)

{
    uint32_t ssr = arch_get_system_reg(env, HEX_SREG_SSR);

    if (sys_in_monitor_mode_ssr(ssr)) {
        return HEX_CPU_MODE_MONITOR;
    } else if (sys_in_guest_mode_ssr(ssr)) {
        return HEX_CPU_MODE_GUEST;
    } else if (sys_in_user_mode_ssr(ssr)) {
        return HEX_CPU_MODE_USER;
    }
    return HEX_CPU_MODE_MONITOR;
}

#endif
