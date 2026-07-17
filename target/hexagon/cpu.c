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
#include "qemu/qemu-print.h"
#include "qemu/log.h"
#include "cpu.h"
#include "internal.h"
#include "exec/cputlb.h"
#include "exec/translation-block.h"
#include "qapi/error.h"
#include "hw/core/qdev-properties.h"
#include "fpu/softfloat-helpers.h"
#include "tcg/tcg.h"
#include "exec/gdbstub.h"
#include "accel/tcg/cpu-ops.h"
#include "cpu_helper.h"
#include "pmu.h"
#include "hex_mmu.h"
#include "hw/hexagon/hexagon.h"

#ifndef CONFIG_USER_ONLY
#include "hw/hexagon/hexagon_tlb.h"
#include "macros.h"
#include "sys_macros.h"
#include "accel/tcg/cpu-ldst.h"
#include "qemu/main-loop.h"
#include "hex_interrupts.h"
#include "hexswi.h"
#include "exec/cpu-interrupt.h"
#include "exec/target_page.h"
#include "hw/hexagon/hexagon_globalreg.h"
#endif

static uint32_t hexagon_version_to_rev(HexagonVersion ver)
{
    switch (ver) {
    case HEX_VER_V66: return v66_rev;
    case HEX_VER_V67: return v67_rev;
    case HEX_VER_V68: return v68_rev;
    case HEX_VER_V69: return v69_rev;
    case HEX_VER_V71: return v71_rev;
    case HEX_VER_V73: return v73_rev;
    /* No hardware rev constants defined for V75/V77/V79 yet. */
    case HEX_VER_V75:
    case HEX_VER_V77:
    case HEX_VER_V79:
        return unknown_rev;
    case HEX_VER_V81: return v81_rev;
    /* HEX_VER_ANY is a sentinel, not a real hardware version. */
    case HEX_VER_ANY:
        return unknown_rev;
    default:
        qemu_log_mask(LOG_UNIMP,
                      "%s: unhandled cpu version 0x%x\n",
                      __func__, ver);
        return unknown_rev;
    }
}

static ObjectClass *hexagon_cpu_class_by_name(const char *cpu_model)
{
    ObjectClass *oc;
    char *typename;
    char **cpuname;

    cpuname = g_strsplit(cpu_model, ",", 1);
    typename = g_strdup_printf(HEXAGON_CPU_TYPE_NAME("%s"), cpuname[0]);
    oc = object_class_by_name(typename);
    g_strfreev(cpuname);
    g_free(typename);

    return oc;
}

static const Property hexagon_cpu_properties[] = {
#if !defined(CONFIG_USER_ONLY)
    DEFINE_PROP_UINT32("jtlb-entries", HexagonCPU, num_tlbs, MAX_TLB_ENTRIES),
    DEFINE_PROP_UINT32("hvx-contexts", HexagonCPU, hvx_contexts, 0),
    DEFINE_PROP_UINT32("exec-start-addr", HexagonCPU, boot_addr, 0xffffffff),
    DEFINE_PROP_LINK("l2vic", HexagonCPU, l2vic,
                     TYPE_L2VIC_INTERFACE, L2VicInterface *),
    DEFINE_PROP_LINK("global-regs", HexagonCPU, globalregs,
                     TYPE_HEXAGON_GLOBALREG, HexagonGlobalRegState *),
    DEFINE_PROP_LINK("tlb", HexagonCPU, tlb,
                     TYPE_HEXAGON_TLB, HexagonTLBState *),
    DEFINE_PROP_STRING("usefs", HexagonCPU, usefs),
    DEFINE_PROP_BOOL("sched-limit", HexagonCPU, sched_limit, false),
    DEFINE_PROP_BOOL("cacheop-exceptions", HexagonCPU, cacheop_exceptions,
                     false),
#endif
    DEFINE_PROP_UINT32("dsp-rev", HexagonCPU, rev_reg, 0),
    DEFINE_PROP_BOOL("lldb-compat", HexagonCPU, lldb_compat, false),
    DEFINE_PROP_UNSIGNED("lldb-stack-adjust", HexagonCPU, lldb_stack_adjust, 0,
                         qdev_prop_uint32, target_ulong),
    DEFINE_PROP_BOOL("short-circuit", HexagonCPU, short_circuit, true),
    DEFINE_PROP_UNSIGNED("hvx-length", HexagonCPU, env.hvx_vec_len, 128,
                         qdev_prop_uint32, uint32_t),
};

const char * const hexagon_regnames[TOTAL_PER_THREAD_REGS] = {
   "r0", "r1",  "r2",  "r3",  "r4",   "r5",  "r6",  "r7",
   "r8", "r9",  "r10", "r11", "r12",  "r13", "r14", "r15",
  "r16", "r17", "r18", "r19", "r20",  "r21", "r22", "r23",
  "r24", "r25", "r26", "r27", "r28",  "r29", "r30", "r31",
  "sa0", "lc0", "sa1", "lc1", "p3_0", "c5",  "m0",  "m1",
  "usr", "pc",  "ugp", "gp",  "cs0",  "cs1", "upcyclelo", "upcyclehi",
  "framelimit", "framekey", "pktcountlo", "pktcounthi", "upmucnt0",
  "upmucnt1", "upmucnt2", "upmucnt3", "upmucnt4", "upmucnt5", "upmucnt6",
  "upmucnt7",  "c28", "c29", "utimerlo", "utimerhi",
};

#ifndef CONFIG_USER_ONLY
const char * const hexagon_sregnames[] = {
    "sgp0",       "sgp1",       "stid",       "elr",        "badva0",
    "badva1",     "ssr",        "ccr",        "htid",       "badva",
    "imask",      "gevb",       "vwctrl",     "s13",        "s14",
    "s15",        "evb",        "modectl",    "syscfg",     "segment",
    "ipendad",    "vid",        "vid1",       "bestwait",   "s24",
    "schedcfg",   "s26",        "cfgbase",    "diag",       "rev",
    "pcyclelo",   "pcyclehi",   "isdbst",     "isdbcfg0",   "isdbcfg1",
    "livelock",   "brkptpc0",   "brkptcfg0",  "brkptpc1",   "brkptcfg1",
    "isdbmbxin",  "isdbmbxout", "isdben",     "isdbgpr",    "pmucnt4",
    "pmucnt5",    "pmucnt6",    "pmucnt7",    "pmucnt0",    "pmucnt1",
    "pmucnt2",    "pmucnt3",    "pmuevtcfg",  "pmustid0",   "pmuevtcfg1",
    "pmustid1",   "timerlo",    "timerhi",    "pmucfg",     "rgdr2",
    "rgdr",       "turkey",     "duck",       "chicken",
    /* 64-69: per-thread commit counters */
    "commit1t",   "commit2t",   "commit3t",   "commit4t",   "commit5t",
    "commit6t",
    /* 70-75: per-thread pcycle counters */
    "pcycle1t",   "pcycle2t",   "pcycle3t",   "pcycle4t",   "pcycle5t",
    "pcycle6t",
    /* 76-80: ISDB and debug */
    "stfinst",    "isdbcmd",    "isdbver",    "brkptinfo",  "rgdr3",
    /* 81-84: more per-thread counters */
    "commit7t",   "commit8t",   "pcycle7t",   "pcycle8t",
    /* 85-92: extended commit counters */
    "commit9t",   "commit10t",  "commit11t",  "commit12t",  "commit13t",
    "commit14t",  "commit15t",  "commit16t",
    /* 93-100: extended pcycle counters */
    "pcycle9t",   "pcycle10t",  "pcycle11t",  "pcycle12t",  "pcycle13t",
    "pcycle14t",  "pcycle15t",  "pcycle16t",
    /* 101-105: ipend, iad, isdb extensions */
    "ipend",      "iad",        "isdbst1",    "isdbst2",    "brkptinfo1",
};

G_STATIC_ASSERT(NUM_SREGS == ARRAY_SIZE(hexagon_sregnames));

const char * const hexagon_gregnames[] = {
    "gelr",       "gsr",       "gosp",      "gbadva",    "gcommit1t",
    "gcommit2t",  "gcommit3t", "gcommit4t", "gcommit5t", "gcommit6t",
    "gpcycle1t",  "gpcycle2t", "gpcycle3t", "gpcycle4t", "gpcycle5t",
    "gpcycle6t",  "gpmucnt4",  "gpmucnt5",  "gpmucnt6",  "gpmucnt7",
    "gcommit7t",  "gcommit8t", "gpcycle7t", "gpcycle8t", "gpcyclelo",
    "gpcyclehi",  "gpmucnt0",  "gpmucnt1",  "gpmucnt2",  "gpmucnt3",
    "g30",        "g31",
};
#endif
/*
 * One of the main debugging techniques is to use "-d cpu" and compare against
 * LLDB output when single stepping.  However, the target and qemu put the
 * stacks at different locations.  This is used to compensate so the diff is
 * cleaner.
 */
static target_ulong adjust_stack_ptrs(CPUHexagonState *env, target_ulong addr)
{
    HexagonCPU *cpu = env_archcpu(env);
    target_ulong stack_adjust = cpu->lldb_stack_adjust;
    target_ulong stack_start = env->stack_start;
    target_ulong stack_size = 0x10000;

    if (stack_adjust == 0) {
        return addr;
    }

    if (stack_start + 0x1000 >= addr && addr >= (stack_start - stack_size)) {
        return addr - stack_adjust;
    }
    return addr;
}

/* HEX_REG_P3_0_ALIASED (aka C4) is an alias for the predicate registers */
static target_ulong read_p3_0(CPUHexagonState *env)
{
    int32_t control_reg = 0;
    int i;
    for (i = NUM_PREGS - 1; i >= 0; i--) {
        control_reg <<= 8;
        control_reg |= env->pred[i] & 0xff;
    }
    return control_reg;
}

static void print_reg(FILE *f, CPUHexagonState *env, int regnum)
{
    target_ulong value;

    if (regnum == HEX_REG_P3_0_ALIASED) {
        value = read_p3_0(env);
    } else {
        value = regnum < 32 ? adjust_stack_ptrs(env, env->gpr[regnum])
                            : env->gpr[regnum];
    }

    qemu_fprintf(f, "  %s = 0x" TARGET_FMT_lx "\n",
                 hexagon_regnames[regnum], value);
}

#ifndef CONFIG_USER_ONLY
static void print_sreg(FILE *f, CPUHexagonState *env, int regnum)
{
    target_ulong val = arch_get_system_reg(env, regnum);
    qemu_fprintf(f, "  %s = 0x" TARGET_FMT_lx "\n", hexagon_sregnames[regnum],
                 val);
}
#endif

static void print_vreg(FILE *f, CPUHexagonState *env, int regnum,
                       bool skip_if_zero)
{
    uint32_t vec_len = env->hvx_vec_len;

    if (skip_if_zero) {
        bool nonzero_found = false;
        for (int i = 0; i < vec_len; i++) {
            if (env->VRegs[regnum].ub[i] != 0) {
                nonzero_found = true;
                break;
            }
        }
        if (!nonzero_found) {
            return;
        }
    }

    qemu_fprintf(f, "  v%d = ( ", regnum);
    qemu_fprintf(f, "0x%02x", env->VRegs[regnum].ub[vec_len - 1]);
    for (int i = vec_len - 2; i >= 0; i--) {
        qemu_fprintf(f, ", 0x%02x", env->VRegs[regnum].ub[i]);
    }
    qemu_fprintf(f, " )\n");
}

void hexagon_debug_vreg(CPUHexagonState *env, int regnum)
{
    print_vreg(stdout, env, regnum, false);
}

static void print_qreg(FILE *f, CPUHexagonState *env, int regnum,
                       bool skip_if_zero)
{
    uint32_t qreg_len = env->hvx_vec_len / 8;

    if (skip_if_zero) {
        bool nonzero_found = false;
        for (int i = 0; i < qreg_len; i++) {
            if (env->QRegs[regnum].ub[i] != 0) {
                nonzero_found = true;
                break;
            }
        }
        if (!nonzero_found) {
            return;
        }
    }

    qemu_fprintf(f, "  q%d = ( ", regnum);
    qemu_fprintf(f, "0x%02x",
                 env->QRegs[regnum].ub[qreg_len - 1]);
    for (int i = qreg_len - 2; i >= 0; i--) {
        qemu_fprintf(f, ", 0x%02x", env->QRegs[regnum].ub[i]);
    }
    qemu_fprintf(f, " )\n");
}

void hexagon_debug_qreg(CPUHexagonState *env, int regnum)
{
    print_qreg(stdout, env, regnum, false);
}

void hexagon_dump(CPUHexagonState *env, FILE *f, int flags)
{
    HexagonCPU *cpu = env_archcpu(env);

    if (cpu->lldb_compat) {
        /*
         * When comparing with LLDB, it doesn't step through single-cycle
         * hardware loops the same way.  So, we just skip them here
         */
        if (env->gpr[HEX_REG_PC] == env->last_pc_dumped) {
            return;
        }
        env->last_pc_dumped = env->gpr[HEX_REG_PC];
    }

    qemu_fprintf(f, "General Purpose Registers = {\n");
    for (int i = 0; i < 32; i++) {
        print_reg(f, env, i);
    }
    print_reg(f, env, HEX_REG_SA0);
    print_reg(f, env, HEX_REG_LC0);
    print_reg(f, env, HEX_REG_SA1);
    print_reg(f, env, HEX_REG_LC1);
    print_reg(f, env, HEX_REG_M0);
    print_reg(f, env, HEX_REG_M1);
    print_reg(f, env, HEX_REG_USR);
    print_reg(f, env, HEX_REG_P3_0_ALIASED);
    print_reg(f, env, HEX_REG_GP);
    print_reg(f, env, HEX_REG_UGP);
    print_reg(f, env, HEX_REG_PC);
#ifdef CONFIG_USER_ONLY
    /*
     * Not modelled in user mode, print junk to minimize the diff's
     * with LLDB output
     */
    qemu_fprintf(f, "  cause = 0x000000db\n");
    qemu_fprintf(f, "  badva = 0x00000000\n");
    qemu_fprintf(f, "  cs0 = 0x00000000\n");
    qemu_fprintf(f, "  cs1 = 0x00000000\n");
    qemu_fprintf(f, "  elr = 0x00000000\n");
#else
    print_reg(f, env, HEX_SREG_BADVA);
    print_reg(f, env, HEX_REG_CS0);
    print_reg(f, env, HEX_REG_CS1);
    print_sreg(f, env, HEX_SREG_ELR);
#endif
    qemu_fprintf(f, "}\n");

    if (flags & CPU_DUMP_FPU) {
        qemu_fprintf(f, "Vector Registers = {\n");
        for (int i = 0; i < NUM_VREGS; i++) {
            print_vreg(f, env, i, true);
        }
        for (int i = 0; i < NUM_QREGS; i++) {
            print_qreg(f, env, i, true);
        }
        qemu_fprintf(f, "}\n");
    }
}

static void hexagon_dump_state(CPUState *cs, FILE *f, int flags)
{
    hexagon_dump(cpu_env(cs), f, flags);
}

void hexagon_debug(CPUHexagonState *env)
{
    hexagon_dump(env, stdout, CPU_DUMP_FPU);
}

static void hexagon_cpu_set_pc(CPUState *cs, vaddr value)
{
    cpu_env(cs)->gpr[HEX_REG_PC] = value;
}

static vaddr hexagon_cpu_get_pc(CPUState *cs)
{
    return cpu_env(cs)->gpr[HEX_REG_PC];
}

bool rev_requires_v2x_for_128b_hvx(CPUHexagonState *env)
{
    /*
     * Returns true for revisions at or below v66, which only support 64-byte
     * HVX natively. On these revisions, enabling 128-byte mode requires the
     * V2X extension bit (VWCTRL.V2X=1).
     */
    HexagonCPU *hex_cpu = container_of(env, HexagonCPU, env);
    return (hex_cpu->rev_reg & 255) <= (v66_rev & 255);
}

static TCGTBCPUState hexagon_get_tb_cpu_state(CPUState *cs)
{
    CPUHexagonState *env = cpu_env(cs);
    vaddr pc = env->gpr[HEX_REG_PC];
    uint32_t hex_flags = 0;

    if (pc == env->gpr[HEX_REG_SA0]) {
        hex_flags = FIELD_DP32(hex_flags, TB_FLAGS, IS_TIGHT_LOOP, 1);
    }
    if (pc & PCALIGN_MASK) {
        hexagon_raise_exception_err(env, HEX_CAUSE_PC_NOT_ALIGNED, 0);
    }

#ifndef CONFIG_USER_ONLY
    target_ulong syscfg;
    target_ulong ssr;
    bool pcycle_enabled;
    bool hvx_enabled;
    bool ss_active;
    bool pmu_enabled;

    syscfg = arch_get_system_reg(env, HEX_SREG_SYSCFG);

    pcycle_enabled = extract32(syscfg,
                               reg_field_info[SYSCFG_PCYCLEEN].offset,
                               reg_field_info[SYSCFG_PCYCLEEN].width);

    hex_flags = FIELD_DP32(hex_flags, TB_FLAGS, MMU_INDEX,
                           cpu_mmu_index(env_cpu(env), false));

    if (pcycle_enabled) {
        hex_flags = FIELD_DP32(hex_flags, TB_FLAGS, PCYCLE_ENABLED, 1);
    }

    ssr = arch_get_system_reg(env, HEX_SREG_SSR);

    hvx_enabled = extract32(ssr, reg_field_info[SSR_XE].offset,
                            reg_field_info[SSR_XE].width);
    hex_flags =
        FIELD_DP32(hex_flags, TB_FLAGS, HVX_COPROC_ENABLED, hvx_enabled);

    if (rev_requires_v2x_for_128b_hvx(env)) {
        int v2x = extract32(syscfg, reg_field_info[SYSCFG_V2X].offset,
                            reg_field_info[SYSCFG_V2X].width);
        hex_flags = FIELD_DP32(hex_flags, TB_FLAGS, HVX_64B_MODE, !v2x);
    }

    ss_active = extract32(ssr, reg_field_info[SSR_SS].offset,
                          reg_field_info[SSR_SS].width);
    hex_flags = FIELD_DP32(hex_flags, TB_FLAGS, SS_ACTIVE, ss_active);
    hex_flags = FIELD_DP32(hex_flags, TB_FLAGS, SS_PENDING, env->ss_pending);

    pmu_enabled = extract32(syscfg, reg_field_info[SYSCFG_PM].offset,
                            reg_field_info[SYSCFG_PM].width);
    hex_flags = FIELD_DP32(hex_flags, TB_FLAGS, PMU_ENABLED, pmu_enabled);
#else
    hex_flags = FIELD_DP32(hex_flags, TB_FLAGS, PCYCLE_ENABLED, true);
    hex_flags = FIELD_DP32(hex_flags, TB_FLAGS, HVX_COPROC_ENABLED, true);
    hex_flags = FIELD_DP32(hex_flags, TB_FLAGS, MMU_INDEX, MMU_USER_IDX);
    hex_flags = FIELD_DP32(hex_flags, TB_FLAGS, HVX_64B_MODE,
                           rev_requires_v2x_for_128b_hvx(env) &&
                               env->hvx_vec_len == 64);
#endif

    return (TCGTBCPUState){ .pc = pc, .flags = hex_flags };
}

static void hexagon_cpu_synchronize_from_tb(CPUState *cs,
                                            const TranslationBlock *tb)
{
    tcg_debug_assert(!tcg_cflags_has(cs, CF_PCREL));
    cpu_env(cs)->gpr[HEX_REG_PC] = tb->pc;
}

#ifndef CONFIG_USER_ONLY
bool hexagon_thread_is_enabled(CPUHexagonState *env)
{
    HexagonCPU *cpu = env_archcpu(env);
    if (!cpu->globalregs) {
        return true;
    }
    target_ulong modectl = arch_get_system_reg(env, HEX_SREG_MODECTL);
    uint32_t thread_enabled_mask = GET_FIELD(MODECTL_E, modectl);
    bool E_bit = thread_enabled_mask & (0x1 << env->threadId);

    return E_bit;
}

static bool hexagon_cpu_has_work(CPUState *cs)
{
    CPUHexagonState *env = cpu_env(cs);

    if (qatomic_read(&cs->exception_index) != HEX_EVENT_NONE) {
        return true;
    }
    return hexagon_thread_is_enabled(env) &&
        (cs->interrupt_request & (CPU_INTERRUPT_HARD | CPU_INTERRUPT_SWI
            | CPU_INTERRUPT_K0_UNLOCK | CPU_INTERRUPT_TLB_UNLOCK));
}
#endif

static void hexagon_restore_state_to_opc(CPUState *cs,
                                         const TranslationBlock *tb,
                                         const uint64_t *data)
{
    cpu_env(cs)->gpr[HEX_REG_PC] = data[0];
}

#ifndef CONFIG_USER_ONLY
void hexagon_cpu_soft_reset(CPUHexagonState *env)
{
    BQL_LOCK_GUARD();
    arch_set_system_reg(env, HEX_SREG_SSR, 0);
    hexagon_ssr_set_cause(env, HEX_CAUSE_RESET);

    HexagonCPU *cpu = env_archcpu(env);
    if (cpu->globalregs) {
        target_ulong evb = arch_get_system_reg(env, HEX_SREG_EVB);
        arch_set_thread_reg(env, HEX_REG_PC, evb);
    } else {
        arch_set_thread_reg(env, HEX_REG_PC, cpu->boot_addr);
    }
}
#endif


#define HEXAGON_CFG_ADDR_BASE(addr) (((addr) >> 16) & 0x0fffff)
static void hexagon_cpu_reset_hold(Object *obj, ResetType type)
{
    CPUState *cs = CPU(obj);
    HexagonCPUClass *mcc = HEXAGON_CPU_GET_CLASS(obj);
    CPUHexagonState *env = cpu_env(cs);

    if (mcc->parent_phases.hold) {
        mcc->parent_phases.hold(obj, type);
    }

    set_default_nan_mode(1, &env->fp_status);
    set_float_detect_tininess(float_tininess_before_rounding, &env->fp_status);
    /* Default NaN value: sign bit set, all frac bits set */
    set_float_default_nan_pattern(0b11111111, &env->fp_status);
#ifndef CONFIG_USER_ONLY
    HexagonCPU *cpu = HEXAGON_CPU(cs);

    memset(env->t_sreg, 0, sizeof(target_ulong) * NUM_SREGS);
    memset(env->greg, 0, sizeof(target_ulong) * NUM_GREGS);
    memset(env->gpr, 0, sizeof(target_ulong) * TOTAL_PER_THREAD_REGS);
    memset(env->pred, 0, sizeof(target_ulong) * NUM_PREGS);
    memset(env->VRegs, 0, sizeof(MMVector) * NUM_VREGS);
    memset(env->QRegs, 0, sizeof(MMQReg) * NUM_QREGS);
    memset(env->vstore_pending, 0, sizeof(target_ulong) * VSTORES_MAX);
    env->t_cycle_count = 0;
    env->vtcm_pending = false;
    env->pmu.num_packets = 0;
    env->pmu.hvx_packets = 0;

    clear_wait_mode(env);

    arch_set_system_reg(env, HEX_SREG_VWCTRL, DEFAULT_VWCTRL_VAL);

    env->threadId = cs->cpu_index;
    arch_set_system_reg(env, HEX_SREG_HTID, cs->cpu_index);
    env->tlb_lock_state = HEX_LOCK_UNLOCKED;
    env->k0_lock_state = HEX_LOCK_UNLOCKED;
    env->tlb_lock_count = 0;
    env->k0_lock_count = 0;
    env->ss_pending = false;
    env->next_PC = 0;
    env->wait_next_pc = 0;

    hexagon_cpu_soft_reset(env);
    arch_set_thread_reg(env, HEX_REG_PC, cpu->boot_addr);
#endif
    env->cause_code = HEX_EVENT_NONE;

    if (env->hmx_state) {
        memset(env->hmx_state, 0, sizeof(HmxState));
    }
}

static void hexagon_cpu_disas_set_info(const CPUState *cs,
                                       disassemble_info *info)
{
    const HexagonCPU *cpu = HEXAGON_CPU(cs);
    info->print_insn = print_insn_hexagon;
    info->endian = BFD_ENDIAN_LITTLE;
    info->target_info = HEXAGON_CPU_GET_CLASS(cpu)->hex_def;
}

static void hexagon_cpu_realize(DeviceState *dev, Error **errp)
{
    CPUState *cs = CPU(dev);
    HexagonCPUClass *mcc = HEXAGON_CPU_GET_CLASS(dev);
    CPUHexagonState *env = cpu_env(cs);
    Error *local_err = NULL;

    if (env->hvx_vec_len != 64 && env->hvx_vec_len != 128) {
        error_setg(errp, "Invalid hvx-length %u, must be 64 or 128",
                   env->hvx_vec_len);
        return;
    }

    cpu_exec_realizefn(cs, &local_err);
    if (local_err != NULL) {
        error_propagate(errp, local_err);
        return;
    }

#ifdef CONFIG_USER_ONLY
    /* Linux-user: self-allocate HMX state per CPU */
    HEXAGON_CPU(cs)->hmx = g_malloc0(sizeof(HmxState));
#endif
    env->hmx_state = HEXAGON_CPU(cs)->hmx;

#ifndef CONFIG_USER_ONLY
    HexagonCPU *cpu = HEXAGON_CPU(cs);
    if (cpu->num_tlbs > MAX_TLB_ENTRIES) {
        error_setg(errp, "Number of TLBs selected is invalid");
        return;
    }

    /* PMU: share global counter arrays across all Hexagon CPUs. */
    {
        CPUState *other;
        CPUHexagonState *first_env = NULL;

        CPU_FOREACH(other) {
            if (object_dynamic_cast(OBJECT(other), TYPE_HEXAGON_CPU)
                && other != cs) {
                CPUHexagonState *e = cpu_env(other);
                if (e->pmu.g_ctrs_off) {
                    first_env = e;
                    break;
                }
            }
        }
        if (first_env) {
            env->pmu.g_ctrs_off = first_env->pmu.g_ctrs_off;
            env->pmu.g_events = first_env->pmu.g_events;
        } else {
            env->pmu.g_ctrs_off = g_new0(uint32_t, NUM_PMU_CTRS);
            env->pmu.g_events = g_new0(uint8_t, NUM_PMU_CTRS);
        }
    }
#endif

    if (mcc->hex_def) {
        HexagonCPU *cpu_dev = HEXAGON_CPU(cs);
        if (!cpu_dev->rev_reg) {
            cpu_dev->rev_reg =
                hexagon_version_to_rev(mcc->hex_def->hex_version);
        }
    }

    gdb_register_coprocessor(cs, hexagon_hvx_gdb_read_register,
                             hexagon_hvx_gdb_write_register,
                             gdb_find_static_feature("hexagon-hvx.xml"));

#ifndef CONFIG_USER_ONLY
    gdb_register_coprocessor(cs, hexagon_sys_gdb_read_register,
                             hexagon_sys_gdb_write_register,
                             gdb_find_static_feature("hexagon-sys.xml"));
#endif

    qemu_init_vcpu(cs);
    cpu_reset(cs);
    mcc->parent_realize(dev, errp);
}

static int hexagon_cpu_mmu_index(CPUState *cs, bool ifetch)
{
#ifndef CONFIG_USER_ONLY
    CPUHexagonState *env = cpu_env(cs);
    HexagonCPU *cpu = HEXAGON_CPU(cs);
    /*
     * This is called on every TB, for every running CPU. SYSCFG is read
     * (and written, see hexagon_globalreg_{read,write}()) with a
     * lock-free atomic access, and cpu_mode below only reads this CPU's
     * own per-thread SSR, so neither needs the BQL here -- avoiding it
     * matters because taking the BQL on every TB turns it into a
     * contention point under MTTCG.
     */
    if (cpu->globalregs) {
        uint32_t syscfg = arch_get_system_reg(env, HEX_SREG_SYSCFG);
        uint8_t mmuen = GET_SYSCFG_FIELD(SYSCFG_MMUEN, syscfg);
        if (!mmuen) {
            return MMU_KERNEL_IDX;
        }
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

#if !defined(CONFIG_USER_ONLY)
static void hexagon_cpu_set_irq(void *opaque, int irq, int level)
{
    HexagonCPU *cpu = HEXAGON_CPU(opaque);
    CPUState *cs = CPU(cpu);
    CPUHexagonState *env = cpu_env(cs);

    switch (irq) {
    case HEXAGON_CPU_IRQ_0 ... HEXAGON_CPU_IRQ_7:
        qemu_log_mask(CPU_LOG_INT, "%s: irq %d, level %d\n",
                      __func__, irq, level);
        if (level) {
            hex_raise_interrupts(env, 1 << irq, CPU_INTERRUPT_HARD);
        }
        break;
    default:
        g_assert_not_reached();
    }
}
#endif

static void hexagon_cpu_init(Object *obj)
{
#if !defined(CONFIG_USER_ONLY)
    HexagonCPU *cpu = HEXAGON_CPU(obj);
    qdev_init_gpio_in(DEVICE(cpu), hexagon_cpu_set_irq, 8);
#endif
}

#if !defined(CONFIG_USER_ONLY)
static bool get_physical_address(CPUHexagonState *env, hwaddr *phys,
                                 int *prot, uint64_t *size, int32_t *excp,
                                 target_ulong address,
                                 MMUAccessType access_type, int mmu_idx)

{
    if (hexagon_cpu_mmu_enabled(env)) {
        return hex_tlb_find_match(env, address, access_type, phys, prot, size,
                                  excp, mmu_idx);
    } else {
        *phys = address & 0xFFFFFFFF;
        *prot = PAGE_VALID | PAGE_READ | PAGE_WRITE | PAGE_EXEC;
        *size = TARGET_PAGE_SIZE;
        return true;
    }
}

/* qemu seems to only want to know about TARGET_PAGE_SIZE pages */
static void find_qemu_subpage(vaddr *addr, hwaddr *phys, uint64_t page_size)
{
    vaddr page_start = *addr & ~((vaddr)(page_size - 1));
    vaddr offset = ((*addr - page_start) / TARGET_PAGE_SIZE) * TARGET_PAGE_SIZE;
    *addr = page_start + offset;
    *phys += offset;
}

static hwaddr hexagon_cpu_get_phys_addr_debug(CPUState *cs, vaddr addr)
{
    CPUHexagonState *env = cpu_env(cs);
    hwaddr phys_addr;
    int prot;
    uint64_t page_size = 0;
    int32_t excp = 0;
    int mmu_idx = MMU_KERNEL_IDX;

    if (get_physical_address(env, &phys_addr, &prot, &page_size, &excp,
                             addr, 0, mmu_idx)) {
        vaddr page_offset = addr & (TARGET_PAGE_SIZE - 1);
        find_qemu_subpage(&addr, &phys_addr, page_size);
        phys_addr += hexagon_cpu_mmu_enabled(env) ? page_offset : 0;
        return phys_addr;
    }

    return -1;
}


#define INVALID_BADVA 0xbadabada

static void set_badva_regs(CPUHexagonState *env, target_ulong VA, int slot,
                           MMUAccessType access_type)
{
    arch_set_system_reg(env, HEX_SREG_BADVA, VA);

    if (access_type == MMU_INST_FETCH || slot == 0) {
        arch_set_system_reg(env, HEX_SREG_BADVA0, VA);
        arch_set_system_reg(env, HEX_SREG_BADVA1, INVALID_BADVA);
        SET_SSR_FIELD(env, SSR_V0, 1);
        SET_SSR_FIELD(env, SSR_V1, 0);
        SET_SSR_FIELD(env, SSR_BVS, 0);
    } else if (slot == 1) {
        arch_set_system_reg(env, HEX_SREG_BADVA0, INVALID_BADVA);
        arch_set_system_reg(env, HEX_SREG_BADVA1, VA);
        SET_SSR_FIELD(env, SSR_V0, 0);
        SET_SSR_FIELD(env, SSR_V1, 1);
        SET_SSR_FIELD(env, SSR_BVS, 1);
    } else {
        g_assert_not_reached();
    }
}

void raise_tlbmiss_exception(CPUState *cs, target_ulong VA, int slot,
                             MMUAccessType access_type)
{
    CPUHexagonState *env = cpu_env(cs);

    set_badva_regs(env, VA, slot, access_type);

    switch (access_type) {
    case MMU_INST_FETCH:
        cs->exception_index = HEX_EVENT_TLB_MISS_X;
        if ((VA & ~TARGET_PAGE_MASK) == 0) {
            env->cause_code = HEX_CAUSE_TLBMISSX_CAUSE_NEXTPAGE;
        } else {
            env->cause_code = HEX_CAUSE_TLBMISSX_CAUSE_NORMAL;
        }
        break;
    case MMU_DATA_LOAD:
        cs->exception_index = HEX_EVENT_TLB_MISS_RW;
        env->cause_code = HEX_CAUSE_TLBMISSRW_CAUSE_READ;
        break;
    case MMU_DATA_STORE:
        cs->exception_index = HEX_EVENT_TLB_MISS_RW;
        env->cause_code = HEX_CAUSE_TLBMISSRW_CAUSE_WRITE;
        break;
    }
}

void raise_perm_exception(CPUState *cs, target_ulong VA, int slot,
                          MMUAccessType access_type, int32_t excp)
{
    CPUHexagonState *env = cpu_env(cs);

    set_badva_regs(env, VA, slot, access_type);
    cs->exception_index = excp;
}

static const char *access_type_names[] = { "MMU_DATA_LOAD ", "MMU_DATA_STORE",
                                           "MMU_INST_FETCH" };

static const char *mmu_idx_names[] = { "MMU_USER_IDX", "MMU_GUEST_IDX",
                                       "MMU_KERNEL_IDX" };

static bool hexagon_tlb_fill(CPUState *cs, vaddr address, int size,
                             MMUAccessType access_type, int mmu_idx, bool probe,
                             uintptr_t retaddr)
{
    CPUHexagonState *env = cpu_env(cs);
    int slot = env->slot;
    hwaddr phys;
    int prot = 0;
    uint64_t page_size = 0;
    int32_t excp = 0;
    bool ret = 0;

    qemu_log_mask(
        CPU_LOG_MMU,
        "%s: tid = 0x%x, pc = 0x%08" PRIx32 ", vaddr = 0x%08" VADDR_PRIx
        ", size = %d, %s,\tprobe = %d, %s\n",
        __func__, env->threadId, env->gpr[HEX_REG_PC], address, size,
        access_type_names[access_type], probe, mmu_idx_names[mmu_idx]);
    ret = get_physical_address(env, &phys, &prot, &page_size, &excp, address,
                               access_type, mmu_idx);
    if (ret) {
        if (!excp) {
            find_qemu_subpage(&address, &phys, page_size);
            tlb_set_page(cs, address, phys, prot, mmu_idx, TARGET_PAGE_SIZE);
            return ret;
        } else {
            raise_perm_exception(cs, address, slot, access_type, excp);
            do_raise_exception(env, cs->exception_index, env->gpr[HEX_REG_PC],
                               retaddr);
        }
    }
    if (probe) {
        return false;
    }
    raise_tlbmiss_exception(cs, address, slot, access_type);
    do_raise_exception(env, cs->exception_index, env->gpr[HEX_REG_PC], retaddr);
}

#include "hw/core/sysemu-cpu-ops.h"

static int64_t hexagon_get_arch_id(CPUState *cs)
{
    return cpu_env(cs)->threadId;
}

static const struct SysemuCPUOps hexagon_sysemu_ops = {
    .has_work = hexagon_cpu_has_work,
    .get_phys_addr_debug = hexagon_cpu_get_phys_addr_debug,
};

static bool hexagon_cpu_exec_interrupt(CPUState *cs, int interrupt_request)
{
    CPUHexagonState *env = cpu_env(cs);
    if (interrupt_request & CPU_INTERRUPT_TLB_UNLOCK) {
        cs->halted = false;
        cpu_reset_interrupt(cs, CPU_INTERRUPT_TLB_UNLOCK);
        return true;
    }
    if (interrupt_request & CPU_INTERRUPT_K0_UNLOCK) {
        cs->halted = false;
        cpu_reset_interrupt(cs, CPU_INTERRUPT_K0_UNLOCK);
        return true;
    }
    if (interrupt_request & (CPU_INTERRUPT_HARD | CPU_INTERRUPT_SWI)) {
        return hex_check_interrupts(env);
    }
    return false;
}

static void G_NORETURN
hexagon_cpu_do_unaligned_access(CPUState *cs, vaddr addr,
                                MMUAccessType access_type,
                                int mmu_idx, uintptr_t retaddr)
{
    CPUHexagonState *env = cpu_env(cs);

    cs->exception_index = HEX_EVENT_PRECISE;
    switch (access_type) {
    case MMU_DATA_LOAD:
        env->cause_code = HEX_CAUSE_MISALIGNED_LOAD;
        break;
    case MMU_DATA_STORE:
        env->cause_code = HEX_CAUSE_MISALIGNED_STORE;
        break;
    case MMU_INST_FETCH:
        env->cause_code = HEX_CAUSE_PC_NOT_ALIGNED;
        break;
    default:
        g_assert_not_reached();
    }

    qemu_log_mask(CPU_LOG_MMU,
                  "unaligned access %08" VADDR_PRIx " from %08" PRIxPTR "\n",
                  addr, retaddr);

    set_badva_regs(env, addr, 0, access_type);
    do_raise_exception(env, cs->exception_index,
                       env->gpr[HEX_REG_PC], retaddr);
}

#endif

static const TCGCPUOps hexagon_tcg_ops = {
    .guest_default_memory_order = TCG_MO_ALL,
    .mttcg_supported = true,
    .initialize = hexagon_translate_init,
    .translate_code = hexagon_translate_code,
    .get_tb_cpu_state = hexagon_get_tb_cpu_state,
    .synchronize_from_tb = hexagon_cpu_synchronize_from_tb,
    .restore_state_to_opc = hexagon_restore_state_to_opc,
    .mmu_index = hexagon_cpu_mmu_index,
#if !defined(CONFIG_USER_ONLY)
    .cpu_exec_interrupt = hexagon_cpu_exec_interrupt,
    .pointer_wrap = cpu_pointer_wrap_uint32,
    .cpu_exec_reset = cpu_reset,
    .tlb_fill = hexagon_tlb_fill,
    .do_unaligned_access = hexagon_cpu_do_unaligned_access,
    .cpu_exec_halt = hexagon_cpu_has_work,
    .do_interrupt = hexagon_cpu_do_interrupt,
#endif /* !CONFIG_USER_ONLY */
};

static void hexagon_cpu_class_init(ObjectClass *c, const void *data)
{
    HexagonCPUClass *mcc = HEXAGON_CPU_CLASS(c);
    CPUClass *cc = CPU_CLASS(c);
    DeviceClass *dc = DEVICE_CLASS(c);
    ResettableClass *rc = RESETTABLE_CLASS(c);

    device_class_set_parent_realize(dc, hexagon_cpu_realize,
                                    &mcc->parent_realize);

    device_class_set_props(dc, hexagon_cpu_properties);
    resettable_class_set_parent_phases(rc, NULL, hexagon_cpu_reset_hold, NULL,
                                       &mcc->parent_phases);

    cc->class_by_name = hexagon_cpu_class_by_name;
    cc->dump_state = hexagon_dump_state;
    cc->set_pc = hexagon_cpu_set_pc;
    cc->get_pc = hexagon_cpu_get_pc;
    cc->gdb_read_register = hexagon_gdb_read_register;
    cc->gdb_write_register = hexagon_gdb_write_register;
    cc->gdb_stop_before_watchpoint = true;
    cc->gdb_core_xml_file = "hexagon-core.xml";
    cc->disas_set_info = hexagon_cpu_disas_set_info;
#ifndef CONFIG_USER_ONLY
    cc->sysemu_ops = &hexagon_sysemu_ops;
    cc->get_arch_id = hexagon_get_arch_id;
    dc->vmsd = &vmstate_hexagon_cpu;
#endif
#ifdef CONFIG_TCG
    cc->tcg_ops = &hexagon_tcg_ops;
#endif
}

#ifndef CONFIG_USER_ONLY
uint32_t hexagon_greg_read(CPUHexagonState *env, uint32_t reg)
{
    target_ulong ssr = arch_get_system_reg(env, HEX_SREG_SSR);
    int ssr_ce = GET_SSR_FIELD(SSR_CE, ssr);
    int ssr_pe = GET_SSR_FIELD(SSR_PE, ssr);

    if (reg <= HEX_GREG_G3) {
        return env->greg[reg];
    }
    switch (reg) {
    case HEX_GREG_GPCYCLELO:
        return ssr_ce ? hexagon_get_sys_pcycle_count_low(env) : 0;

    case HEX_GREG_GPCYCLEHI:
        return ssr_ce ? hexagon_get_sys_pcycle_count_high(env) : 0;

    default:
        if (is_pmu_greg(reg)) {
            if (!ssr_pe) {
                return 0;
            }
            return hexagon_get_pmu_counter(env, pmu_index_from_greg(reg));
        }
        qemu_log_mask(LOG_UNIMP, "reading greg %" PRId32
                " not yet supported.\n", reg);
        return 0;
    }
}
#endif

static void hexagon_cpu_class_base_init(ObjectClass *c, const void *data)
{
    HexagonCPUClass *mcc = HEXAGON_CPU_CLASS(c);
    /* Make sure all CPU models define a HexagonCPUDef */
    g_assert(!object_class_is_abstract(c) && data != NULL);
    mcc->hex_def = data;
}

#define DEFINE_CPU(type_name, version)         \
    {                                          \
        .name = type_name,                     \
        .parent = TYPE_HEXAGON_CPU,            \
        .class_data = &(const HexagonCPUDef) { \
            .hex_version = version,            \
        },                                     \
    }

static const TypeInfo hexagon_cpu_type_infos[] = {
    {
        .name = TYPE_HEXAGON_CPU,
        .parent = TYPE_CPU,
        .instance_size = sizeof(HexagonCPU),
        .instance_align = __alignof(HexagonCPU),
        .instance_init = hexagon_cpu_init,
        .abstract = true,
        .class_size = sizeof(HexagonCPUClass),
        .class_init = hexagon_cpu_class_init,
        .class_base_init = hexagon_cpu_class_base_init,
    },
    DEFINE_CPU(TYPE_HEXAGON_CPU_V5,               HEX_VER_V5),
    DEFINE_CPU(TYPE_HEXAGON_CPU_V55,              HEX_VER_V55),
    DEFINE_CPU(TYPE_HEXAGON_CPU_V60,              HEX_VER_V60),
    DEFINE_CPU(TYPE_HEXAGON_CPU_V61,              HEX_VER_V61),
    DEFINE_CPU(TYPE_HEXAGON_CPU_V62,              HEX_VER_V62),
    DEFINE_CPU(TYPE_HEXAGON_CPU_V65,              HEX_VER_V65),
    DEFINE_CPU(TYPE_HEXAGON_CPU_V66,              HEX_VER_V66),
    DEFINE_CPU(TYPE_HEXAGON_CPU_V67,              HEX_VER_V67),
    DEFINE_CPU(TYPE_HEXAGON_CPU_V68,              HEX_VER_V68),
    DEFINE_CPU(TYPE_HEXAGON_CPU_V69,              HEX_VER_V69),
    DEFINE_CPU(TYPE_HEXAGON_CPU_V71,              HEX_VER_V71),
    DEFINE_CPU(TYPE_HEXAGON_CPU_V73,              HEX_VER_V73),
    DEFINE_CPU(TYPE_HEXAGON_CPU_V75,              HEX_VER_V75),
    DEFINE_CPU(TYPE_HEXAGON_CPU_V77,              HEX_VER_V77),
    DEFINE_CPU(TYPE_HEXAGON_CPU_V79,              HEX_VER_V79),
    DEFINE_CPU(TYPE_HEXAGON_CPU_V81,              HEX_VER_V81),
    DEFINE_CPU(TYPE_HEXAGON_CPU_ANY,              HEX_VER_ANY),
};

DEFINE_TYPES(hexagon_cpu_type_infos)
