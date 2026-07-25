/*
 * Copyright(c) 2022-2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "qemu/log.h"
#include "qemu/main-loop.h"
#include "cpu.h"
#include "cpu_helper.h"
#include "exec/cpu-interrupt.h"
#include "hex_interrupts.h"
#include "hexswi.h"
#include "hw/intc/l2vic.h"
#include "macros.h"
#include "sys_macros.h"
#include "system/cpus.h"

static bool hex_is_qualified_for_int(CPUHexagonState *env, int int_num);

static bool get_syscfg_gie(CPUHexagonState *env)
{
    target_ulong syscfg = arch_get_system_reg(env, HEX_SREG_SYSCFG);
    return GET_SYSCFG_FIELD(SYSCFG_GIE, syscfg);
}

static bool get_ssr_ex(CPUHexagonState *env)
{
    target_ulong ssr = arch_get_system_reg(env, HEX_SREG_SSR);
    return GET_SSR_FIELD(SSR_EX, ssr);
}

static bool get_ssr_ie(CPUHexagonState *env)
{
    target_ulong ssr = arch_get_system_reg(env, HEX_SREG_SSR);
    return GET_SSR_FIELD(SSR_IE, ssr);
}

/* Do these together so we only have to call hexagon_modify_ssr once */
static void set_ssr_ex_cause(CPUHexagonState *env, int ex, uint32_t cause)
{
    target_ulong old = arch_get_system_reg(env, HEX_SREG_SSR);
    SET_SYSTEM_FIELD(env, HEX_SREG_SSR, SSR_EX, ex);
    SET_SYSTEM_FIELD(env, HEX_SREG_SSR, SSR_CAUSE, cause);
    target_ulong new = arch_get_system_reg(env, HEX_SREG_SSR);
    hexagon_modify_ssr(env, new, old);
}

static bool get_iad_bit(CPUHexagonState *env, int int_num)
{
    target_ulong iad = arch_get_system_reg(env, HEX_SREG_IAD);
    return extract32(iad, int_num, 1);
}

static void set_iad_bit(CPUHexagonState *env, int int_num, int val)
{
    target_ulong iad = arch_get_system_reg(env, HEX_SREG_IAD);
    iad = deposit32(iad, int_num, 1, val);
    arch_set_system_reg(env, HEX_SREG_IAD, iad);
}

static uint32_t get_ipend(CPUHexagonState *env)
{
    return arch_get_system_reg(env, HEX_SREG_IPEND);
}

static inline bool get_ipend_bit(CPUHexagonState *env, int int_num)
{
    target_ulong ipend = arch_get_system_reg(env, HEX_SREG_IPEND);
    return extract32(ipend, int_num, 1);
}

static void clear_ipend(CPUHexagonState *env, uint32_t mask)
{
    target_ulong ipend = arch_get_system_reg(env, HEX_SREG_IPEND);
    ipend &= ~mask;
    arch_set_system_reg(env, HEX_SREG_IPEND, ipend);
}

static void set_ipend(CPUHexagonState *env, uint32_t mask)
{
    target_ulong ipend = arch_get_system_reg(env, HEX_SREG_IPEND);
    ipend |= mask;
    arch_set_system_reg(env, HEX_SREG_IPEND, ipend);
}

static void set_ipend_bit(CPUHexagonState *env, int int_num, int val)
{
    target_ulong ipend = arch_get_system_reg(env, HEX_SREG_IPEND);
    ipend = deposit32(ipend, int_num, 1, val);
    arch_set_system_reg(env, HEX_SREG_IPEND, ipend);
}


static bool get_imask_bit(CPUHexagonState *env, int int_num)
{
    target_ulong imask = arch_get_system_reg(env, HEX_SREG_IMASK);
    return extract32(imask, int_num, 1);
}

static uint32_t get_prio(CPUHexagonState *env)
{
    target_ulong stid = arch_get_system_reg(env, HEX_SREG_STID);
    return extract32(stid, reg_field_info[STID_PRIO].offset,
                     reg_field_info[STID_PRIO].width);
}

static void set_elr(CPUHexagonState *env, target_ulong val)
{
    arch_set_system_reg(env, HEX_SREG_ELR, val);
}

static bool get_schedcfgen(CPUHexagonState *env)
{
    target_ulong schedcfg = arch_get_system_reg(env, HEX_SREG_SCHEDCFG);
    return extract32(schedcfg, reg_field_info[SCHEDCFG_EN].offset,
                     reg_field_info[SCHEDCFG_EN].width);
}

static bool is_lowest_prio(CPUHexagonState *env, int int_num)
{
    uint32_t my_prio = get_prio(env);
    CPUState *cs;

    CPU_FOREACH(cs) {
        CPUHexagonState *hex_env = cpu_env(cs);
        if (!hex_is_qualified_for_int(hex_env, int_num)) {
            continue;
        }

        /* Note that lower values indicate *higher* priority */
        if (my_prio < get_prio(hex_env)) {
            return false;
        }
    }
    return true;
}

static bool hex_is_qualified_for_int(CPUHexagonState *env, int int_num)
{
    bool syscfg_gie = get_syscfg_gie(env);
    bool iad = get_iad_bit(env, int_num);
    bool ssr_ie = get_ssr_ie(env);
    bool ssr_ex = get_ssr_ex(env);
    bool imask = get_imask_bit(env, int_num);
    bool lock_waiting = (env->k0_lock_state == HEX_LOCK_WAITING) ||
                        (env->tlb_lock_state == HEX_LOCK_WAITING);

    return syscfg_gie && !iad && ssr_ie && !ssr_ex && !imask && !lock_waiting;
}

static void clear_pending_locks(CPUHexagonState *env)
{
    g_assert(bql_locked());
    if (env->k0_lock_state == HEX_LOCK_WAITING) {
        env->k0_lock_state = HEX_LOCK_UNLOCKED;
    }
    if (env->tlb_lock_state == HEX_LOCK_WAITING) {
        env->tlb_lock_state = HEX_LOCK_UNLOCKED;
    }
}

static bool should_not_exec(CPUHexagonState *env)
{
    return (get_exe_mode(env) == HEX_EXE_MODE_WAIT) ||
           (env->k0_lock_state == HEX_LOCK_WAITING) ||
           (env->tlb_lock_state == HEX_LOCK_WAITING);
}

static void restore_state(CPUHexagonState *env, bool int_accepted)
{
    CPUState *cs = env_cpu(env);
    cpu_reset_interrupt(cs, CPU_INTERRUPT_HARD | CPU_INTERRUPT_SWI);
    if (!int_accepted && should_not_exec(env)) {
        cpu_interrupt(cs, CPU_INTERRUPT_HALT);
    }
}

static bool int_should_dtg(CPUHexagonState *env, int int_num)
{
    uint32_t ccr;

    ccr = arch_get_system_reg(env, HEX_SREG_CCR);

    switch (int_num) {
    case 3:
        if (!GET_FIELD(CCR_VV1, ccr)) {
            return false;
        }
        break;
    case 4:
        if (!GET_FIELD(CCR_VV2, ccr)) {
            return false;
        }
        break;
    case 5:
        if (!GET_FIELD(CCR_VV3, ccr)) {
            return false;
        }
        break;
    default:
        return false;
    }

    return !!GET_FIELD(CCR_GIE, ccr);
}

static void hex_accept_int(CPUHexagonState *env, int int_num)
{
    CPUState *cs = env_cpu(env);
    const int exe_mode = get_exe_mode(env);
    const bool in_wait_mode = exe_mode == HEX_EXE_MODE_WAIT;
    target_ulong elr;

    set_ipend_bit(env, int_num, 0);
    set_iad_bit(env, int_num, 1);

    cs->exception_index = HEX_EVENT_INT0 + int_num;
    env->cause_code = HEX_EVENT_INT0 + int_num;
    clear_pending_locks(env);

    if (in_wait_mode) {
        qemu_log_mask(CPU_LOG_INT,
            "%s: thread " TARGET_FMT_ld " resuming, exiting WAIT mode\n",
            __func__, env->threadId);
        elr = env->wait_next_pc;
        clear_wait_mode(env);
        cs->halted = false;
    } else if (env->k0_lock_state == HEX_LOCK_WAITING) {
        g_assert_not_reached();
    } else {
        elr = env->gpr[HEX_REG_PC];
    }

    if (int_should_dtg(env, int_num)) {
        HexagonCPU *cpu = env_archcpu(env);
        int vic_group = int_num - 2;
        uint32_t vid_packed = l2vic_read_vid(cpu->l2vic, vic_group / 2);
        uint32_t vid = extract32(vid_packed,
                                 (vic_group & 1) ? 16 : 0, 16);
        guest_event_entry(env, vid, elr, HEX_EVENT_INT0, false);
    } else {
        target_ulong evb = arch_get_system_reg(env, HEX_SREG_EVB);
        set_ssr_ex_cause(env, 1, HEX_CAUSE_INT0 | int_num);
        set_elr(env, elr);
        env->gpr[HEX_REG_PC] = evb | (cs->exception_index << 2);
    }

    if (get_ipend(env) == 0) {
        restore_state(env, true);
    }
}

bool hex_check_interrupts(CPUHexagonState *env)
{
    CPUState *cs = env_cpu(env);
    bool int_handled = false;
    bool ssr_ex;
    int max_ints = 32;
    bool schedcfgen;

    BQL_LOCK_GUARD();

    ssr_ex = get_ssr_ex(env);

    /* Early exit if nothing pending */
    if (get_ipend(env) == 0) {
        restore_state(env, false);
        return false;
    }
    /* Only check priorities when schedcfgen is set */
    schedcfgen = get_schedcfgen(env);
    for (int i = 0; i < max_ints; i++) {
        if (!get_iad_bit(env, i) && get_ipend_bit(env, i)) {
            qemu_log_mask(CPU_LOG_INT,
                          "%s: thread[" TARGET_FMT_ld "] pc = 0x"
                          TARGET_FMT_lx " found int %d\n", __func__,
                          env->threadId, env->gpr[HEX_REG_PC], i);
            if (hex_is_qualified_for_int(env, i) &&
                (!schedcfgen || is_lowest_prio(env, i))) {
                qemu_log_mask(CPU_LOG_INT,
                              "%s: thread[" TARGET_FMT_ld
                              "] int %d handled\n",
                              __func__, env->threadId, i);
                hex_accept_int(env, i);
                int_handled = true;
                break;
            }

            /*
             * A thread in WAIT mode exits when an interrupt is pending
             * that satisfies per-thread conditions (IE, IMASK, IAD),
             * regardless of the global interrupt enable (GIE).  The
             * interrupt is delivered normally (vectors to handler).
             */
            if (get_exe_mode(env) == HEX_EXE_MODE_WAIT &&
                !get_syscfg_gie(env) &&
                get_ssr_ie(env) && !get_imask_bit(env, i) && !ssr_ex) {
                qemu_log_mask(CPU_LOG_INT,
                              "%s: thread[" TARGET_FMT_ld
                              "] delivering int %d from WAIT (GIE=0)\n",
                              __func__, env->threadId, i);
                hex_accept_int(env, i);
                int_handled = true;
                break;
            }

            qemu_log_mask(CPU_LOG_INT,
                          "%s: thread[" TARGET_FMT_ld
                          "] int %d not qualified "
                          "GIE=%d iad=%d IE=%d EX=%d imask=%d "
                          "schedcfg_en=%d lowest=%d\n",
                          __func__, env->threadId, i,
                          (int)get_syscfg_gie(env),
                          (int)get_iad_bit(env, i),
                          (int)get_ssr_ie(env),
                          (int)ssr_ex,
                          (int)get_imask_bit(env, i),
                          schedcfgen,
                          (int)is_lowest_prio(env, i));
        }
    }

    if (!int_handled && !ssr_ex) {
        /*
         * A pending interrupt wakes a thread from WAIT mode even when
         * the interrupt cannot be dispatched (e.g. GIE=0).  The thread
         * resumes execution at the instruction after WAIT without
         * taking the exception.
         */
        if (get_exe_mode(env) == HEX_EXE_MODE_WAIT) {
            env->gpr[HEX_REG_PC] = env->wait_next_pc;
            clear_wait_mode(env);
            cs->halted = false;
        }

        /*
         * Keep CPU_INTERRUPT_SWI armed while IPEND has bits set.
         * In MTTCG, a temporary condition (GIE=0 during a scheduler
         * critical section) can block delivery.  By retaining the
         * interrupt request, the thread re-checks on each TB boundary
         * and delivers as soon as the condition clears (GIE=1).
         * Without this, all threads drop their interrupt_request while
         * GIE=0, causing permanent deadlock.
         *
         * A thread stalled on a lock (k0lock/tlblock) is the exception:
         * it cannot service interrupts and must stay halted until the
         * lock is released, so re-halt it via restore_state() regardless
         * of IPEND.  The lock-release path (hex_{k0,tlb}_unlock) re-raises
         * CPU_INTERRUPT_HARD if the woken thread still has pending
         * interrupts, so nothing is lost.
         */
        if (get_ipend(env) == 0 || should_not_exec(env)) {
            restore_state(env, false);
        }
    } else if (!int_handled && ssr_ex) {
        /*
         * Thread is in exception mode (EX=1) and cannot accept new
         * interrupts until IRET clears EX.  Clear the interrupt
         * request to avoid spinning; hex_interrupt_update will
         * re-arm it after IRET.
         */
        restore_state(env, false);
    } else if (int_handled) {
        assert(!cs->halted);
    }

    return int_handled;
}

void hex_clear_interrupts(CPUHexagonState *env, uint32_t mask, uint32_t type)
{
    if (mask == 0) {
        return;
    }

    /*
     * Notify all CPUs that the interrupt has happened
     */
    BQL_LOCK_GUARD();
    clear_ipend(env, mask);
    hex_interrupt_update(env);
}

void hex_raise_interrupts(CPUHexagonState *env, uint32_t mask, uint32_t type)
{
    g_assert(bql_locked());
    if (mask == 0) {
        return;
    }

    /*
     * Notify all CPUs that the interrupt has happened
     */
    set_ipend(env, mask);
    hex_interrupt_update(env);
}

void hex_interrupt_update(CPUHexagonState *env)
{
    CPUState *cs;

    g_assert(bql_locked());
    if (get_ipend(env) != 0) {
        CPU_FOREACH(cs) {
            CPUHexagonState *hex_env = cpu_env(cs);
            const int exe_mode = get_exe_mode(hex_env);
            if (exe_mode != HEX_EXE_MODE_OFF) {
                cpu_interrupt(cs, CPU_INTERRUPT_SWI);
            }
        }
    }
}

/*
 * Synchronously deliver pending interrupts to qualified threads.
 * Called before GIE transitions 1->0 to emulate hardware pipeline
 * behavior where in-flight interrupt delivery completes before
 * the GIE disable takes effect.
 *
 * Delivers one interrupt per qualified thread to match hardware
 * where each thread in the shared pipeline can have one in-flight
 * interrupt at the moment GIE is cleared.
 */
bool hex_deliver_pending(CPUHexagonState *env)
{
    CPUState *cs;
    bool schedcfgen;

    g_assert(bql_locked());

    if (get_ipend(env) == 0) {
        return true;
    }

    schedcfgen = get_schedcfgen(env);

    CPU_FOREACH(cs) {
        CPUHexagonState *hex_env = cpu_env(cs);
        const int exe_mode = get_exe_mode(hex_env);

        if (exe_mode == HEX_EXE_MODE_OFF) {
            continue;
        }

        for (int i = 0; i < 32; i++) {
            if (!get_ipend_bit(hex_env, i)) {
                continue;
            }
            if (get_iad_bit(hex_env, i)) {
                continue;
            }
            if (!hex_is_qualified_for_int(hex_env, i)) {
                continue;
            }
            if (schedcfgen && !is_lowest_prio(hex_env, i)) {
                continue;
            }
            hex_accept_int(hex_env, i);
            return true;
        }
    }

    return false;
}
