
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

#ifndef HEXAGON_CPU_HELPER_H
#define HEXAGON_CPU_HELPER_H

static inline void arch_set_thread_reg(CPUHexagonState *env, uint32_t reg,
                                       uint32_t val)
{
    g_assert(reg < TOTAL_PER_THREAD_REGS);
    env->gpr[reg] = val;
}

static inline uint32_t arch_get_thread_reg(CPUHexagonState *env, uint32_t reg)
{
    g_assert(reg < TOTAL_PER_THREAD_REGS);
    return env->gpr[reg];
}

static inline void arch_set_system_reg(CPUHexagonState *env, uint32_t reg,
                                       uint32_t val)
{
    g_assert(reg < NUM_SREGS);
    if (reg < HEX_SREG_GLB_START) {
        env->t_sreg[reg] = val;
    } else {
        env->g_sreg[reg] = val;
    }
}

static inline uint32_t *arch_get_system_reg_addr(CPUHexagonState *env,
                                                 uint32_t reg)
{
    g_assert(reg < NUM_SREGS);
    return reg < HEX_SREG_GLB_START ? &env->t_sreg[reg] : &env->g_sreg[reg];
}

uint32_t arch_get_system_reg(CPUHexagonState *env, uint32_t reg);

#define ARCH_GET_THREAD_REG(ENV,REG) \
    arch_get_thread_reg(ENV, REG)
#define ARCH_SET_THREAD_REG(ENV,REG,VAL) \
    arch_set_thread_reg(ENV, REG,VAL)
#define ARCH_GET_SYSTEM_REG(ENV,REG) \
    arch_get_system_reg(ENV, REG)
#define ARCH_GET_SYSTEM_REG_ADDR(ENV, REG) \
    arch_get_system_reg_addr(ENV, REG)
#define ARCH_SET_SYSTEM_REG(ENV,REG,VAL) \
    arch_set_system_reg(ENV, REG, VAL)

#define DEBUG_MEMORY_READ_ENV(ENV,ADDR,SIZE,PTR) \
    hexagon_read_memory(ENV, ADDR, SIZE, PTR)
#define DEBUG_MEMORY_READ(ADDR,SIZE,PTR) \
    hexagon_read_memory(env, ADDR, SIZE, PTR)
#define DEBUG_MEMORY_READ_LOCKED(ADDR,SIZE,PTR) \
    hexagon_read_memory_locked(env, ADDR, SIZE, PTR)
#define DEBUG_MEMORY_WRITE(ADDR,SIZE,DATA) \
    hexagon_write_memory(env, ADDR, SIZE, (uint64_t)DATA)

void hexagon_read_memory_block(CPUHexagonState *env, target_ulong addr, int byte_count,
    unsigned char *dstbuf);
void hexagon_read_memory(CPUHexagonState *env, target_ulong vaddr,
    int size, void *retptr);
void hexagon_write_memory_block(CPUHexagonState *env, target_ulong addr, int byte_count,
    unsigned char *srcbuf);
void hexagon_write_memory(CPUHexagonState *env, target_ulong vaddr,
    int size, uint64_t data);
int hexagon_read_memory_locked(CPUHexagonState *env,
                                            target_ulong vaddr, int size,
                                            void *retptr);
void hexagon_touch_memory(CPUHexagonState *env, uint32_t start_addr,
                                 uint32_t length);

void hexagon_wait_thread(CPUHexagonState *env, target_ulong PC);
void hexagon_resume_threads(CPUHexagonState *env, uint32_t mask);
void hexagon_start_threads(CPUHexagonState *env, uint32_t mask);
void hexagon_stop_thread(CPUHexagonState *env);
void hexagon_modify_ssr(CPUHexagonState *env, uint32_t new, uint32_t old);
void hexagon_ssr_set_cause(CPUHexagonState *env, uint32_t cause);
/*
 * Clear the interrupt pending bits in the mask.
 */
void hexagon_clear_interrupts(CPUHexagonState *global_env, uint32_t mask);

/*
 * Assert interrupt number @a irq.  This function will search for the appropriate
 * thread to handle the given interrupt and if it's not disabled, it will dispatch
 * it or pend it if none is available.  Threads selected to handle interrupts that
 * are waiting will be resumed.
 */
bool hexagon_assert_interrupt(CPUHexagonState *global_env, uint32_t irq);
void hexagon_enable_int(CPUHexagonState *env, uint32_t int_num);



const char *get_sys_ssr_str(uint32_t ssr);
const char *get_sys_str(CPUHexagonState *env);
int sys_in_monitor_mode_reg(uint32_t ssr);
int sys_in_guest_mode_reg(uint32_t ssr);
int sys_in_user_mode_reg(uint32_t ssr);
int sys_in_monitor_mode(CPUHexagonState *env);
int sys_in_guest_mode(CPUHexagonState *env);
int sys_in_user_mode(CPUHexagonState *env);
int get_cpu_mode(CPUHexagonState *env);
int get_exe_mode(CPUHexagonState *env);
const char *get_exe_mode_str(CPUHexagonState *env);
void clear_wait_mode(CPUHexagonState *env);
void hexagon_set_sys_pcycle_count(CPUHexagonState *env, uint64_t);
void hexagon_set_sys_pcycle_count_low(CPUHexagonState *env, uint32_t);
void hexagon_set_sys_pcycle_count_high(CPUHexagonState *env, uint32_t);

uint32_t hexagon_get_pmu_counter(CPUHexagonState *env, uint32_t reg);
uint32_t hexagon_get_pmu_event_stats(int event);
void hexagon_reset_pmu_event_stats(int event);
void hexagon_set_pmu_counter(CPUHexagonState *env, uint32_t reg, uint32_t val);
#endif
