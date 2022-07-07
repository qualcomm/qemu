/*
 *  Copyright(c) 2019-2023 Qualcomm Innovation Center, Inc. All Rights Reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "qemu/osdep.h"
#include "cpu.h"
#include "sysemu/cpus.h"
#ifdef CONFIG_USER_ONLY
#include "qemu.h"
#include "exec/helper-proto.h"
#else
#include "hw/boards.h"
#include "hw/hexagon/hexagon.h"
#endif
#include "exec/exec-all.h"
#include "exec/cpu_ldst.h"
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
#include "sysemu/runstate.h"


unsigned cpu_mmu_index(CPUHexagonState *env, bool ifetch)
{
#ifndef CONFIG_USER_ONLY
    uint32_t syscfg = ARCH_GET_SYSTEM_REG(env, HEX_SREG_SYSCFG);
    uint8_t mmuen = GET_SYSCFG_FIELD(SYSCFG_MMUEN, syscfg);
    if (!mmuen) {
        return MMU_KERNEL_IDX;
    }

    int cpu_mode = get_cpu_mode(env);
    if (cpu_mode == HEX_CPU_MODE_MONITOR) {
        return MMU_KERNEL_IDX;
    } else if (cpu_mode == HEX_CPU_MODE_GUEST) {
        return MMU_GUEST_IDX;
    }
#endif

    return MMU_USER_IDX;
}

static const int PCYCLES_PER_PACKET = 3;
uint64_t hexagon_get_sys_pcycle_count(CPUHexagonState *env)
{
    uint64_t packets = 0;
    CPUState *cs;
    CPU_FOREACH(cs) {
        HexagonCPU *cpu = HEXAGON_CPU(cs);
        CPUHexagonState *env_ = &cpu->env;
        packets += env_->t_packet_count;
    }
    return *(env->g_pcycle_base) + (packets * PCYCLES_PER_PACKET);
}

uint32_t hexagon_get_sys_pcycle_count_high(CPUHexagonState *env)
{
    return hexagon_get_sys_pcycle_count(env) >> 32;
}

uint32_t hexagon_get_sys_pcycle_count_low(CPUHexagonState *env)
{
    return extract64(hexagon_get_sys_pcycle_count(env), 0, 32);
}

#ifndef CONFIG_USER_ONLY

uint32_t arch_get_system_reg(CPUHexagonState *env, uint32_t reg)
{
    if (reg == HEX_SREG_PCYCLELO) {
        return hexagon_get_sys_pcycle_count_low(env);
    } else if (reg == HEX_SREG_PCYCLEHI) {
        return hexagon_get_sys_pcycle_count_high(env);
    }

    g_assert(reg < NUM_SREGS);
    return reg < HEX_SREG_GLB_START ? env->t_sreg[reg] : env->g_sreg[reg];
}

#define BYTES_LEFT_IN_PAGE(A) (TARGET_PAGE_SIZE - ((A) % TARGET_PAGE_SIZE))

static inline QEMU_ALWAYS_INLINE bool hexagon_read_memory_small(
    CPUHexagonState *env, target_ulong addr, int byte_count,
    unsigned char *dstbuf, int mmu_idx)

 {
    /* handle small sizes */
    switch (byte_count) {
    case 1:
        *dstbuf = cpu_ldub_mmuidx_ra(env, addr, mmu_idx, GETPC());
        return true;

    case 2:
        if (QEMU_IS_ALIGNED(addr, 2)) {
            *(unsigned short *)dstbuf =
                cpu_lduw_mmuidx_ra(env, addr, mmu_idx, GETPC());
            return true;
        }
        break;

    case 4:
        if (QEMU_IS_ALIGNED(addr, 4)) {
            *(uint32_t *)dstbuf =
                cpu_ldl_mmuidx_ra(env, addr, mmu_idx, GETPC());
            return true;
        }
        break;

    case 8:
        if (QEMU_IS_ALIGNED(addr, 8)) {
            *(uint64_t *)dstbuf =
                cpu_ldq_mmuidx_ra(env, addr, mmu_idx, GETPC());
            return true;
        }
        break;

    default:
        /* larger request, handle elsewhere */
        return false;
    }

    /* not aligned, copy bytes */
    for (int i = 0; i < byte_count; ++i) {
        *dstbuf++ = cpu_ldub_mmuidx_ra(env, addr++, mmu_idx, GETPC());
    }
    return true;
}

void hexagon_read_memory_block(CPUHexagonState *env, target_ulong addr,
    int byte_count, unsigned char *dstbuf)

 {
    unsigned mmu_idx = cpu_mmu_index(env, false);

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
            env, addr, copy_byte_count, mmu_idx, GETPC());

        byte_count -= copy_byte_count;
        if (host_addr) {
            addr += copy_byte_count;
            while (copy_byte_count-- > 0) {
                *dstbuf++ = (unsigned char)ldub_p(host_addr++);
            }
        } else {
            while (copy_byte_count-- > 0) {
                *dstbuf++ = cpu_ldub_mmuidx_ra(env, addr++, mmu_idx, GETPC());
            }
        }
        bytes_left_in_page = BYTES_LEFT_IN_PAGE(addr);
    }
}

void hexagon_read_memory(CPUHexagonState *env, target_ulong vaddr,
    int size, void *retptr)

{
    unsigned mmu_idx = cpu_mmu_index(env, false);
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
        unsigned mmu_idx = cpu_mmu_index(env, false);
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
        cpu_stb_mmuidx_ra(env, addr, *srcbuf, mmu_idx, GETPC());
        return true;

    case 2:
        if (QEMU_IS_ALIGNED(addr, 2)) {
            cpu_stw_mmuidx_ra(env, addr, *(uint16_t *)srcbuf, mmu_idx, GETPC());
            return true;
        }
        break;

    case 4:
        if (QEMU_IS_ALIGNED(addr, 4)) {
            cpu_stl_mmuidx_ra(env, addr, *(uint32_t *)srcbuf, mmu_idx, GETPC());
            return true;
        }
        break;

    case 8:
        if (QEMU_IS_ALIGNED(addr, 8)) {
            cpu_stq_mmuidx_ra(env, addr, *(uint64_t *)srcbuf, mmu_idx, GETPC());
            return true;
        }
        break;

    default:
        /* larger request, handle elsewhere */
        return false;
    }

    /* not aligned, copy bytes */
    for (int i = 0; i < byte_count; ++i) {
        cpu_stb_mmuidx_ra(env, addr++, *srcbuf++, mmu_idx, GETPC());
    }

    return true;
}

void hexagon_write_memory_block(CPUHexagonState *env, target_ulong addr,
    int byte_count, unsigned char *srcbuf)

{
    unsigned mmu_idx = cpu_mmu_index(env, false);

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
            env, addr, copy_byte_count, mmu_idx, GETPC());

        byte_count -= copy_byte_count;
        if (host_addr) {
            addr += copy_byte_count;
            while (copy_byte_count-- > 0) {
                stb_p(host_addr++, *srcbuf++);
            }
        } else {
            while (copy_byte_count-- > 0) {
                cpu_stb_mmuidx_ra(env, addr++, *srcbuf++, mmu_idx, GETPC());
            }
        }
        bytes_left_in_page = BYTES_LEFT_IN_PAGE(addr);
    }
}

void hexagon_write_memory(CPUHexagonState *env, target_ulong vaddr,
    int size, size8u_t data)

{
    paddr_t paddr = vaddr;
    unsigned mmu_idx = cpu_mmu_index(env, false);

    if (hexagon_write_memory_small(env,
        paddr, size, (unsigned char *)&data, mmu_idx) == true)
        return;

    CPUState *cs = env_cpu(env);
    cpu_abort(cs, "%s: ERROR: bad size = %d!\n", __func__, size);
}

void hexagon_touch_memory(CPUHexagonState *env, uint32_t start_addr,
    uint32_t length)

{
    unsigned int warm;

    for (uint32_t i = 0; i < length; i += 0x1000) {
        DEBUG_MEMORY_READ(start_addr + i, 1, &warm);
    }
}

static void set_enable_mask(CPUHexagonState *env)

{
    const bool exception_context = qemu_mutex_iothread_locked();
    LOCK_IOTHREAD(exception_context);

    const uint32_t modectl = ARCH_GET_SYSTEM_REG(env, HEX_SREG_MODECTL);
    uint32_t thread_enabled_mask = GET_FIELD(MODECTL_E, modectl);
    thread_enabled_mask |= 0x1 << env->threadId;
    SET_SYSTEM_FIELD(env, HEX_SREG_MODECTL, MODECTL_E, thread_enabled_mask);
    UNLOCK_IOTHREAD(exception_context);
}

static uint32_t clear_enable_mask(CPUHexagonState *env)

{
    const bool exception_context = qemu_mutex_iothread_locked();
    LOCK_IOTHREAD(exception_context);

    const uint32_t modectl = ARCH_GET_SYSTEM_REG(env, HEX_SREG_MODECTL);
    uint32_t thread_enabled_mask = GET_FIELD(MODECTL_E, modectl);
    thread_enabled_mask &= ~(0x1 << env->threadId);
    SET_SYSTEM_FIELD(env, HEX_SREG_MODECTL, MODECTL_E, thread_enabled_mask);
    UNLOCK_IOTHREAD(exception_context);
    return thread_enabled_mask;
}

static void set_wait_mode(CPUHexagonState *env)

{
    const bool exception_context = qemu_mutex_iothread_locked();
    LOCK_IOTHREAD(exception_context);

    const uint32_t modectl = ARCH_GET_SYSTEM_REG(env, HEX_SREG_MODECTL);
    uint32_t thread_wait_mask = GET_FIELD(MODECTL_W, modectl);
    thread_wait_mask |= 0x1 << env->threadId;
    SET_SYSTEM_FIELD(env, HEX_SREG_MODECTL, MODECTL_W, thread_wait_mask);
    UNLOCK_IOTHREAD(exception_context);
}

void hexagon_wait_thread(CPUHexagonState *env, target_ulong PC)

{
    const bool exception_context = qemu_mutex_iothread_locked();
    LOCK_IOTHREAD(exception_context);

    if (qemu_loglevel_mask(LOG_GUEST_ERROR) &&
        (ATOMIC_LOAD(env->k0_lock_state) != HEX_LOCK_UNLOCKED ||
         ATOMIC_LOAD(env->tlb_lock_state) != HEX_LOCK_UNLOCKED)) {
        qemu_log("WARNING: executing wait() with acquired lock"
                 "may lead to deadlock\n");
    }

    CPUState *cs = env_cpu(env);
    /*
     * The addtion of cpu_has_work is borrowed from arm's wfi helper
     * and is critical for our stability
     */
    if ((cs->exception_index != HEX_EVENT_NONE) ||
        (cpu_has_work(cs))) {
        UNLOCK_IOTHREAD(exception_context);
        return;
    }
    set_wait_mode(env);
    env->wait_next_pc = PC + 4;

    UNLOCK_IOTHREAD(exception_context);
    cpu_stop_current();
}

static void hexagon_resume_thread(CPUHexagonState *env, uint32_t ei)

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
    cs = env_cpu(env);
    cs->exception_index = ei;
    cpu_resume(cs);
}

void hexagon_resume_threads(CPUHexagonState *current_env, uint32_t mask)
{
    const bool exception_context = qemu_mutex_iothread_locked();
    CPUState *cs;
    CPUHexagonState *env;
    bool found;

    LOCK_IOTHREAD(exception_context);
    for (int htid = 0 ; htid < THREADS_MAX ; ++htid) {
        if (!(mask & (0x1 << htid))) {
            continue;
        }

        found = false;
        CPU_FOREACH(cs) {
            HexagonCPU *cpu = HEXAGON_CPU(cs);
            env = &cpu->env;
            if (env->threadId == htid) {
                found = true;
                break;
            }
        }
        if (!found) {
            cpu_abort(cs,
                "Error: Hexagon: Illegal resume thread mask 0x%x", mask);
        }

        if (get_exe_mode(env) != HEX_EXE_MODE_WAIT) {
            /* this thread not currently in wait mode */
            continue;
        }
        hexagon_resume_thread(env, HEX_EVENT_NONE);
    }
    UNLOCK_IOTHREAD(exception_context);
}

static void do_start_thread(CPUState *cs, run_on_cpu_data tbd)
{
    HexagonCPU *cpu = HEXAGON_CPU(cs);
    CPUHexagonState *env = &cpu->env;

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
        HexagonCPU *cpu = HEXAGON_CPU(cs);
        CPUHexagonState *env = &cpu->env;
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
        HexagonCPU *cpu = HEXAGON_CPU(cs);
        CPUHexagonState *thread = &cpu->env;
        if (thread->threadId == 0) {
            return thread->gpr[2];
        }
    }
    g_assert_not_reached();
}

void hexagon_stop_thread(CPUHexagonState *env)

{
    HexagonCPU *cpu = env_archcpu(env);
    #if HEX_DEBUG
    HEX_DEBUG_LOG("%s: htid %d, cpu %p\n", __func__,
                  ARCH_GET_SYSTEM_REG(env, HEX_SREG_HTID), cpu);
#endif

    uint32_t thread_enabled_mask = clear_enable_mask(env);
    cpu_stop_current();
    if (!thread_enabled_mask) {
        if (!cpu->vp_mode) {
            /* All threads are stopped, exit */
            exit(get_thread0_r2());
        }
    }
}

static int sys_in_monitor_mode_ssr(uint32_t ssr)
{
    if ((GET_SSR_FIELD(SSR_EX, ssr) != 0) ||
       ((GET_SSR_FIELD(SSR_EX, ssr) == 0) && (GET_SSR_FIELD(SSR_UM, ssr) == 0)))
        return 1;
    return 0;
}

int sys_in_monitor_mode(CPUHexagonState *env)
{
    uint32_t ssr = ARCH_GET_SYSTEM_REG(env, HEX_SREG_SSR);
    return sys_in_monitor_mode_ssr(ssr);
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
    uint32_t ssr = ARCH_GET_SYSTEM_REG(env, HEX_SREG_SSR);
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
    uint32_t ssr = ARCH_GET_SYSTEM_REG(env, HEX_SREG_SSR);
    return sys_in_user_mode_ssr(ssr);
}

int get_cpu_mode(CPUHexagonState *env)

{
    uint32_t ssr = ARCH_GET_SYSTEM_REG(env, HEX_SREG_SSR);

    if (sys_in_monitor_mode_ssr(ssr)) {
        return HEX_CPU_MODE_MONITOR;
    } else if (sys_in_guest_mode_ssr(ssr)) {
        return HEX_CPU_MODE_GUEST;
    } else if (sys_in_user_mode_ssr(ssr)) {
        return HEX_CPU_MODE_USER;
    }
    return HEX_CPU_MODE_MONITOR;
}

const char *get_sys_ssr_str(uint32_t ssr)

{
    if (sys_in_monitor_mode_ssr(ssr)) {
        return "monitor";
    } else if (sys_in_guest_mode_ssr(ssr)) {
        return "guest";
    } else if (sys_in_user_mode_ssr(ssr)) {
        return "user";
    }
    return "unknown";
}

const char *get_sys_str(CPUHexagonState *env)

{
    if (sys_in_monitor_mode(env)) {
        return "monitor";
    } else if (sys_in_guest_mode(env)) {
        return "guest";
    } else if (sys_in_user_mode(env)) {
        return "user";
    }
    return "unknown";
}

const char *get_exe_mode_str(CPUHexagonState *env)

{
    int exe_mode = get_exe_mode(env);

    if (exe_mode == HEX_EXE_MODE_OFF) {
        return "off";
    } else if (exe_mode == HEX_EXE_MODE_RUN) {
        return "run";
    } else if (exe_mode == HEX_EXE_MODE_WAIT) {
        return "wait";
    } else if (exe_mode == HEX_EXE_MODE_DEBUG) {
        return "debug";
    }
    return "unknown";
}

int get_exe_mode(CPUHexagonState *env)
{
    g_assert(qemu_mutex_iothread_locked());

    target_ulong modectl = ARCH_GET_SYSTEM_REG(env, HEX_SREG_MODECTL);
    uint32_t thread_enabled_mask = GET_FIELD(MODECTL_E, modectl);
    bool E_bit = thread_enabled_mask & (0x1 << env->threadId);
    uint32_t thread_wait_mask = GET_FIELD(MODECTL_W, modectl);
    bool W_bit = thread_wait_mask & (0x1 << env->threadId);
    target_ulong isdbst = ARCH_GET_SYSTEM_REG(env, HEX_SREG_ISDBST);
    uint32_t debugmode = GET_FIELD(ISDBST_DEBUGMODE, isdbst);
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
    const bool exception_context = qemu_mutex_iothread_locked();
    LOCK_IOTHREAD(exception_context);

    const uint32_t modectl = ARCH_GET_SYSTEM_REG(env, HEX_SREG_MODECTL);
    uint32_t thread_wait_mask = GET_FIELD(MODECTL_W, modectl);
    thread_wait_mask &= ~(0x1 << env->threadId);
    SET_SYSTEM_FIELD(env, HEX_SREG_MODECTL, MODECTL_W, thread_wait_mask);
    UNLOCK_IOTHREAD(exception_context);
}

void hexagon_ssr_set_cause(CPUHexagonState *env, uint32_t cause)
{
    const bool exception_context = qemu_mutex_iothread_locked();
    LOCK_IOTHREAD(exception_context);


    const uint32_t old = ARCH_GET_SYSTEM_REG(env, HEX_SREG_SSR);
    SET_SYSTEM_FIELD(env, HEX_SREG_SSR, SSR_EX, 1);
    SET_SYSTEM_FIELD(env, HEX_SREG_SSR, SSR_CAUSE, cause);
    const uint32_t new = ARCH_GET_SYSTEM_REG(env, HEX_SREG_SSR);

    hexagon_modify_ssr(env, new, old);
    UNLOCK_IOTHREAD(exception_context);
}

void hexagon_modify_ssr(CPUHexagonState *env, uint32_t new, uint32_t old)
{
    const bool exception_context = qemu_mutex_iothread_locked();
    LOCK_IOTHREAD(exception_context);

    bool old_EX = GET_SSR_FIELD(SSR_EX, old);
    bool old_UM = GET_SSR_FIELD(SSR_UM, old);
    bool old_GM = GET_SSR_FIELD(SSR_GM, old);
    bool old_IE = GET_SSR_FIELD(SSR_IE, old);
    bool new_EX = GET_SSR_FIELD(SSR_EX, new);
    bool new_UM = GET_SSR_FIELD(SSR_UM, new);
    bool new_GM = GET_SSR_FIELD(SSR_GM, new);
    bool new_IE = GET_SSR_FIELD(SSR_IE, new);

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

    /* See if the interrupts have been enabled or we have exited EX mode */
    if ((new_IE && !old_IE) ||
        (!new_EX && old_EX)) {
        hex_interrupt_update(env);
    }
    UNLOCK_IOTHREAD(exception_context);
}

void hexagon_set_sys_pcycle_count_high(CPUHexagonState *env,
        uint32_t cycles_hi)
{
    uint64_t cur_cycles = hexagon_get_sys_pcycle_count(env);
    uint64_t cycles = ((uint64_t)cycles_hi << 32)
        | extract64(cur_cycles, 0, 32);
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
    *(env->g_pcycle_base) = cycles;

    CPUState *cs;
    CPU_FOREACH(cs) {
        HexagonCPU *cpu = HEXAGON_CPU(cs);
        CPUHexagonState *env_ = &cpu->env;
        env_->t_packet_count = 0;
    }
}

static CPUHexagonState *get_cpu(unsigned int num)
{
    CPUState *cs;
    CPU_FOREACH(cs) {
        HexagonCPU *cpu = HEXAGON_CPU(cs);
        CPUHexagonState *env = &cpu->env;
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

uint32_t hexagon_get_pmu_counter(CPUHexagonState *cur_env, uint32_t reg)
{
    uint32_t ctr;
    int index;
    int event;
    int th;
    CPUState *cs;

    pmu_lock();
    index = pmu_index_from_sreg(reg);
    event = cur_env->pmu.g_events[index];
    ctr = cur_env->pmu.g_ctrs_off[index];

    switch (event) {
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
        ctr += th < num_cpus() ? get_cpu(th)->pmu.num_packets : 0;
        break;
    case HVX_PKT:
        CPU_FOREACH(cs) {
            HexagonCPU *cpu = HEXAGON_CPU(cs);
            CPUHexagonState *env = &cpu->env;
            ctr += env->pmu.hvx_packets;
        }
        break;
    }
    pmu_unlock();
    return ctr;
}

/*
 * Note: this does NOT touch the offset value.
 * For that, use hexagon_set_pmu_counter().
 */
void hexagon_reset_pmu_event_stats(int event)
{
    int th;
    CPUState *cs;
    switch (event) {
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
            HexagonCPU *cpu = HEXAGON_CPU(cs);
            CPUHexagonState *env = &cpu->env;
            env->pmu.hvx_packets = 0;
        }
        break;
    default:
        break;
    }
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
