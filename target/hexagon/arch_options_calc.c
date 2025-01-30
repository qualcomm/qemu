/*
 *  Copyright(c) 2019-2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "cpu.h"
#define thread_t CPUHexagonState
#include "arch_options_calc.h"

int get_ext_contexts(processor_t *proc)
{
    int ext_contexts = 0;
    if (proc->arch_proc_options->QDSP6_VX_PRESENT) {
        ext_contexts = proc->arch_proc_options->QDSP6_VX_CONTEXTS;
    }
    return ext_contexts;
}
