/*
 *  Copyright(c) 2024 Qualcomm Innovation Center, Inc. All Rights Reserved.
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

#ifndef HEXAGON_CPU_MEMOP_PC
#define HEXAGON_CPU_MEMOP_PC


/*************************** Internal functions ******************************/

typedef CPUHexagonState hex_memop_pc_guard;

static inline hex_memop_pc_guard *set_hex_memop_pc(CPUHexagonState *env,
                                                    const char *filename,
                                                    int line, uintptr_t pc,
                                                    bool allow_reset)
{
    if (!allow_reset && env->memop_pc.set) {
        printf("ERROR: trying to set memop pc at file '%s' line %d\n",
               filename, line);
        printf("but it was already set from '%s' line %d.\n",
               env->memop_pc.filename, env->memop_pc.line);
        g_assert_not_reached();
    }
    env->memop_pc.pc = pc;
    env->memop_pc.set = true;
    env->memop_pc.filename = filename;
    env->memop_pc.line = line;
    return env;
}

static inline void hex_memop_pc_guard_cleanup(hex_memop_pc_guard *guard)
{
    CPUHexagonState *env = guard;
    g_assert(env->memop_pc.set);
    env->memop_pc.set = false;
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC(hex_memop_pc_guard, hex_memop_pc_guard_cleanup)


/******************************* Public API **********************************/

    /* MUST be called from top level helpers ONLY. */
#define CPU_MEMOP_PC_SET(ENV) \
    g_autoptr(hex_memop_pc_guard) _hex_memop_pc_guard __attribute__((unused)) \
        = set_hex_memop_pc(ENV, __FILE__, __LINE__, GETPC(), false)

/*
 * To be called by exception handler ONLY. In this case we don't to unwind,
 * so we set te memop pc to 0. Also, it doesn't matter if it was already set
 * as the helper could reach an exception.
 */
#define CPU_MEMOP_PC_SET_ON_EXCEPTION(ENV) \
    g_autoptr(hex_memop_pc_guard) _hex_memop_pc_guard __attribute__((unused)) \
        = set_hex_memop_pc(ENV, __FILE__, __LINE__, 0, true)

static inline uintptr_t CPU_MEMOP_PC(CPUHexagonState *env)
{
    g_assert(env->memop_pc.set);
    return env->memop_pc.pc;
}

#endif /* HEXAGON_CPU_MEMOP_PC */
