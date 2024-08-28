/*
 * Copyright(c) 2019-2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef HEXAGON_CPU_HELPER_H
#define HEXAGON_CPU_HELPER_H

uint32_t hexagon_get_pmu_counter(CPUHexagonState *cur_env, int index);
uint64_t hexagon_get_sys_pcycle_count(CPUHexagonState *env);
uint32_t hexagon_get_sys_pcycle_count_low(CPUHexagonState *env);
uint32_t hexagon_get_sys_pcycle_count_high(CPUHexagonState *env);
void hexagon_set_sys_pcycle_count(CPUHexagonState *env, uint64_t);
void hexagon_set_sys_pcycle_count_low(CPUHexagonState *env, uint32_t);
void hexagon_set_sys_pcycle_count_high(CPUHexagonState *env, uint32_t);
void hexagon_modify_ssr(CPUHexagonState *env, uint32_t new, uint32_t old);
int get_exe_mode(CPUHexagonState *env);
void clear_wait_mode(CPUHexagonState *env);
void hexagon_ssr_set_cause(CPUHexagonState *env, uint32_t cause);
void hexagon_start_threads(CPUHexagonState *env, uint32_t mask);
void hexagon_stop_thread(CPUHexagonState *env);
void hexagon_wait_thread(CPUHexagonState *env, target_ulong PC);

static inline void arch_set_thread_reg(CPUHexagonState *env, uint32_t reg,
                                       uint32_t val)
{
    g_assert(reg < TOTAL_PER_THREAD_REGS);
    g_assert_not_reached();
}

static inline uint32_t arch_get_thread_reg(CPUHexagonState *env, uint32_t reg)
{
    g_assert(reg < TOTAL_PER_THREAD_REGS);
    g_assert_not_reached();
}

void arch_set_system_reg_slow(CPUHexagonState *env, uint32_t reg, uint32_t val);

static inline void arch_set_system_reg(CPUHexagonState *env, uint32_t reg,
                                       uint32_t val)
{
    g_assert(reg < NUM_SREGS);
    if (reg < HEX_SREG_GLB_START) {
        env->t_sreg[reg] = val;
    } else {
        arch_set_system_reg_slow(env, reg, val);
    }
}

uint32_t arch_get_system_reg_slow(CPUHexagonState *env, uint32_t reg);

static inline uint32_t arch_get_system_reg(CPUHexagonState *env, uint32_t reg)
{
    g_assert(reg < NUM_SREGS);
    if (reg < HEX_SREG_GLB_START) {
        return env->t_sreg[reg];
    }
    return arch_get_system_reg_slow(env, reg);
}

#endif
