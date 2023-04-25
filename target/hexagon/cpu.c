/*
 *  Copyright(c) 2019-2022 Qualcomm Innovation Center, Inc. All Rights Reserved.
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
#include "cpu.h"
#include "internal.h"
#include "exec/exec-all.h"
#include "qapi/error.h"
#include "hw/qdev-properties.h"
#include "fpu/softfloat-helpers.h"
#include "tcg/tcg.h"
#include "gdb_qreginfo.h"
#include "dma/dma.h"
#include "trace.h"
#include "hw/hexagon/hexagon.h"

#if !defined(CONFIG_USER_ONLY)
#include "migration/vmstate.h"
#include "macros.h"
#include "sys_macros.h"
#include "hex_mmu.h"
#include "hw/intc/l2vic.h"
#include "qemu/main-loop.h"
#include "sysemu/cpus.h"
#include "hex_interrupts.h"
#endif
#include "opcodes.h"
#include "coproc.h"

#define INVALID_REG_VAL (0xababababULL)

static void hexagon_common_cpu_init(Object *obj)
{
}

#ifdef CONFIG_USER_ONLY
static void hexagon_v66_cpu_init(Object *obj)
{
    HexagonCPU *cpu = HEXAGON_CPU(obj);
    cpu->rev_reg = v66_rev;
    hexagon_common_cpu_init(obj);
}
#endif

static void hexagon_v67_cpu_init(Object *obj)
{
    HexagonCPU *cpu = HEXAGON_CPU(obj);
    cpu->rev_reg = v67_rev;
    hexagon_common_cpu_init(obj);
}

static void hexagon_cpu_list_entry(gpointer data, gpointer user_data)
{
    ObjectClass *oc = data;
    char *name = g_strdup(object_class_get_name(oc));
    if (g_str_has_suffix(name, HEXAGON_CPU_TYPE_SUFFIX)) {
        name[strlen(name) - strlen(HEXAGON_CPU_TYPE_SUFFIX)] = '\0';
    }
    qemu_printf("  %s\n", name);
    g_free(name);
}

void hexagon_cpu_list(void)
{
    GSList *list;
    list = object_class_get_list_sorted(TYPE_HEXAGON_CPU, false);
    qemu_printf("Available CPUs:\n");
    g_slist_foreach(list, hexagon_cpu_list_entry, NULL);
    g_slist_free(list);
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
    if (!oc || !object_class_dynamic_cast(oc, TYPE_HEXAGON_CPU) ||
        object_class_is_abstract(oc)) {
        return NULL;
    }
    return oc;
}

static Property hexagon_cpu_properties[] = {
#if !defined(CONFIG_USER_ONLY)
    DEFINE_PROP_BOOL("count-gcycle-xt", HexagonCPU, count_gcycle_xt, false),
    DEFINE_PROP_BOOL("sched-limit", HexagonCPU, sched_limit, false),
    DEFINE_PROP_STRING("usefs", HexagonCPU, usefs),
    DEFINE_PROP_UINT64("config-table-addr", HexagonCPU, config_table_addr,
        0xffffffffULL),
    DEFINE_PROP_BOOL("virtual-platform-mode", HexagonCPU, vp_mode, false),
    DEFINE_PROP_UINT32("start-evb", HexagonCPU, boot_evb, 0x0),
    DEFINE_PROP_UINT32("exec-start-addr", HexagonCPU, boot_addr,
        0xffffffffULL),
    DEFINE_PROP_UINT32("l2vic-base-addr", HexagonCPU, l2vic_base_addr,
        0xffffffffULL),
    DEFINE_PROP_UINT32("qtimer-base-addr", HexagonCPU, qtimer_base_addr,
        0xffffffffULL),
    DEFINE_PROP_BOOL("cacheop-exceptions", HexagonCPU, cacheop_exceptions,
        false),
#endif
    DEFINE_PROP_UINT32("dsp-rev", HexagonCPU, rev_reg, 0),
    DEFINE_PROP_BOOL("lldb-compat", HexagonCPU, lldb_compat, false),
    DEFINE_PROP_UNSIGNED("lldb-stack-adjust", HexagonCPU, lldb_stack_adjust,
                         0, qdev_prop_uint32, target_ulong),
    DEFINE_PROP_BOOL("paranoid-commit-state", HexagonCPU, paranoid_commit_state,
        false),
    DEFINE_PROP_END_OF_LIST()
};


const char * const hexagon_regnames[] = {
#ifdef CONFIG_USER_ONLY
    "r0", "r1",  "r2",  "r3",  "r4",   "r5",  "r6",  "r7",
    "r8", "r9",  "r10", "r11", "r12",  "r13", "r14", "r15",
#else
    "r00", "r01",  "r02", "r03", "r04",  "r05", "r06", "r07",
    "r08", "r09",  "r10", "r11", "r12",  "r13", "r14", "r15",
#endif
    "r16", "r17", "r18", "r19", "r20",  "r21", "r22", "r23",
    "r24", "r25", "r26", "r27", "r28",  "r29", "r30", "r31",
    "sa0", "lc0", "sa1", "lc1", "p3_0", "c5",  "m0",  "m1",
    "usr", "pc",  "ugp", "gp",  "cs0",  "cs1", "upcyclelo", "upcyclehi",
    "framelimit", "framekey", "pktcountlo", "pktcounthi", "pkt_cnt",
    "insn_cnt", "hvx_cnt", "c23", "c24", "c25", "c26", "c27", "c28",
    "c29", "utimerlo", "utimerhi",
};

G_STATIC_ASSERT(TOTAL_PER_THREAD_REGS == ARRAY_SIZE(hexagon_regnames));

#ifndef CONFIG_USER_ONLY
const char * const hexagon_sregnames[] = {
    "sgp0",       "sgp1",       "stid",       "elr",        "badva0",
    "badva1",     "ssr",        "ccr",        "htid",       "badva",
    "imask",      "gevb",       "s12",        "s13",        "s14",
    "s15",        "evb",        "modectl",    "syscfg",     "free19",
    "ipendad",    "vid",        "vid1",       "bestwait",   "free24",
    "schedcfg",   "free26",     "cfgbase",    "diag",       "rev",
    "pcyclelo",   "pcyclehi",   "isdbst",     "isdbcfg0",   "isdbcfg1",
    "livelock",   "brkptpc0",   "brkptccfg0", "brkptpc1",   "brkptcfg1",
    "isdbmbxin",  "isdbmbxout", "isdben",     "isdbgpr",    "pmucnt4",
    "pmucnt5",    "pmucnt6",    "pmucnt7",    "pmucnt0",    "pmucnt1",
    "pmucnt2",    "pmucnt3",    "pmuevtcfg",  "pmustid0",   "pmuevtcfg1",
    "pmustid1",   "timerlo",    "timerhi",    "pmucfg",     "s59",
    "s60",        "s61",        "s62",        "s63",        "commit1t",
    "commit2t",   "commit3t",   "commit4t",   "commit5t",   "commit6t",
    "pcycle1t",   "pcycle2t",   "pcycle3t",   "pcycle4t",   "pcycle5t",
    "pcycle6t",   "stfinst",    "isdbcmd",    "isdbver",    "brkptinfo",
    "rgdr3",      "commit7t",   "commit8t",   "pcycle7t",   "pcycle8t",
    "s85",
};

G_STATIC_ASSERT(NUM_SREGS == ARRAY_SIZE(hexagon_sregnames));

const char * const hexagon_gregnames[] = {
    "g0",         "g1",         "g2",       "g3",
    "rsv4",       "rsv5",       "rsv6",     "rsv7",
    "rsv8",       "rsv9",       "rsv10",    "rsv11",
    "rsv12",      "rsv13",      "rsv14",    "rsv15,",
    "gpmucnt4",   "gpmucnt5",   "gpmucnt6", "gpmucnt7",
    "rsv20",      "rsv21",      "rsv22",    "rsv23",
    "gpcyclelo",  "gpcyclehi",  "gpmucnt0", "gpmucnt1",
    "gpmucnt2",   "gpmucnt3",   "rsv30",    "rsv31"
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
#ifdef CONFIG_USER_ONLY
    env->gpr[HEX_REG_USR] = 0x56000;
#endif

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
static target_ulong get_badva(CPUHexagonState *env)
{
  target_ulong ssr = ARCH_GET_SYSTEM_REG(env, HEX_SREG_SSR);
  if (GET_SSR_FIELD(SSR_BVS, ssr)) {
      return ARCH_GET_SYSTEM_REG(env, HEX_SREG_BADVA1);
  }
  else {
      return ARCH_GET_SYSTEM_REG(env, HEX_SREG_BADVA0);
  }
}

static void print_sreg(FILE *f, CPUHexagonState *env, int regnum)
{
    target_ulong val = ARCH_GET_SYSTEM_REG(env, regnum);
    if (regnum == HEX_SREG_BADVA) {
        val = get_badva(env);
    }
    qemu_fprintf(f, "  %s = 0x" TARGET_FMT_lx "\n", hexagon_sregnames[regnum],
                 val);
}

static void print_greg(FILE *f, CPUHexagonState *env, int regnum)
{
    target_ulong val = hexagon_greg_read(env, regnum);
    qemu_fprintf(f, "  %s = 0x" TARGET_FMT_lx "\n", hexagon_gregnames[regnum],
                 val);
}
#endif

static void print_vreg(FILE *f, CPUHexagonState *env, int regnum,
                       bool skip_if_zero)
{
    if (skip_if_zero) {
        bool nonzero_found = false;
        for (int i = 0; i < MAX_VEC_SIZE_BYTES; i++) {
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
    qemu_fprintf(f, "0x%02x", env->VRegs[regnum].ub[MAX_VEC_SIZE_BYTES - 1]);
    for (int i = MAX_VEC_SIZE_BYTES - 2; i >= 0; i--) {
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
    if (skip_if_zero) {
        bool nonzero_found = false;
        for (int i = 0; i < MAX_VEC_SIZE_BYTES / 8; i++) {
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
                 env->QRegs[regnum].ub[MAX_VEC_SIZE_BYTES / 8 - 1]);
    for (int i = MAX_VEC_SIZE_BYTES / 8 - 2; i >= 0; i--) {
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

#ifdef CONFIG_USER_ONLY
    qemu_fprintf(f, "General Purpose Registers = {\n");
#else
    qemu_fprintf(f, "TID %d : General Purpose Registers = {\n", env->threadId);
#endif

    for (int i = 0; i < 32; i++) {
        print_reg(f, env, i);
    }
    print_reg(f, env, HEX_REG_SA0);
    print_reg(f, env, HEX_REG_LC0);
    print_reg(f, env, HEX_REG_SA1);
    print_reg(f, env, HEX_REG_LC1);

#ifdef CONFIG_USER_ONLY
    print_reg(f, env, HEX_REG_M0);
    print_reg(f, env, HEX_REG_M1);
    print_reg(f, env, HEX_REG_USR);
    print_reg(f, env, HEX_REG_P3_0_ALIASED);
    print_reg(f, env, HEX_REG_GP);
    print_reg(f, env, HEX_REG_UGP);
    print_reg(f, env, HEX_REG_PC);
    /*
     * Not modelled in user mode, print junk to minimize the diff's
     * with LLDB output
     */
    qemu_fprintf(f, "  cause = 0x000000db\n");
    qemu_fprintf(f, "  badva = 0x00000000\n");
    qemu_fprintf(f, "  cs0 = 0x00000000\n");
    qemu_fprintf(f, "  cs1 = 0x00000000\n");
#else
    print_reg(f, env, HEX_REG_P3_0_ALIASED);
    print_reg(f, env, HEX_REG_M0);
    print_reg(f, env, HEX_REG_M1);
    print_reg(f, env, HEX_REG_USR);
    print_reg(f, env, HEX_REG_PC);
    print_reg(f, env, HEX_REG_UGP);
    print_reg(f, env, HEX_REG_GP);

    print_reg(f, env, HEX_REG_CS0);
    print_reg(f, env, HEX_REG_CS1);

    print_reg(f, env, HEX_REG_UPCYCLELO);
    print_reg(f, env, HEX_REG_UPCYCLEHI);
    print_reg(f, env, HEX_REG_FRAMELIMIT);
    print_reg(f, env, HEX_REG_FRAMEKEY);
    print_reg(f, env, HEX_REG_PKTCNTLO);
    print_reg(f, env, HEX_REG_PKTCNTHI);
    print_reg(f, env, HEX_REG_UTIMERLO);
    print_reg(f, env, HEX_REG_UTIMERHI);
    print_sreg(f, env, HEX_SREG_SGP0);
    print_sreg(f, env, HEX_SREG_SGP1);
    print_sreg(f, env, HEX_SREG_STID);
    print_sreg(f, env, HEX_SREG_ELR);
    print_sreg(f, env, HEX_SREG_BADVA0);
    print_sreg(f, env, HEX_SREG_BADVA1);
    print_sreg(f, env, HEX_SREG_SSR);
    print_sreg(f, env, HEX_SREG_CCR);
    print_sreg(f, env, HEX_SREG_HTID);
    print_sreg(f, env, HEX_SREG_BADVA);
    print_sreg(f, env, HEX_SREG_IMASK);
    print_sreg(f, env, HEX_SREG_IPENDAD);
    print_sreg(f, env, HEX_SREG_VID);
    print_sreg(f, env, HEX_SREG_GEVB);
    print_greg(f, env, HEX_GREG_GELR);
    print_greg(f, env, HEX_GREG_GSR);
    print_greg(f, env, HEX_GREG_GOSP);
    print_greg(f, env, HEX_GREG_GBADVA);
    qemu_fprintf(f, "  dm0 = 0x00000000\n");
    qemu_fprintf(f, "  dm1 = 0x00000000\n");
    qemu_fprintf(f, "  dm2 = 0x000002a0\n");
    qemu_fprintf(f, "  dm3 = 0x00000000\n");
    qemu_fprintf(f, "  dm4 = 0x00000000\n");
    qemu_fprintf(f, "  dm5 = 0x00000000\n");
    qemu_fprintf(f, "  dm6 = 0x00000000\n");
    qemu_fprintf(f, "  dm7 = 0x00000000\n");
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
    HexagonCPU *cpu = HEXAGON_CPU(cs);
    CPUHexagonState *env = &cpu->env;

    hexagon_dump(env, f, flags);
}

void hexagon_debug(CPUHexagonState *env)
{
    hexagon_dump(env, stdout, CPU_DUMP_FPU);
}

static void hexagon_cpu_set_pc(CPUState *cs, vaddr value)
{
    HexagonCPU *cpu = HEXAGON_CPU(cs);
    CPUHexagonState *env = &cpu->env;
    env->gpr[HEX_REG_PC] = value;
}

static vaddr hexagon_cpu_get_pc(CPUState *cs)
{
    HexagonCPU *cpu = HEXAGON_CPU(cs);
    CPUHexagonState *env = &cpu->env;
    return env->gpr[HEX_REG_PC];
}

static void hexagon_cpu_synchronize_from_tb(CPUState *cs,
                                            const TranslationBlock *tb)
{
    HexagonCPU *cpu = HEXAGON_CPU(cs);
    CPUHexagonState *env = &cpu->env;
    tcg_debug_assert(!(cs->tcg_cflags & CF_PCREL));
    env->gpr[HEX_REG_PC] = tb->pc;
}

static void hexagon_restore_state_to_opc(CPUState *cs,
                                         const TranslationBlock *tb,
                                         const uint64_t *data)
{
    HexagonCPU *cpu = HEXAGON_CPU(cs);
    CPUHexagonState *env = &cpu->env;

    env->gpr[HEX_REG_PC] = data[0];
}

#if !defined(CONFIG_USER_ONLY)
void hexagon_cpu_soft_reset(CPUHexagonState *env)
{
    ARCH_SET_SYSTEM_REG(env, HEX_SREG_SSR, 0);
    hexagon_ssr_set_cause(env, 0);

    target_ulong evb = ARCH_GET_SYSTEM_REG(env, HEX_SREG_EVB);
    ARCH_SET_THREAD_REG(env, HEX_REG_PC, evb);
}
#endif

static void hexagon_cpu_reset_hold(Object *obj)
{
    CPUState *cs = CPU(obj);
    HexagonCPU *cpu = HEXAGON_CPU(cs);
    HexagonCPUClass *mcc = HEXAGON_CPU_GET_CLASS(cpu);
    CPUHexagonState *env = &cpu->env;

    if (mcc->parent_phases.hold) {
        mcc->parent_phases.hold(obj);
    }

    set_default_nan_mode(1, &env->fp_status);
    set_float_detect_tininess(float_tininess_before_rounding, &env->fp_status);

    env->t_packet_count = 0;
    *(env->g_pcycle_base) = 0;

#ifndef CONFIG_USER_ONLY
    CoprocArgs args;
    args.opcode = COPROC_RESET;
    coproc(args);

    clear_wait_mode(env);

    ATOMIC_STORE(env->k0_lock_state, HEX_LOCK_UNLOCKED);
    ATOMIC_STORE(env->tlb_lock_state, HEX_LOCK_UNLOCKED);
    ATOMIC_STORE(env->ss_pending, false);

    hexagon_cpu_soft_reset(env);
#endif
}

static void hexagon_cpu_disas_set_info(CPUState *s, disassemble_info *info)
{
    info->print_insn = print_insn_hexagon;
}

dma_t *dma_adapter_init(processor_t *proc, int dmanum);
const rev_features_t rev_features_v68 = {
};

const options_struct options_struct_v68 = {
    .l2tcm_base  = 0,  /* FIXME - Should be l2tcm_base ?? */
};

const arch_proc_opt_t arch_proc_opt_v68 = {
    .vtcm_size = VTCM_SIZE,
    .vtcm_offset = VTCM_OFFSET,
    .dmadebugfile = NULL,
    .pmu_enable = 0,
    .dmadebug_verbosity = 0,
    .xfp_inexact_enable = 1,
    .xfp_cvt_frac = 13,
    .xfp_cvt_int = 3,
    .QDSP6_DMA_PRESENT     = 1,
    .QDSP6_DMA_EXTENDED_VA_PRESENT = 0,
    .QDSP6_VX_PRESENT = 1,
    .QDSP6_VX_CONTEXTS = VECTOR_UNIT_MAX,
    .QDSP6_VX_MEM_ENTRIES = 2048,
    .QDSP6_VX_VEC_SZ = 1024,
};

struct ProcessorState ProcessorStateV68 = {
    .features = &rev_features_v68,
    .options = &options_struct_v68,
    .arch_proc_options = &arch_proc_opt_v68,
    .runnable_threads_max = THREADS_MAX,
    .thread_system_mask = 0xf,
    .timing_on = 0,
};

static void hexagon_cpu_realize(DeviceState *dev, Error **errp)
{
    CPUState *cs = CPU(dev);
    HexagonCPUClass *mcc = HEXAGON_CPU_GET_CLASS(dev);
    Error *local_err = NULL;

    cpu_exec_realizefn(cs, &local_err);
    if (local_err != NULL) {
        error_propagate(errp, local_err);
        return;
    }
    if (cs->cpu_index >= THREADS_MAX) {
        error_setg(errp, "Out of threads.  Max is: %d, wanted: %d", THREADS_MAX, cs->cpu_index);
        return;
    }

    qemu_init_vcpu(cs);

    HexagonCPU *cpu = HEXAGON_CPU(cs);
    CPUHexagonState *env = &cpu->env;
    env->threadId = cs->cpu_index;
    env->processor_ptr = &ProcessorStateV68;
    env->processor_ptr->thread[env->threadId] = env;
    env->processor_ptr->dma[env->threadId] = dma_adapter_init(
        env->processor_ptr,
        env->threadId);
    env->system_ptr = NULL;

#ifndef CONFIG_USER_ONLY
    ARCH_SET_SYSTEM_REG(env, HEX_SREG_HTID, env->threadId);
#define HEXAGON_CFG_ADDR_BASE(addr) ((addr >> 16) & 0x0fffff)
    if (cs->cpu_index == 0) {
        env->g_sreg = g_malloc0(sizeof(target_ulong) * NUM_SREGS);
        env->g_gcycle = g_malloc0(sizeof(target_ulong) * NUM_GLOBAL_GCYCLE);
        hex_mmu_init(env);
        ARCH_SET_SYSTEM_REG(env, HEX_SREG_EVB, 0x0);
        ARCH_SET_SYSTEM_REG(env, HEX_SREG_CFGBASE,
                            HEXAGON_CFG_ADDR_BASE(cpu->config_table_addr));
        ARCH_SET_SYSTEM_REG(env, HEX_SREG_REV, cpu->rev_reg);
        ARCH_SET_SYSTEM_REG(env, HEX_SREG_ISDBVER, 0);
        ARCH_SET_SYSTEM_REG(env, HEX_SREG_MODECTL, 0x1);
        env->g_pcycle_base = g_malloc0(sizeof(*env->g_pcycle_base));
        env->pmu.g_ctrs_off = g_malloc0(NUM_PMU_CTRS * sizeof(*env->pmu.g_ctrs_off));
        env->pmu.g_events = g_malloc0(NUM_PMU_CTRS * sizeof(*env->pmu.g_events));

        /*
         * These register indices are placeholders in these arrays
         * and their actual values are synthesized from state elsewhere.
         * We can initialize these with invalid values so that if we
         * mistakenly generate reads, they will look obviously wrong.
         */
        ARCH_SET_SYSTEM_REG(env, HEX_SREG_PCYCLELO, INVALID_REG_VAL);
        ARCH_SET_SYSTEM_REG(env, HEX_SREG_PCYCLEHI, INVALID_REG_VAL);
        ARCH_SET_SYSTEM_REG(env, HEX_SREG_TIMERLO, INVALID_REG_VAL);
        ARCH_SET_SYSTEM_REG(env, HEX_SREG_TIMERHI, INVALID_REG_VAL);
        ARCH_SET_SYSTEM_REG(env, HEX_SREG_PMUCNT0, INVALID_REG_VAL);
        ARCH_SET_SYSTEM_REG(env, HEX_SREG_PMUCNT1, INVALID_REG_VAL);
        ARCH_SET_SYSTEM_REG(env, HEX_SREG_PMUCNT2, INVALID_REG_VAL);
        ARCH_SET_SYSTEM_REG(env, HEX_SREG_PMUCNT3, INVALID_REG_VAL);
        ARCH_SET_SYSTEM_REG(env, HEX_SREG_PMUCNT4, INVALID_REG_VAL);
        ARCH_SET_SYSTEM_REG(env, HEX_SREG_PMUCNT5, INVALID_REG_VAL);
        ARCH_SET_SYSTEM_REG(env, HEX_SREG_PMUCNT6, INVALID_REG_VAL);
        ARCH_SET_SYSTEM_REG(env, HEX_SREG_PMUCNT7, INVALID_REG_VAL);

        CoprocArgs args;
        args.opcode = COPROC_INIT;
        args.vtcm_haddr = env->vtcm_haddr;
        args.vtcm_base = env->vtcm_base;
        args.reg_usr = GET_FIELD(USR_FPCOPROC, env->gpr[HEX_REG_USR]);
        coproc(args);
    }
    else {
        CPUState *cpu0_s = NULL;
        CPUHexagonState *env0 = NULL;
        CPU_FOREACH (cpu0_s) {
            assert(cpu0_s->cpu_index == 0);
            env0 = &(HEXAGON_CPU(cpu0_s)->env);

            break;
        }
        env->g_sreg = env0->g_sreg;
        env->g_gcycle = env0->g_gcycle;
        env->hex_tlb = env0->hex_tlb;
        env->cmdline = env0->cmdline;
        env->lib_search_dir = env0->lib_search_dir;
        env->g_pcycle_base = env0->g_pcycle_base;
        env->pmu.g_ctrs_off = env0->pmu.g_ctrs_off;
        env->pmu.g_events = env0->pmu.g_events;

        dma_t *dma_ptr = env->processor_ptr->dma[env->threadId];
        udma_ctx_t *udma_ctx = (udma_ctx_t *)dma_ptr->udma_ctx;

        dma_t *dma0_ptr = env->processor_ptr->dma[env0->threadId];
        udma_ctx_t *udma0_ctx = (udma_ctx_t *)dma0_ptr->udma_ctx;

        /* init dm2 of new thread to env_0 thread */
        udma_ctx->dm2.val = udma0_ctx->dm2.val;

        clear_wait_mode(env);
    }
#else
    env->g_pcycle_base = g_malloc0(sizeof(*env->g_pcycle_base));
#endif

    cpu_reset(cs);
#if !defined(CONFIG_USER_ONLY)
    ARCH_SET_SYSTEM_REG(env, HEX_SREG_EVB, cpu->boot_evb);
    env->gpr[HEX_REG_PC] = cpu->boot_addr;

    env->gpr[HEX_REG_UPCYCLELO] = INVALID_REG_VAL;
    env->gpr[HEX_REG_UPCYCLEHI] = INVALID_REG_VAL;
    env->gpr[HEX_REG_UTIMERLO]  = INVALID_REG_VAL;
    env->gpr[HEX_REG_UTIMERHI]  = INVALID_REG_VAL;
    env->greg[HEX_GREG_GPCYCLELO] = INVALID_REG_VAL;
    env->greg[HEX_GREG_GPCYCLEHI] = INVALID_REG_VAL;
    env->greg[HEX_GREG_GPMUCNT0] = INVALID_REG_VAL;
    env->greg[HEX_GREG_GPMUCNT1] = INVALID_REG_VAL;
    env->greg[HEX_GREG_GPMUCNT2] = INVALID_REG_VAL;
    env->greg[HEX_GREG_GPMUCNT3] = INVALID_REG_VAL;
    env->greg[HEX_GREG_GPMUCNT4] = INVALID_REG_VAL;
    env->greg[HEX_GREG_GPMUCNT5] = INVALID_REG_VAL;
    env->greg[HEX_GREG_GPMUCNT6] = INVALID_REG_VAL;
    env->greg[HEX_GREG_GPMUCNT7] = INVALID_REG_VAL;
#endif

    mcc->parent_realize(dev, errp);
}

#ifndef CONFIG_USER_ONLY
bool hexagon_thread_is_enabled(CPUHexagonState *env) {
    target_ulong modectl = ARCH_GET_SYSTEM_REG(env, HEX_SREG_MODECTL);
    uint32_t thread_enabled_mask = GET_FIELD(MODECTL_E, modectl);
    bool E_bit = thread_enabled_mask & (0x1 << env->threadId);

    return E_bit;
}

static bool hexagon_cpu_has_work(CPUState *cs)
{
    HexagonCPU *cpu = HEXAGON_CPU(cs);
    CPUHexagonState *env = &cpu->env;

    return hexagon_thread_is_enabled(env) &&
        (cs->interrupt_request & (CPU_INTERRUPT_HARD | CPU_INTERRUPT_SWI));
}

static void hexagon_cpu_set_irq(void *opaque, int irq, int level)
{
    HexagonCPU *cpu = opaque;
    CPUHexagonState *env = &cpu->env;

    trace_hexagon_irq_line(irq, level);

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
    HexagonCPU *cpu = HEXAGON_CPU(obj);

    cpu_set_cpustate_pointers(cpu);
#if !defined(CONFIG_USER_ONLY)
    qdev_init_gpio_in(DEVICE(cpu), hexagon_cpu_set_irq, 8);
#endif
}


#ifndef CONFIG_USER_ONLY

static bool get_physical_address(CPUHexagonState *env, hwaddr *phys,
                                int *prot, int *size, int32_t *excp,
                                target_ulong address,
                                MMUAccessType access_type, int mmu_idx)

{
    if (hexagon_cpu_mmu_enabled(env)) {
        return hex_tlb_find_match(env, address, access_type, phys, prot,
                                  size, excp, mmu_idx);
    } else {
        *phys = address & 0xFFFFFFFF;
        *prot = PAGE_VALID | PAGE_READ | PAGE_WRITE | PAGE_EXEC;
        *size = TARGET_PAGE_SIZE;
        return true;
    }
}

/* qemu seems to only want to know about TARGET_PAGE_SIZE pages */
static void find_qemu_subpage(vaddr *addr, hwaddr *phys,
                                     int page_size)
{
    vaddr page_start = *addr & ~((vaddr)(page_size - 1));
    vaddr offset = ((*addr - page_start) / TARGET_PAGE_SIZE) *
        TARGET_PAGE_SIZE;
    *addr = page_start + offset;
    *phys += offset;
}

#ifndef CONFIG_USER_ONLY
static hwaddr hexagon_cpu_get_phys_page_debug(CPUState *cs, vaddr addr)
{
    HexagonCPU *cpu = HEXAGON_CPU(cs);
    CPUHexagonState *env = &cpu->env;
    hwaddr phys_addr;
    int prot;
    int page_size = 0;
    int32_t excp = 0;
    int mmu_idx = MMU_KERNEL_IDX;

    if (get_physical_address(env, &phys_addr, &prot, &page_size, &excp,
                             addr, 0, mmu_idx)) {
        find_qemu_subpage(&addr, &phys_addr, page_size);
        return phys_addr;
    }

    return -1;
}
#endif

#define INVALID_BADVA                                      0xbadabada

static void set_badva_regs(CPUHexagonState *env, target_ulong VA, int slot,
                           MMUAccessType access_type)
{
    ARCH_SET_SYSTEM_REG(env, HEX_SREG_BADVA, VA);

    if (access_type == MMU_INST_FETCH || slot == 0) {
        ARCH_SET_SYSTEM_REG(env, HEX_SREG_BADVA0, VA);
        ARCH_SET_SYSTEM_REG(env, HEX_SREG_BADVA1, INVALID_BADVA);
        SET_SSR_FIELD(env, SSR_V0, 1);
        SET_SSR_FIELD(env, SSR_V1, 0);
        SET_SSR_FIELD(env, SSR_BVS, 0);
    } else if (slot == 1) {
        ARCH_SET_SYSTEM_REG(env, HEX_SREG_BADVA0, INVALID_BADVA);
        ARCH_SET_SYSTEM_REG(env, HEX_SREG_BADVA1, VA);
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
    HexagonCPU *cpu = HEXAGON_CPU(cs);
    CPUHexagonState *env = &cpu->env;

    set_badva_regs(env, VA, slot, access_type);

    switch (access_type) {
    case MMU_INST_FETCH:
        cs->exception_index = HEX_EVENT_TLB_MISS_X;
        if ((VA & ~TARGET_PAGE_MASK) == 0) {
          env->cause_code = HEX_CAUSE_TLBMISSX_CAUSE_NEXTPAGE;
        }
        else {
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
    HexagonCPU *cpu = HEXAGON_CPU(cs);
    CPUHexagonState *env = &cpu->env;

    set_badva_regs(env, VA, slot, access_type);
    cs->exception_index = excp;
}

static const char *access_type_names[] = {
    "MMU_DATA_LOAD ",
    "MMU_DATA_STORE",
    "MMU_INST_FETCH"
};

static const char *mmu_idx_names[] = {
    "MMU_USER_IDX",
    "MMU_GUEST_IDX",
    "MMU_KERNEL_IDX"
};
#endif

#if !defined(CONFIG_USER_ONLY)
static bool hexagon_tlb_fill(CPUState *cs, vaddr address, int size,
                             MMUAccessType access_type, int mmu_idx,
                             bool probe, uintptr_t retaddr)
{
    HexagonCPU *cpu = HEXAGON_CPU(cs);
    CPUHexagonState *env = &cpu->env;
#ifdef CONFIG_USER_ONLY
    switch (access_type) {
    case MMU_INST_FETCH:
        cs->exception_index = HEX_EVENT_PRECISE;
        env->cause_code = HEX_CAUSE_FETCH_NO_UPAGE;
        break;
    case MMU_DATA_LOAD:
        cs->exception_index = HEX_EVENT_PRECISE;
        env->cause_code = HEX_CAUSE_PRIV_NO_UREAD;
        break;
    case MMU_DATA_STORE:
        cs->exception_index = HEX_EVENT_PRECISE;
        env->cause_code = HEX_CAUSE_PRIV_NO_UWRITE;
        break;
    }
    cpu_loop_exit_restore(cs, retaddr);
#else
    int slot = env->slot;
    hwaddr phys;
    int prot = 0;
    int page_size = 0;
    int32_t excp = 0;
    bool ret = 0;

    qemu_log_mask(CPU_LOG_MMU,
                  "%s: tid = 0x%x, pc = 0x%08" PRIx32
                  ", vaddr = 0x%08" VADDR_PRIx
                  ", size = %d, %s,\tprobe = %d, %s\n",
                  __func__, env->threadId, env->gpr[HEX_REG_PC], address, size,
                  access_type_names[access_type],
                  probe, mmu_idx_names[mmu_idx]);
    ret = get_physical_address(env, &phys, &prot, &page_size, &excp,
                               address, access_type, mmu_idx);
    if (ret) {
        if (!excp) {
            find_qemu_subpage(&address, &phys, page_size);
            tlb_set_page(cs, address, phys, prot,
                         mmu_idx, TARGET_PAGE_SIZE);
            return ret;
        } else {
            raise_perm_exception(cs, address, slot, access_type, excp);
            do_raise_exception(env, cs->exception_index,
                               env->gpr[HEX_REG_PC], retaddr);
        }
    }

    raise_tlbmiss_exception(cs, address, slot, access_type);
    do_raise_exception(env, cs->exception_index,
                       env->gpr[HEX_REG_PC], retaddr);
#endif
}
#endif

#ifndef CONFIG_USER_ONLY
#include "hw/core/sysemu-cpu-ops.h"

static const struct SysemuCPUOps hexagon_sysemu_ops = {
    .get_phys_page_debug = hexagon_cpu_get_phys_page_debug,
};
#endif

#include "hw/core/tcg-cpu-ops.h"

#define CHECK_EX 0
#ifndef CONFIG_USER_ONLY
static bool hexagon_cpu_exec_interrupt(CPUState *cs, int interrupt_request)
{
    HexagonCPU *cpu = HEXAGON_CPU(cs);
    CPUHexagonState *env = &cpu->env;
    if (interrupt_request & (CPU_INTERRUPT_HARD | CPU_INTERRUPT_SWI)) {
        return hex_check_interrupts(env);
    }
    return false;
}

static void G_NORETURN hexagon_cpu_do_unaligned_access(CPUState *cs, vaddr addr,
                                        MMUAccessType access_type,
                                        int mmu_idx,
                                        uintptr_t retaddr)
{
    HexagonCPU *cpu = HEXAGON_CPU(cs);
    CPUHexagonState *env = &cpu->env;

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
        "unaligned access %08x from %08x\n", (int)addr, (int)retaddr);

    set_badva_regs(env, addr, 0, access_type);
    do_raise_exception(env, cs->exception_index, env->gpr[HEX_REG_PC], retaddr);
}

#endif

#ifdef CONFIG_TCG
static struct TCGCPUOps hexagon_tcg_ops = {
    .initialize = hexagon_translate_init,
    .synchronize_from_tb = hexagon_cpu_synchronize_from_tb,
    .restore_state_to_opc = hexagon_restore_state_to_opc,

#if !defined(CONFIG_USER_ONLY)
    .tlb_fill = hexagon_tlb_fill,
    .cpu_exec_interrupt = hexagon_cpu_exec_interrupt,
    .do_interrupt = hexagon_cpu_do_interrupt,
    .do_unaligned_access = hexagon_cpu_do_unaligned_access,
#endif /* !CONFIG_USER_ONLY */
};
#endif

static void hexagon_cpu_class_init(ObjectClass *c, void *data)
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
#if !defined(CONFIG_USER_ONLY)
    cc->has_work = hexagon_cpu_has_work;
#endif
    cc->dump_state = hexagon_dump_state;
    cc->set_pc = hexagon_cpu_set_pc;
    cc->get_pc = hexagon_cpu_get_pc;
    cc->gdb_read_register = hexagon_gdb_read_register;
    cc->gdb_write_register = hexagon_gdb_write_register;
    cc->gdb_qreg_info_lines = (const char **)hexagon_qreg_descs;
    cc->gdb_qreg_info_line_count = ARRAY_SIZE(hexagon_qreg_descs);
#if defined(CONFIG_USER_ONLY)
    cc->gdb_num_core_regs = TOTAL_PER_THREAD_REGS + NUM_VREGS + NUM_QREGS;
#else
    cc->gdb_num_core_regs = TOTAL_PER_THREAD_REGS + NUM_VREGS + NUM_QREGS + NUM_SREGS + NUM_GREGS;
#endif
    cc->gdb_stop_before_watchpoint = true;
    cc->disas_set_info = hexagon_cpu_disas_set_info;
#ifndef CONFIG_USER_ONLY
    cc->sysemu_ops = &hexagon_sysemu_ops;
#endif
#ifdef CONFIG_TCG
    cc->tcg_ops = &hexagon_tcg_ops;
#endif
}

#ifndef CONFIG_USER_ONLY
uint32_t hexagon_greg_read(CPUHexagonState *env, uint32_t reg)
{
    target_ulong ssr = ARCH_GET_SYSTEM_REG(env, HEX_SREG_SSR);
    int ssr_ce = GET_SSR_FIELD(SSR_CE, ssr);
    int ssr_pe = GET_SSR_FIELD(SSR_PE, ssr);
    int off;

    if (reg <= HEX_GREG_G3) {
      return env->greg[reg];
    }
    switch (reg) {
    case HEX_GREG_GCYCLE_1T:
    case HEX_GREG_GCYCLE_2T:
    case HEX_GREG_GCYCLE_3T:
    case HEX_GREG_GCYCLE_4T:
    case HEX_GREG_GCYCLE_5T:
    case HEX_GREG_GCYCLE_6T:
        off = reg - HEX_GREG_GCYCLE_1T;
        return ssr_pe ? env->g_gcycle[off] : 0;

    case HEX_GREG_GPCYCLELO:
        return ssr_ce ? hexagon_get_sys_pcycle_count_low(env) : 0;

    case HEX_GREG_GPCYCLEHI:
        return ssr_ce ? hexagon_get_sys_pcycle_count_high(env) : 0;

    case HEX_GREG_GPMUCNT0:
    case HEX_GREG_GPMUCNT1:
    case HEX_GREG_GPMUCNT2:
    case HEX_GREG_GPMUCNT3:
        off = reg - HEX_GREG_GPMUCNT0;
        return ssr_pe ? hexagon_get_pmu_counter(env, HEX_SREG_PMUCNT0 + off) : 0;
    case HEX_GREG_GPMUCNT4:
    case HEX_GREG_GPMUCNT5:
    case HEX_GREG_GPMUCNT6:
    case HEX_GREG_GPMUCNT7:
        off = reg - HEX_GREG_GPMUCNT4;
        return ssr_pe ? hexagon_get_pmu_counter(env, HEX_SREG_PMUCNT4 + off) : 0;
    default:
        return 0;
    }
}
#endif

#define DEFINE_CPU(type_name, initfn)      \
    {                                      \
        .name = type_name,                 \
        .parent = TYPE_HEXAGON_CPU,        \
        .instance_init = initfn            \
    }

static const TypeInfo hexagon_cpu_type_infos[] = {
    {
        .name = TYPE_HEXAGON_CPU,
        .parent = TYPE_CPU,
        .instance_size = sizeof(HexagonCPU),
        .instance_init = hexagon_cpu_init,
        .abstract = true,
        .class_size = sizeof(HexagonCPUClass),
        .class_init = hexagon_cpu_class_init,
    },
#ifdef CONFIG_USER_ONLY
    DEFINE_CPU(TYPE_HEXAGON_CPU_ANY,  hexagon_v67_cpu_init), /* Default to v67 */
    DEFINE_CPU(TYPE_HEXAGON_CPU_V66,  hexagon_v66_cpu_init),
#else
    DEFINE_CPU(TYPE_HEXAGON_CPU_ANY,  hexagon_common_cpu_init),
#endif
    DEFINE_CPU(TYPE_HEXAGON_CPU_V67,  hexagon_v67_cpu_init),
};

DEFINE_TYPES(hexagon_cpu_type_infos)
