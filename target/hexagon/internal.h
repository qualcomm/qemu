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

#ifndef HEXAGON_INTERNAL_H
#define HEXAGON_INTERNAL_H

#include "qemu/log.h"

/*
 * Change HEX_DEBUG to 1 to turn on debugging output
 */
#define HEX_DEBUG 0
#define HEX_DEBUG_LOG(...) \
    do { \
        if (HEX_DEBUG) { \
            qemu_log(__VA_ARGS__); \
        } \
    } while (0)

int hexagon_gdb_read_register(CPUState *cpu, GByteArray *buf, int reg);
int hexagon_gdb_write_register(CPUState *cpu, uint8_t *buf, int reg);
#ifndef CONFIG_USER_ONLY
int hexagon_sys_gdb_read_register(CPUHexagonState *env, GByteArray *mem_buf, int n);
int hexagon_sys_gdb_write_register(CPUHexagonState *env, uint8_t *mem_buf, int n);
#endif
int hexagon_hvx_gdb_read_register(CPUHexagonState *env, GByteArray *mem_buf, int n);
int hexagon_hvx_gdb_write_register(CPUHexagonState *env, uint8_t *mem_buf, int n);

/*
 * Change COUNT_HEX_HELPERS to 1 to count how many times each helper
 * is called.  This is useful to figure out which helpers would benefit
 * from writing an fWRAP macro.
 */
#define COUNT_HEX_HELPERS 0

void G_NORETURN do_raise_exception(CPUHexagonState *env,
        uint32_t exception,
        target_ulong PC,
        uintptr_t retaddr);
G_NORETURN void raise_exception(CPUHexagonState *env, uint32_t excp,
                                target_ulong PC);

extern void hexagon_dump(CPUHexagonState *env, FILE *f, int flags);

extern void hexagon_debug_vreg(CPUHexagonState *env, int regnum);
extern void hexagon_debug_qreg(CPUHexagonState *env, int regnum);
extern void hexagon_debug(CPUHexagonState *env);

#if COUNT_HEX_HELPERS
extern void print_helper_counts(void);
#endif

extern const char * const hexagon_regnames[TOTAL_PER_THREAD_REGS];
extern const char * const hexagon_sregnames[];
extern const char * const hexagon_gregnames[];

extern void init_genptr(void);

#define hexagon_cpu_mmu_enabled(env) \
    GET_SYSCFG_FIELD(SYSCFG_MMUEN, ARCH_GET_SYSTEM_REG(env, HEX_SREG_SYSCFG))


#endif
