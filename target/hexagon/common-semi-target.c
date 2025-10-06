/*
 * Target-specific parts of semihosting/arm-compat-semi.c.
 *
 * Copyright (c) 2023-2025, Qualcomm.
 *
 * Copyright (c) 2005, 2007 CodeSourcery.
 * Copyright (c) 2019, 2022 Linaro
 * Copyright © 2020 by Keith Packard <keithp@keithp.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef TARGET_HEXAGON_COMMON_SEMI_TARGET_H
#define TARGET_HEXAGON_COMMON_SEMI_TARGET_H

#include "qemu/osdep.h"
#include "cpu.h"
#include "cpu_helper.h"
#include "qemu/log.h"
#include "semihosting/common-semi.h"

uint64_t common_semi_arg(CPUState *cs, int argno)
{
    HexagonCPU *cpu = HEXAGON_CPU(cs);
    CPUHexagonState *env = &cpu->env;
    return arch_get_thread_reg(env, HEX_REG_R00 + argno);
}

void common_semi_set_ret(CPUState *cs, uint64_t ret)
{
    HexagonCPU *cpu = HEXAGON_CPU(cs);
    CPUHexagonState *env = &cpu->env;
    arch_set_thread_reg(env, HEX_REG_R00, ret);
}

void common_semi_set_err(CPUState *cs, uint64_t err)
{
    HexagonCPU *cpu = HEXAGON_CPU(cs);
    CPUHexagonState *env = &cpu->env;
    arch_set_thread_reg(env, HEX_REG_R01, err);
}

bool common_semi_sys_exit_is_extended(CPUState *cs)
{
    return sizeof(target_ulong) == 8;
}

bool is_64bit_semihosting(CPUArchState *env)
{
    return false;
}

uint64_t common_semi_stack_bottom(CPUState *cs)
{
    HexagonCPU *cpu = HEXAGON_CPU(cs);
    CPUHexagonState *env = &cpu->env;
    return arch_get_thread_reg(env, HEX_REG_SP);
}

bool common_semi_has_synccache(CPUArchState *env)
{
    return false;
}

#endif
