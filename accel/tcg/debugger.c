/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "tcg/debugger.h"

uint64_t debug_read_64(CPUState *cpu, vaddr addr)
{
    uint64_t res;
    if (cpu_memory_rw_debug(cpu, addr, &res, sizeof(uint64_t), false) != 0) {
        return 0;
    }
    return res;
}
