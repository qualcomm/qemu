/*
 * Target-specific parts of semihosting/arm-compat-semi.c.
 *
 * Copyright (c) 2005, 2007 CodeSourcery.
 * Copyright (c) 2019, 2022 Linaro
 * Copyright (c) 2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "cpu.h"
#include "semihosting/common-semi.h"
#include "target/xtensa/cpu-qom.h"

uint64_t common_semi_arg(CPUState *cs, int argno)
{
    XtensaCPU *cpu = XTENSA_CPU(cs);
    CPUXtensaState *env = &cpu->env;
    return env->regs[3 + argno];
}

void common_semi_set_ret(CPUState *cs, uint64_t ret)
{
    XtensaCPU *cpu = XTENSA_CPU(cs);
    CPUXtensaState *env = &cpu->env;
    env->regs[2] = ret;
}

void common_semi_set_err(CPUState *cs, uint64_t err)
{
    XtensaCPU *cpu = XTENSA_CPU(cs);
    CPUXtensaState *env = &cpu->env;
    env->regs[3] = err;
}

bool common_semi_sys_exit_is_extended(CPUState *cs)
{
    return false;
}

bool is_64bit_semihosting(CPUArchState *env)
{
    return false;
}

uint64_t common_semi_stack_bottom(CPUState *cs)
{
    XtensaCPU *cpu = XTENSA_CPU(cs);
    CPUXtensaState *env = &cpu->env;
    return env->regs[1];
}

bool common_semi_has_synccache(CPUArchState *env)
{
    return false;
}
