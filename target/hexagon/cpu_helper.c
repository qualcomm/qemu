/*
 *  Copyright (c) Qualcomm Innovation Center, Inc. All Rights Reserved.
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "cpu.h"
#include "system/cpus.h"
#ifdef CONFIG_USER_ONLY
#include "qemu.h"
#include "exec/helper-proto.h"
#else
#include "hex_interrupts.h"
#include "hex_mmu.h"
#include "hw/core/boards.h"
#include "hw/hexagon/hexagon.h"
#include "hw/hexagon/hexagon_globalreg.h"
#endif
#include "accel/tcg/cpu-ldst.h"
#include "qemu/log.h"
#include "tcg/tcg-op.h"
#include "internal.h"
#include "macros.h"
#include "sys_macros.h"
#include "arch.h"
#include "fma_emu.h"
#include "mmvec/mmvec.h"
#include "mmvec/macros_auto.h"
#ifndef CONFIG_USER_ONLY
#include "hex_mmu.h"
#include "hex_interrupts.h"
#include "pmu.h"
#endif
#include "system/runstate.h"
#include "trace.h"

#ifndef CONFIG_USER_ONLY

uint64_t hexagon_get_sys_pcycle_count(CPUHexagonState *env)
{
    HexagonCPU *cpu = env_archcpu(env);
    uint64_t total = hexagon_globalreg_get_pcycle_base(cpu->globalregs);
    CPUState *cs;
    CPU_FOREACH(cs) {
        CPUHexagonState *thread_env = cpu_env(cs);
        total += thread_env->t_cycle_count;
    }
    return total;
}

uint32_t hexagon_get_sys_pcycle_count_high(CPUHexagonState *env)
{
    return (uint32_t)(hexagon_get_sys_pcycle_count(env) >> 32);
}

uint32_t hexagon_get_sys_pcycle_count_low(CPUHexagonState *env)
{
    return (uint32_t)(hexagon_get_sys_pcycle_count(env));
}

#define BYTES_LEFT_IN_PAGE(A) (TARGET_PAGE_SIZE - ((A) % TARGET_PAGE_SIZE))

static inline QEMU_ALWAYS_INLINE bool hexagon_read_memory_small(
    CPUHexagonState *env, target_ulong addr, int byte_count,
    unsigned char *dstbuf, int mmu_idx)

 {
    /* handle small sizes */
    switch (byte_count) {
    case 1:
        *dstbuf = cpu_ldub_mmuidx_ra(env, addr, mmu_idx, CPU_MEMOP_PC(env));
        return true;

    case 2:
        if (QEMU_IS_ALIGNED(addr, 2)) {
            *(unsigned short *)dstbuf =
                cpu_lduw_le_mmuidx_ra(env, addr, mmu_idx, CPU_MEMOP_PC(env));
            return true;
        }
        break;

    case 4:
        if (QEMU_IS_ALIGNED(addr, 4)) {
            *(uint32_t *)dstbuf =
                cpu_ldl_le_mmuidx_ra(env, addr, mmu_idx, CPU_MEMOP_PC(env));
            return true;
        }
        break;

    case 8:
        if (QEMU_IS_ALIGNED(addr, 8)) {
            *(uint64_t *)dstbuf =
                cpu_ldq_le_mmuidx_ra(env, addr, mmu_idx, CPU_MEMOP_PC(env));
            return true;
        }
        break;

    default:
        /* larger request, handle elsewhere */
        return false;
    }

    /* not aligned, copy bytes */
    for (int i = 0; i < byte_count; ++i) {
        *dstbuf++ = cpu_ldub_mmuidx_ra(env, addr++, mmu_idx, CPU_MEMOP_PC(env));
    }
    return true;
}

void hexagon_read_memory_block(CPUHexagonState *env, target_ulong addr,
    int byte_count, unsigned char *dstbuf)

 {
    unsigned mmu_idx = cpu_mmu_index(env_cpu(env), false);

    /* handle small sizes */
    if (hexagon_read_memory_small(env,
        addr, byte_count, dstbuf, mmu_idx) == true) {
        return;
    }

    /* handle larger sizes here */
    unsigned bytes_left_in_page = BYTES_LEFT_IN_PAGE(addr);
    while (byte_count > 0) {
        unsigned copy_byte_count = (bytes_left_in_page > byte_count) ?
             byte_count : bytes_left_in_page;
        unsigned char *host_addr = (unsigned char *)probe_read(
            env, addr, copy_byte_count, mmu_idx, CPU_MEMOP_PC(env));

        byte_count -= copy_byte_count;
        if (host_addr) {
            memcpy(dstbuf, host_addr, copy_byte_count);
            addr += copy_byte_count;
            dstbuf += copy_byte_count;
        } else {
            while (copy_byte_count-- > 0) {
                *dstbuf++ = cpu_ldub_mmuidx_ra(env, addr++, mmu_idx, CPU_MEMOP_PC(env));
            }
        }
        bytes_left_in_page = BYTES_LEFT_IN_PAGE(addr);
    }
}

void hexagon_read_memory(CPUHexagonState *env, target_ulong vaddr,
    int size, void *retptr)

{
    unsigned mmu_idx = cpu_mmu_index(env_cpu(env), false);
    target_ulong paddr = vaddr;

    if (hexagon_read_memory_small(env,
        paddr, size, retptr, mmu_idx) == true)
        return;

    CPUState *cs = env_cpu(env);
    cpu_abort(cs, "%s: ERROR: bad size = %d!\n", __func__, size);
}

int hexagon_read_memory_locked(CPUHexagonState *env, target_ulong vaddr,
    int size, void *retptr)

{
    int ret = 0;

    if (size == 4 || size == 8) {
        unsigned mmu_idx = cpu_mmu_index(env_cpu(env), false);
        target_ulong paddr = vaddr;

        if (hexagon_read_memory_small(env,
            paddr, size, retptr, mmu_idx) == true)
            return ret;
    }

    CPUState *cs = env_cpu(env);
    cpu_abort(cs, "%s: ERROR: bad size = %d!\n", __func__, size);

    return ret; /* cant actually execute because of abort */
}

static inline QEMU_ALWAYS_INLINE bool hexagon_write_memory_small(
    CPUHexagonState *env, target_ulong addr, int byte_count,
    unsigned char *srcbuf, int mmu_idx)

{
    /* handle small sizes */
    switch (byte_count) {
    case 1:
        cpu_stb_mmuidx_ra(env, addr, *srcbuf, mmu_idx, CPU_MEMOP_PC(env));
        return true;

    case 2:
        if (QEMU_IS_ALIGNED(addr, 2)) {
            cpu_stw_le_mmuidx_ra(env, addr, *(uint16_t *)srcbuf, mmu_idx,
                                 CPU_MEMOP_PC(env));
            return true;
        }
        break;

    case 4:
        if (QEMU_IS_ALIGNED(addr, 4)) {
            cpu_stl_le_mmuidx_ra(env, addr, *(uint32_t *)srcbuf, mmu_idx,
                                 CPU_MEMOP_PC(env));
            return true;
        }
        break;

    case 8:
        if (QEMU_IS_ALIGNED(addr, 8)) {
            cpu_stq_le_mmuidx_ra(env, addr, *(uint64_t *)srcbuf, mmu_idx,
                                 CPU_MEMOP_PC(env));
            return true;
        }
        break;

    default:
        /* larger request, handle elsewhere */
        return false;
    }

    /* not aligned, copy bytes */
    for (int i = 0; i < byte_count; ++i) {
        cpu_stb_mmuidx_ra(env, addr++, *srcbuf++, mmu_idx, CPU_MEMOP_PC(env));
    }

    return true;
}

void hexagon_write_memory_block(CPUHexagonState *env, target_ulong addr,
    int byte_count, unsigned char *srcbuf)

{
    unsigned mmu_idx = cpu_mmu_index(env_cpu(env), false);

    /* handle small sizes */
    if (hexagon_write_memory_small(env,
        addr, byte_count, srcbuf, mmu_idx) == true) {
        return;
    }

    /* handle larger sizes here */
    unsigned bytes_left_in_page = BYTES_LEFT_IN_PAGE(addr);
    while (byte_count > 0) {
        unsigned copy_byte_count = (bytes_left_in_page > byte_count) ?
             byte_count : bytes_left_in_page;
        unsigned char *host_addr = (unsigned char *)probe_write(
            env, addr, copy_byte_count, mmu_idx, CPU_MEMOP_PC(env));

        byte_count -= copy_byte_count;
        if (host_addr) {
            memcpy(host_addr, srcbuf, copy_byte_count);
            addr += copy_byte_count;
            srcbuf += copy_byte_count;
        } else {
            while (copy_byte_count-- > 0) {
                cpu_stb_mmuidx_ra(env, addr++, *srcbuf++, mmu_idx, CPU_MEMOP_PC(env));
            }
        }
        bytes_left_in_page = BYTES_LEFT_IN_PAGE(addr);
    }
}

void hexagon_write_memory(CPUHexagonState *env, target_ulong vaddr,
    int size, size8u_t data)

{
    paddr_t paddr = vaddr;
    unsigned mmu_idx = cpu_mmu_index(env_cpu(env), false);

    if (hexagon_write_memory_small(env,
        paddr, size, (unsigned char *)&data, mmu_idx) == true)
        return;

    CPUState *cs = env_cpu(env);
    cpu_abort(cs, "%s: ERROR: bad size = %d!\n", __func__, size);
}

static inline uint32_t page_start(uint32_t addr)
{
    uint32_t page_align = ~(TARGET_PAGE_SIZE - 1);
    return addr & page_align;
}

void hexagon_touch_memory(CPUHexagonState *env, uint32_t start_addr,
                          uint32_t length, int mode)
{
    unsigned mmu_idx = cpu_mmu_index(env_cpu(env), false);
    uint32_t pagelen = TARGET_PAGE_SIZE;
    uint32_t addr = page_start(start_addr);

    while (length > 0) {
        uint32_t cur_len = MIN(length, pagelen);
        probe_access(env, addr, cur_len, mode, mmu_idx, CPU_MEMOP_PC(env));
        length -= cur_len;
        addr += cur_len;
    }
}

static void set_enable_mask(CPUHexagonState *env)

{
    HexagonCPU *cpu;
    uint32_t modectl, thread_enabled_mask;

    g_assert(bql_locked());

    cpu = env_archcpu(env);
    if (!cpu->globalregs) {
        return;
    }
    modectl = hexagon_globalreg_read(cpu->globalregs, HEX_SREG_MODECTL);
    thread_enabled_mask = GET_FIELD(MODECTL_E, modectl);
    thread_enabled_mask |= 0x1 << env->threadId;
    SET_SYSTEM_FIELD(env, HEX_SREG_MODECTL, MODECTL_E, thread_enabled_mask);
}

static uint32_t clear_enable_mask(CPUHexagonState *env)

{
    g_assert(bql_locked());

    const uint32_t modectl =
        hexagon_globalreg_read(env_archcpu(env)->globalregs,
                               HEX_SREG_MODECTL);
    uint32_t thread_enabled_mask = GET_FIELD(MODECTL_E, modectl);
    thread_enabled_mask &= ~(0x1 << env->threadId);
    SET_SYSTEM_FIELD(env, HEX_SREG_MODECTL, MODECTL_E, thread_enabled_mask);
    return thread_enabled_mask;
}

static void set_wait_mode(CPUHexagonState *env)

{
    g_assert(bql_locked());

    const uint32_t modectl =
        hexagon_globalreg_read(env_archcpu(env)->globalregs,
                               HEX_SREG_MODECTL);
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

static void do_start_thread(CPUState *cs, run_on_cpu_data tbd)
{
    BQL_LOCK_GUARD();

    CPUHexagonState *env = cpu_env(cs);

    hexagon_cpu_soft_reset(env);

    cs->halted = 0;
    cs->exception_index = HEX_EVENT_NONE;
    cpu_resume(cs);
}

void hexagon_start_threads(CPUHexagonState *current_env, uint32_t mask)
{
    CPUState *cs;

    /*
     * Acquire BQL so we can call set_enable_mask() synchronously for each
     * target thread.  This ensures the full MODECTL_E mask is visible to
     * the issuing thread before hexagon_start_threads() returns, regardless
     * of when the async resume work items drain.
     */
    BQL_LOCK_GUARD();

    CPU_FOREACH(cs) {
        CPUHexagonState *env = cpu_env(cs);
        if (!(mask & (0x1 << env->threadId))) {
            continue;
        }

        if (current_env->threadId != env->threadId) {
            set_enable_mask(env);
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
    HexagonCPU *cpu = env_archcpu(env);

    uint32_t thread_enabled_mask = clear_enable_mask(env);
    CPUState *cs = env_cpu(env);
    cpu_interrupt(cs, CPU_INTERRUPT_HALT);
    if (!thread_enabled_mask) {
        /* All threads are stopped, request shutdown */
        if (cpu->dump_json_file) {
            hexagon_dump_json(env);
        }
        qemu_system_shutdown_request_with_code(
            SHUTDOWN_CAUSE_GUEST_SHUTDOWN, get_thread0_r2());
    }
}

static int sys_in_monitor_mode_ssr(uint32_t ssr)
{
    if ((GET_SSR_FIELD(SSR_EX, ssr) != 0) ||
        ((GET_SSR_FIELD(SSR_EX, ssr) == 0) &&
         (GET_SSR_FIELD(SSR_UM, ssr) == 0))) {
        return 1;
    }
    return 0;
}

int sys_in_monitor_mode(CPUHexagonState *env)
{
    uint32_t ssr = env->t_sreg[HEX_SREG_SSR];
    return sys_in_monitor_mode_ssr(ssr);
}


static int sys_in_guest_mode_ssr(uint32_t ssr)
{
    if ((GET_SSR_FIELD(SSR_EX, ssr) == 0) &&
        (GET_SSR_FIELD(SSR_UM, ssr) != 0) &&
        (GET_SSR_FIELD(SSR_GM, ssr) != 0)) {
        return 1;
    }
    return 0;
}

int sys_in_guest_mode(CPUHexagonState *env)
{
    uint32_t ssr = env->t_sreg[HEX_SREG_SSR];
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

int sys_in_user_mode(CPUHexagonState *env)
{
    uint32_t ssr = env->t_sreg[HEX_SREG_SSR];
    return sys_in_user_mode_ssr(ssr);
}

static bool sys_coproc_active(CPUHexagonState *env)
{
    uint32_t ssr = env->t_sreg[HEX_SREG_SSR];
    return (GET_SSR_FIELD(SSR_XE2, ssr) == 1);
}

int get_cpu_mode(CPUHexagonState *env)

{
    uint32_t ssr = env->t_sreg[HEX_SREG_SSR];

    if (sys_in_monitor_mode_ssr(ssr)) {
        return HEX_CPU_MODE_MONITOR;
    } else if (sys_in_guest_mode_ssr(ssr)) {
        return HEX_CPU_MODE_GUEST;
    } else if (sys_in_user_mode_ssr(ssr)) {
        return HEX_CPU_MODE_USER;
    }
    return HEX_CPU_MODE_MONITOR;
}

int get_exe_mode(CPUHexagonState *env)
{
    g_assert(bql_locked());

    HexagonCPU *cpu = env_archcpu(env);
    target_ulong modectl =
        hexagon_globalreg_read(cpu->globalregs, HEX_SREG_MODECTL);
    uint32_t thread_enabled_mask = GET_FIELD(MODECTL_E, modectl);
    bool E_bit = thread_enabled_mask & (0x1 << env->threadId);
    uint32_t thread_wait_mask = GET_FIELD(MODECTL_W, modectl);
    bool W_bit = thread_wait_mask & (0x1 << env->threadId);
    target_ulong isdbst2 =
        hexagon_globalreg_read(cpu->globalregs, HEX_SREG_ISDBST2);
    uint32_t debugmode = GET_FIELD(ISDBST2_DEBUGMODE, isdbst2);
    bool D_bit = debugmode & (0x1 << env->threadId);

    /* Figure 4-2 */
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
    g_assert_not_reached();
}

void clear_wait_mode(CPUHexagonState *env)
{
    g_assert(bql_locked());
    HexagonCPU *cpu = env_archcpu(env);
    if (cpu->globalregs) {
        const uint32_t modectl =
            hexagon_globalreg_read(cpu->globalregs, HEX_SREG_MODECTL);
        uint32_t thread_wait_mask = GET_FIELD(MODECTL_W, modectl);
        thread_wait_mask &= ~(0x1 << env->threadId);
        SET_SYSTEM_FIELD(env, HEX_SREG_MODECTL, MODECTL_W, thread_wait_mask);
    }
}

void hexagon_ssr_set_cause(CPUHexagonState *env, uint32_t cause)
{
    uint32_t old, new;

    g_assert(bql_locked());

    old = env->t_sreg[HEX_SREG_SSR];
    SET_SYSTEM_FIELD(env, HEX_SREG_SSR, SSR_EX, 1);
    SET_SYSTEM_FIELD(env, HEX_SREG_SSR, SSR_CAUSE, cause);
    new = env->t_sreg[HEX_SREG_SSR];

    hexagon_modify_ssr(env, new, old);
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
        uint32_t thread_ssr = thread_env->t_sreg[HEX_SREG_SSR];
        if (GET_SSR_FIELD(SSR_XE, thread_ssr) && GET_FIELD(SSR_XA, thread_ssr) == XA) {
            qemu_log_mask(LOG_GUEST_ERROR,
                    "setting SSR.XA '%d' on thread %d but thread"
                    " %d has same extension active\n", XA, env->threadId,
                    thread_env->threadId);
        }
    }
}

void hexagon_modify_ssr(CPUHexagonState *env, uint32_t new, uint32_t old)
{
    bool old_EX, old_UM, old_GM, old_IE, old_XE2;
    bool new_EX, new_UM, new_GM, new_IE, new_XE2;
    uint8_t old_asid, new_asid, old_XA, new_XA;

    g_assert(bql_locked());

    old_EX = GET_SSR_FIELD(SSR_EX, old);
    old_UM = GET_SSR_FIELD(SSR_UM, old);
    old_GM = GET_SSR_FIELD(SSR_GM, old);
    old_IE = GET_SSR_FIELD(SSR_IE, old);
    old_XE2 = GET_SSR_FIELD(SSR_XE2, old);
    old_XA = GET_SSR_FIELD(SSR_XA, old);
    new_EX = GET_SSR_FIELD(SSR_EX, new);
    new_UM = GET_SSR_FIELD(SSR_UM, new);
    new_GM = GET_SSR_FIELD(SSR_GM, new);
    new_IE = GET_SSR_FIELD(SSR_IE, new);
    new_XE2 = GET_SSR_FIELD(SSR_XE2, new);
    new_XA = GET_SSR_FIELD(SSR_XA, new);
    old_asid = GET_SSR_FIELD(SSR_ASID, old);
    new_asid = GET_SSR_FIELD(SSR_ASID, new);

    if ((old_EX != new_EX) ||
        (old_UM != new_UM) ||
        (old_GM != new_GM) ||
        (new_asid != old_asid)) {
        hex_mmu_mode_change(env);
    }

    if (old_XE2 != new_XE2) {
        CPUState *cs;
        int xe2max = 0;
        CPU_FOREACH(cs) {
            CPUHexagonState *thread_env = cpu_env(cs);
            if (sys_coproc_active(thread_env)) {
                xe2max++;
            }
        }
        if (xe2max > 1) {
            qemu_log_mask(LOG_GUEST_ERROR,
                          "Undefined behavior: HMX unit over committed.\n");
        }
    }

    if (old_XA != new_XA) {
        int old_unit = parse_context_idx(env, old_XA);
        int new_unit = parse_context_idx(env, new_XA);
        trace_hexagon_ssr_xa(env->threadId, old_XA, new_XA);

        CPUState *cs = env_cpu(env);
        HexagonCPU *cpu = HEXAGON_CPU(cs);
        /* Ownership exchange */
        if (hexagon_version(cpu) > 0x75) {
            memcpy(VRegs[old_unit], env->VRegs, sizeof(env->VRegs));
            memcpy(QRegs[old_unit], env->QRegs, sizeof(env->QRegs));
            memcpy(env->VRegs, VRegs[new_unit], sizeof(env->VRegs));
            memcpy(env->QRegs, QRegs[new_unit], sizeof(env->QRegs));
        } else {
            if ((old_XA != 0) && (new_XA != 0)) {
                memcpy(VRegs[old_unit], env->VRegs, sizeof(env->VRegs));
                memcpy(QRegs[old_unit], env->QRegs, sizeof(env->QRegs));
                memcpy(env->VRegs, VRegs[new_unit], sizeof(env->VRegs));
                memcpy(env->QRegs, QRegs[new_unit], sizeof(env->QRegs));
            }
            /* New owner acquire */
            else if (new_XA != 0) {
                memcpy(env->VRegs, VRegs[new_unit], sizeof(env->VRegs));
                memcpy(env->QRegs, QRegs[new_unit], sizeof(env->QRegs));
            }
            /* Done using HVX */
            else {
                memcpy(VRegs[old_unit], env->VRegs, sizeof(env->VRegs));
                memcpy(QRegs[old_unit], env->QRegs, sizeof(env->QRegs));
            }
        }

        check_overcommitted_hvx(env, new);
    }

    /* See if the interrupts have been enabled or we have exited EX mode */
    if ((new_IE && !old_IE) ||
        (!new_EX && old_EX)) {
        hex_interrupt_update(env);
    }
}

void hexagon_set_sys_pcycle_count_high(CPUHexagonState *env, uint32_t val)
{
    uint64_t old;
    g_assert(bql_locked());
    old = hexagon_get_sys_pcycle_count(env);
    old = deposit64(old, 32, 32, val);
    hexagon_set_sys_pcycle_count(env, old);
}

void hexagon_set_sys_pcycle_count_low(CPUHexagonState *env, uint32_t val)
{
    uint64_t old;
    g_assert(bql_locked());
    old = hexagon_get_sys_pcycle_count(env);
    old = deposit64(old, 0, 32, val);
    hexagon_set_sys_pcycle_count(env, old);
}

void hexagon_set_sys_pcycle_count(CPUHexagonState *env, uint64_t cycles)
{
    HexagonCPU *cpu = env_archcpu(env);
    hexagon_globalreg_set_pcycle_base(cpu->globalregs, cycles);

    CPUState *cs;
    CPU_FOREACH(cs) {
        CPUHexagonState *thread_env = cpu_env(cs);
        thread_env->t_cycle_count = 0;
    }
}

static CPUHexagonState *get_cpu(unsigned int num)
{
    CPUState *cs;
    CPU_FOREACH(cs) {
        CPUHexagonState *env = cpu_env(cs);
        if (env->threadId == num) {
            return env;
        }
    }
    g_assert_not_reached();
}

static int num_cpus(void)
{
    int num = 0;
    CPUState *cs;
    CPU_FOREACH(cs) {
        num++;
    }
    return num;
}

uint32_t hexagon_get_pmu_event_stats(int event)
{
    uint32_t ret = 0;
    int th;
    CPUState *cs;

    pmu_lock();
    switch (event) {
    case COMMITTED_PKT_ANY:
        CPU_FOREACH(cs) {
            CPUHexagonState *env = cpu_env(cs);
            ret += env->pmu.num_packets;
        }
        break;
    case COMMITTED_PKT_T0:
    case COMMITTED_PKT_T1:
    case COMMITTED_PKT_T2:
    case COMMITTED_PKT_T3:
    case COMMITTED_PKT_T4:
    case COMMITTED_PKT_T5:
    case COMMITTED_PKT_T6:
    case COMMITTED_PKT_T7:
        th = event < COMMITTED_PKT_T6 ?
             event - COMMITTED_PKT_T0 :
             6 + event - COMMITTED_PKT_T6;
        ret += th < num_cpus() ? get_cpu(th)->pmu.num_packets : 0;
        break;
    case HVX_PKT:
        CPU_FOREACH(cs) {
            CPUHexagonState *env = cpu_env(cs);
            ret += env->pmu.hvx_packets;
        }
        break;
    }
    pmu_unlock();
    return ret;
}

uint32_t hexagon_get_pmu_counter(CPUHexagonState *cur_env, int index)
{
    uint32_t ret;
    int event;

    g_assert(index >= 0 && index < NUM_PMU_CTRS);
    pmu_lock();
    event = cur_env->pmu.g_events[index];
    ret = cur_env->pmu.g_ctrs_off[index] + hexagon_get_pmu_event_stats(event);
    pmu_unlock();
    return ret;
}

/*
 * Note: this does NOT touch the offset value.
 * For that, use hexagon_set_pmu_counter().
 */
void hexagon_reset_pmu_event_stats(int event)
{
    pmu_lock();
    int th;
    CPUState *cs;
    switch (event) {
    case COMMITTED_PKT_ANY:
        CPU_FOREACH(cs) {
            CPUHexagonState *env = cpu_env(cs);
            env->pmu.num_packets = 0;
        }
        break;
    case COMMITTED_PKT_T0:
    case COMMITTED_PKT_T1:
    case COMMITTED_PKT_T2:
    case COMMITTED_PKT_T3:
    case COMMITTED_PKT_T4:
    case COMMITTED_PKT_T5:
    case COMMITTED_PKT_T6:
    case COMMITTED_PKT_T7:
        th = pmu_committed_pkt_thread(event);
        if (th < num_cpus()) {
            get_cpu(th)->pmu.num_packets = 0;
        }
        break;
    case HVX_PKT:
        CPU_FOREACH(cs) {
            CPUHexagonState *env = cpu_env(cs);
            env->pmu.hvx_packets = 0;
        }
        break;
    default:
        break;
    }
    pmu_unlock();
}

void hexagon_set_pmu_counter(CPUHexagonState *env, uint32_t reg, uint32_t val)
{
    pmu_lock();
    int index = pmu_index_from_sreg(reg);
    env->pmu.g_ctrs_off[index] = val;
    hexagon_reset_pmu_event_stats(env->pmu.g_events[index]);
    pmu_unlock();
}

#endif
