/*
 *  Copyright(c) 2019-2021 Qualcomm Innovation Center, Inc. All Rights Reserved.
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



#include <math.h>
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
    }
    else if (cpu_mode == HEX_CPU_MODE_GUEST) {
      return MMU_GUEST_IDX;
    }
#endif

    return MMU_USER_IDX;
}

#ifndef CONFIG_USER_ONLY

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
                *(unsigned short *)dstbuf = cpu_lduw_mmuidx_ra(env,
                    addr, mmu_idx, GETPC());
                return true;
            }
            break;

        case 4:
            if (QEMU_IS_ALIGNED(addr, 4)) {
                *(uint32_t *)dstbuf = cpu_ldl_mmuidx_ra(env,
                    addr, mmu_idx, GETPC());
                return true;
            }
            break;

        case 8:
            if (QEMU_IS_ALIGNED(addr, 8)) {
                *(uint64_t *)dstbuf = cpu_ldq_mmuidx_ra(env,
                    addr, mmu_idx, GETPC());
                return true;
            }
            break;

        default:
            /* larger request, handle elsewhere */
            return false;
    }

    /* not aligned, copy bytes */
    for (int i = 0 ; i < byte_count ; ++i)
        *dstbuf++ = cpu_ldub_mmuidx_ra(env, addr++, mmu_idx, GETPC());
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
    cpu_abort(cs, "%s: ERROR: bad size = %d!\n", __FUNCTION__, size);
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
    cpu_abort(cs, "%s: ERROR: bad size = %d!\n", __FUNCTION__, size);

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
                cpu_stw_mmuidx_ra(env,
                    addr, *(uint16_t *)srcbuf, mmu_idx, GETPC());
                return true;
            }
            break;

         case 4:
            if (QEMU_IS_ALIGNED(addr, 4)) {
                cpu_stl_mmuidx_ra(env,
                    addr, *(uint32_t *)srcbuf, mmu_idx, GETPC());
                return true;
            }
            break;

         case 8:
            if (QEMU_IS_ALIGNED(addr, 8)) {
                cpu_stq_mmuidx_ra(env,
                    addr, *(uint64_t *)srcbuf, mmu_idx, GETPC());
                return true;
            }
            break;

         default:
            /* larger request, handle elsewhere */
            return false;
    }

    /* not aligned, copy bytes */
    for (int i = 0 ; i < byte_count ; ++i)
         cpu_stb_mmuidx_ra(env, addr++, *srcbuf++, mmu_idx, GETPC());

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
    cpu_abort(cs, "%s: ERROR: bad size = %d!\n", __FUNCTION__, size);
}

void hexagon_touch_memory(CPUHexagonState *env, uint32_t start_addr,
    uint32_t length)

{
    unsigned int warm;

    for (uint32_t i = 0; i < length; i += 0x1000) {
         DEBUG_MEMORY_READ(start_addr + i, 1, &warm);
    }
}

void hexagon_wait_thread(CPUHexagonState *env)

{
    qemu_mutex_lock_iothread();
    g_assert(env->k0_lock_state == HEX_LOCK_UNLOCKED);
    g_assert(env->tlb_lock_state == HEX_LOCK_UNLOCKED);

    CPUState *cs = env_cpu(env);
    /*
     * The addtion of cpu_has_work is borrowed from arm's wfi helper
     * and is critical for our stability
     */
    if ((cs->exception_index != HEX_EVENT_NONE) ||
        (cpu_has_work(cs))) {
        qemu_mutex_unlock_iothread();
        return;
    }
    set_wait_mode(env);
    env->wait_next_pc = env->gpr[HEX_REG_PC] + 4;
    env->next_PC = env->gpr[HEX_REG_PC];

    qemu_mutex_unlock_iothread();
    cpu_stop_current();
}

void hexagon_resume_thread(CPUHexagonState *env, uint32_t ei)

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
    int htid;

    for (htid = 0 ; htid < THREADS_MAX ; ++htid) {
        if (!(mask & (0x1 << htid))) {
            continue;
        }

        HexagonCPU *cpu;
        CPUHexagonState *env;
        CPUState *cs = env_cpu(current_env);
        bool found = false;
        CPU_FOREACH(cs) {
            cpu = HEXAGON_CPU(cs);
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
            // this thread not currently in wait mode
            continue;
        }
        hexagon_resume_thread(env, HEX_EVENT_NONE);
    }
}

static void do_start_thread(CPUState *cs, run_on_cpu_data tbd)
{
    HexagonCPU *cpu = HEXAGON_CPU(cs);
    CPUHexagonState *env = &cpu->env;

    hexagon_cpu_soft_reset(env);

    uint32_t modectl = ARCH_GET_SYSTEM_REG(env, HEX_SREG_MODECTL);
    uint32_t thread_enabled_mask = GET_FIELD(MODECTL_E, modectl);
    thread_enabled_mask |= 0x1 << env->threadId;
    SET_SYSTEM_FIELD(env,HEX_SREG_MODECTL,MODECTL_E,thread_enabled_mask);

    cs->halted = 0;
    cs->exception_index = HEX_EVENT_NONE;
    cpu_resume(cs);
}

//extern dma_t *dma_adapter_init(processor_t *proc, int dmanum);
void hexagon_start_threads(CPUHexagonState *current_env, uint32_t mask)
{
    CPUState *cs;
    CPU_FOREACH(cs) {
        HexagonCPU *cpu = HEXAGON_CPU(cs);
        CPUHexagonState *env = &cpu->env;
        if (!(mask & (0x1 << env->threadId))) {
            continue;
        }

        if (current_env->threadId == env->threadId)
            run_on_cpu(cs, do_start_thread, RUN_ON_CPU_NULL);
        else
            async_safe_run_on_cpu(cs, do_start_thread, RUN_ON_CPU_NULL);
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
    HEX_DEBUG_LOG("%s: htid %d, cpu %p\n",
        __FUNCTION__, ARCH_GET_SYSTEM_REG(env, HEX_SREG_HTID), cpu);
    #endif

    uint32_t modectl = ARCH_GET_SYSTEM_REG(env, HEX_SREG_MODECTL);
    uint32_t thread_enabled_mask = GET_FIELD(MODECTL_E, modectl);
    thread_enabled_mask &= ~(0x1 << env->threadId);
    SET_SYSTEM_FIELD(env,HEX_SREG_MODECTL,MODECTL_E,thread_enabled_mask);
    cpu_stop_current();
    if (!thread_enabled_mask) {
        if (!cpu->vp_mode) {
            /* All threads are stopped, exit */
            exit(get_thread0_r2());
        }
    }
}

int sys_in_monitor_mode_ssr(uint32_t ssr);
int sys_in_monitor_mode_ssr(uint32_t ssr)
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

int sys_in_guest_mode_ssr(uint32_t ssr);
int sys_in_guest_mode_ssr(uint32_t ssr)
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

int sys_in_user_mode_ssr(uint32_t ssr);
int sys_in_user_mode_ssr(uint32_t ssr)
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
  // from table 4-2
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

    if (exe_mode == HEX_EXE_MODE_OFF)
        return "off";
    else if (exe_mode == HEX_EXE_MODE_RUN)
        return "run";
    else if (exe_mode == HEX_EXE_MODE_WAIT)
        return "wait";
    else if (exe_mode == HEX_EXE_MODE_DEBUG)
        return "debug";
    return "unknown";
}

int get_exe_mode(CPUHexagonState *env)
{
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

void set_wait_mode(CPUHexagonState *env)

{
    uint32_t modectl = ARCH_GET_SYSTEM_REG(env, HEX_SREG_MODECTL);
    uint32_t thread_wait_mask = GET_FIELD(MODECTL_W, modectl);
    thread_wait_mask |= 0x1 << env->threadId;
    SET_SYSTEM_FIELD(env,HEX_SREG_MODECTL,MODECTL_W,thread_wait_mask);
}

void clear_wait_mode(CPUHexagonState *env)

{
    uint32_t modectl = ARCH_GET_SYSTEM_REG(env, HEX_SREG_MODECTL);
    uint32_t thread_wait_mask = GET_FIELD(MODECTL_W, modectl);
    thread_wait_mask &= ~(0x1 << env->threadId);
    SET_SYSTEM_FIELD(env,HEX_SREG_MODECTL,MODECTL_W,thread_wait_mask);
}

void hexagon_ssr_set_cause(CPUHexagonState *env, uint32_t cause)
{
    const uint32_t old = ARCH_GET_SYSTEM_REG(env, HEX_SREG_SSR);
    SET_SYSTEM_FIELD(env, HEX_SREG_SSR, SSR_EX, 1);
    SET_SYSTEM_FIELD(env, HEX_SREG_SSR, SSR_CAUSE, cause);
    const uint32_t new = ARCH_GET_SYSTEM_REG(env, HEX_SREG_SSR);

    hexagon_modify_ssr(env, new, old);
}

void hexagon_modify_ssr(CPUHexagonState *env, uint32_t new, uint32_t old)
{
    target_ulong old_EX = GET_SSR_FIELD(SSR_EX, old);
    target_ulong old_UM = GET_SSR_FIELD(SSR_UM, old);
    target_ulong old_GM = GET_SSR_FIELD(SSR_GM, old);
    target_ulong new_EX = GET_SSR_FIELD(SSR_EX, new);
    target_ulong new_UM = GET_SSR_FIELD(SSR_UM, new);
    target_ulong new_GM = GET_SSR_FIELD(SSR_GM, new);

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

#endif
