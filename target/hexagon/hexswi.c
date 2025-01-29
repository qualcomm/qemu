/*
 * Copyright(c) 2019-2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "cpu.h"
#include "cpu_helper.h"
#include "exec/helper-proto.h"
#include "qemu/log.h"
#include "qemu/main-loop.h"
#include "arch.h"
#include "internal.h"
#include "macros.h"
#include "sys_macros.h"
#include "tcg/tcg-op.h"
#include "hex_mmu.h"
#include "hexswi.h"

#ifdef CONFIG_USER_ONLY
#error "This file is only used in system emulation"
#endif

#include "semihosting/common-semi.h"

/* non-arm-compatible semihosting calls */
#define HEXAGON_SPECIFIC_SWI_FLAGS \
    DEF_SWI_FLAG(OPEN,             0x01) \
    DEF_SWI_FLAG(WRITEC,           0x03) \
    DEF_SWI_FLAG(WRITE0,           0x04) \
    DEF_SWI_FLAG(ISTTY,            0x09) \
    DEF_SWI_FLAG(HEAPINFO,         0x16) \
    DEF_SWI_FLAG(EXCEPTION,        0x18) \
    DEF_SWI_FLAG(READ_CYCLES,      0x40) \
    DEF_SWI_FLAG(PROF_ON,          0x41) \
    DEF_SWI_FLAG(PROF_OFF,         0x42) \
    DEF_SWI_FLAG(WRITECREG,        0x43) \
    DEF_SWI_FLAG(READ_TCYCLES,     0x44) \
    DEF_SWI_FLAG(LOG_EVENT,        0x45) \
    DEF_SWI_FLAG(REDRAW,           0x46) \
    DEF_SWI_FLAG(READ_ICOUNT,      0x47) \
    DEF_SWI_FLAG(PROF_STATSRESET,  0x48) \
    DEF_SWI_FLAG(DUMP_PMU_STATS,   0x4a) \
    DEF_SWI_FLAG(READ_PCYCLES,     0x52) \
    DEF_SWI_FLAG(COREDUMP,         0xCD) \
    DEF_SWI_FLAG(FTELL,            0x100) \
    DEF_SWI_FLAG(FSTAT,            0x101) \
    DEF_SWI_FLAG(STAT,             0x103) \
    DEF_SWI_FLAG(GETCWD,           0x104) \
    DEF_SWI_FLAG(ACCESS,           0x105) \
    DEF_SWI_FLAG(OPENDIR,          0x180) \
    DEF_SWI_FLAG(CLOSEDIR,         0x181) \
    DEF_SWI_FLAG(READDIR,          0x182) \
    DEF_SWI_FLAG(EXEC,             0x185) \
    DEF_SWI_FLAG(FTRUNC,           0x186)

/*
 * We use the arm-compatible semihosting routines for these ones, but we do
 * need some hexagon-specific preprocessing.
 */
#define HEX_SYS_WRITE       0x05
#define HEX_SYS_READ        0x06
#define HEX_SYS_READC       0x07

#define DEF_SWI_FLAG(name, val) HEX_SYS_ ##name = val,
enum hex_swi_flag {
    HEXAGON_SPECIFIC_SWI_FLAGS
};
#undef DEF_SWI_FLAG

#define DEF_SWI_FLAG(_, val) case val:
static inline bool is_hexagon_specific_swi_flag(enum hex_swi_flag what_swi)
{
    switch (what_swi) {
    HEXAGON_SPECIFIC_SWI_FLAGS
        return true;
    }
    return false;
}
#undef DEF_SWI_FLAG

static void do_preload(CPUHexagonState *env, target_ulong swi_info, bool load)
{
    uint32_t addr, count;
    uintptr_t retaddr = 0;
    hexagon_read_memory(env, swi_info + 4, 4, &addr, retaddr);
    hexagon_read_memory(env, swi_info + 8, 4, &count, retaddr);
    hexagon_touch_memory(env, addr, count, load ? MMU_DATA_LOAD : MMU_DATA_STORE);
}

static void sim_handle_trap0(CPUHexagonState *env)
{
    g_assert(bql_locked());
    target_ulong what_swi = arch_get_thread_reg(env, HEX_REG_R00);
    target_ulong swi_info = arch_get_thread_reg(env, HEX_REG_R01);

    if (!is_hexagon_specific_swi_flag(what_swi)) {
        if (what_swi == HEX_SYS_READ || what_swi == HEX_SYS_READC ||
            what_swi == HEX_SYS_WRITE) {
            /*
             * Avoid page faults if the buffer is not in memory yet.
             * NOTE: Counterintuitive, but a WRITE must be able to LOAD from
             * the input address. The contents of that buffer will be
             * directed to the SWI interface.
             */
            do_preload(env, swi_info, (what_swi == HEX_SYS_WRITE));
        }
        CPUState *cs = env_cpu(env);
        do_common_semihosting(cs);
        return;
    }

    switch (what_swi) {

    case HEX_SYS_EXCEPTION:
        arch_set_system_reg(env, HEX_SREG_MODECTL, 0);
        exit(arch_get_thread_reg(env, HEX_REG_R02));
        break;

    /* TODO: implement other hexagon-specific semihosting calls */

    default:
        qemu_log_mask(LOG_GUEST_ERROR, "unknown swi request: 0x%x\n", what_swi);
        cpu_abort(env_cpu(env), "Hexagon Unsupported swi call 0x%x\n", what_swi);
    }
}

static void set_addresses(CPUHexagonState *env, target_ulong pc_offset,
                          target_ulong exception_index)

{
    HexagonCPU *cpu = env_archcpu(env);
    uint32_t evb = cpu->globalregs ?
        arch_get_system_reg(env, HEX_SREG_EVB) :
        cpu->boot_addr;
    arch_set_system_reg(env, HEX_SREG_ELR,
                        arch_get_thread_reg(env, HEX_REG_PC) + pc_offset);
    arch_set_thread_reg(env, HEX_REG_PC, evb | (exception_index << 2));
}

static const char *event_name[] = {
    [HEX_EVENT_RESET] = "HEX_EVENT_RESET",
    [HEX_EVENT_IMPRECISE] = "HEX_EVENT_IMPRECISE",
    [HEX_EVENT_TLB_MISS_X] = "HEX_EVENT_TLB_MISS_X",
    [HEX_EVENT_TLB_MISS_RW] = "HEX_EVENT_TLB_MISS_RW",
    [HEX_EVENT_TRAP0] = "HEX_EVENT_TRAP0",
    [HEX_EVENT_TRAP1] = "HEX_EVENT_TRAP1",
    [HEX_EVENT_FPTRAP] = "HEX_EVENT_FPTRAP",
    [HEX_EVENT_DEBUG] = "HEX_EVENT_DEBUG",
    [HEX_EVENT_INT0] = "HEX_EVENT_INT0",
    [HEX_EVENT_INT1] = "HEX_EVENT_INT1",
    [HEX_EVENT_INT2] = "HEX_EVENT_INT2",
    [HEX_EVENT_INT3] = "HEX_EVENT_INT3",
    [HEX_EVENT_INT4] = "HEX_EVENT_INT4",
    [HEX_EVENT_INT5] = "HEX_EVENT_INT5",
    [HEX_EVENT_INT6] = "HEX_EVENT_INT6",
    [HEX_EVENT_INT7] = "HEX_EVENT_INT7",
    [HEX_EVENT_INT8] = "HEX_EVENT_INT8",
    [HEX_EVENT_INT9] = "HEX_EVENT_INT9",
    [HEX_EVENT_INTA] = "HEX_EVENT_INTA",
    [HEX_EVENT_INTB] = "HEX_EVENT_INTB",
    [HEX_EVENT_INTC] = "HEX_EVENT_INTC",
    [HEX_EVENT_INTD] = "HEX_EVENT_INTD",
    [HEX_EVENT_INTE] = "HEX_EVENT_INTE",
    [HEX_EVENT_INTF] = "HEX_EVENT_INTF"
};

void hexagon_cpu_do_interrupt(CPUState *cs)

{
    CPUHexagonState *env = cpu_env(cs);
    uint32_t ssr;

    BQL_LOCK_GUARD();

    qemu_log_mask(CPU_LOG_INT, "\t%s: event 0x%x:%s, cause 0x%x(%d)\n",
                  __func__, cs->exception_index,
                  event_name[cs->exception_index], env->cause_code,
                  env->cause_code);

    env->llsc_addr = ~0;

    ssr = arch_get_system_reg(env, HEX_SREG_SSR);
    if (GET_SSR_FIELD(SSR_EX, ssr) == 1) {
        arch_set_system_reg(env, HEX_SREG_DIAG, env->cause_code);
        env->cause_code = HEX_CAUSE_DOUBLE_EXCEPT;
        cs->exception_index = HEX_EVENT_PRECISE;
    }

    switch (cs->exception_index) {
    case HEX_EVENT_TRAP0:
        if (env->cause_code == 0) {
            sim_handle_trap0(env);
        }

        hexagon_ssr_set_cause(env, env->cause_code);
        set_addresses(env, 4, cs->exception_index);
        break;

    case HEX_EVENT_TRAP1:
        hexagon_ssr_set_cause(env, env->cause_code);
        set_addresses(env, 4, cs->exception_index);
        break;

    case HEX_EVENT_TLB_MISS_X:
        switch (env->cause_code) {
        case HEX_CAUSE_TLBMISSX_CAUSE_NORMAL:
        case HEX_CAUSE_TLBMISSX_CAUSE_NEXTPAGE:
            qemu_log_mask(CPU_LOG_MMU,
                          "TLB miss EX exception (0x%x) caught: "
                          "Cause code (0x%x) "
                          "TID = 0x%" PRIx32 ", PC = 0x%" PRIx32
                          ", BADVA = 0x%" PRIx32 "\n",
                          cs->exception_index, env->cause_code, env->threadId,
                          arch_get_thread_reg(env, HEX_REG_PC),
                          arch_get_system_reg(env, HEX_SREG_BADVA));

            hexagon_ssr_set_cause(env, env->cause_code);
            set_addresses(env, 0, cs->exception_index);
            break;

        default:
            cpu_abort(cs,
                      "1:Hexagon exception %d/0x%x: "
                      "Unknown cause code %d/0x%x\n",
                      cs->exception_index, cs->exception_index, env->cause_code,
                      env->cause_code);
            break;
        }
        break;

    case HEX_EVENT_TLB_MISS_RW:
        switch (env->cause_code) {
        case HEX_CAUSE_TLBMISSRW_CAUSE_READ:
        case HEX_CAUSE_TLBMISSRW_CAUSE_WRITE:
            qemu_log_mask(CPU_LOG_MMU,
                          "TLB miss RW exception (0x%x) caught: "
                          "Cause code (0x%x) "
                          "TID = 0x%" PRIx32 ", PC = 0x%" PRIx32
                          ", BADVA = 0x%" PRIx32 "\n",
                          cs->exception_index, env->cause_code, env->threadId,
                          env->gpr[HEX_REG_PC],
                          arch_get_system_reg(env, HEX_SREG_BADVA));

            hexagon_ssr_set_cause(env, env->cause_code);
            set_addresses(env, 0, cs->exception_index);
            /* env->sreg[HEX_SREG_BADVA] is set when the exception is raised */
            break;

        default:
            cpu_abort(cs,
                      "2:Hexagon exception %d/0x%x: "
                      "Unknown cause code %d/0x%x\n",
                      cs->exception_index, cs->exception_index, env->cause_code,
                      env->cause_code);
            break;
        }
        break;

    case HEX_EVENT_FPTRAP:
        hexagon_ssr_set_cause(env, env->cause_code);
        arch_set_thread_reg(env, HEX_REG_PC,
                            arch_get_system_reg(env, HEX_SREG_EVB) |
                                (cs->exception_index << 2));
        break;

    case HEX_EVENT_DEBUG:
        hexagon_ssr_set_cause(env, env->cause_code);
        set_addresses(env, 0, cs->exception_index);
        qemu_log_mask(LOG_UNIMP, "single-step exception is not handled\n");
        break;

    case HEX_EVENT_PRECISE:
        switch (env->cause_code) {
        case HEX_CAUSE_FETCH_NO_XPAGE:
        case HEX_CAUSE_FETCH_NO_UPAGE:
        case HEX_CAUSE_PRIV_NO_READ:
        case HEX_CAUSE_PRIV_NO_UREAD:
        case HEX_CAUSE_PRIV_NO_WRITE:
        case HEX_CAUSE_PRIV_NO_UWRITE:
        case HEX_CAUSE_MISALIGNED_LOAD:
        case HEX_CAUSE_MISALIGNED_STORE:
        case HEX_CAUSE_PC_NOT_ALIGNED:
            qemu_log_mask(CPU_LOG_MMU,
                          "MMU permission exception (0x%x) caught: "
                          "Cause code (0x%x) "
                          "TID = 0x%" PRIx32 ", PC = 0x%" PRIx32
                          ", BADVA = 0x%" PRIx32 "\n",
                          cs->exception_index, env->cause_code, env->threadId,
                          env->gpr[HEX_REG_PC],
                          arch_get_system_reg(env, HEX_SREG_BADVA));


            hexagon_ssr_set_cause(env, env->cause_code);
            set_addresses(env, 0, cs->exception_index);
            /* env->sreg[HEX_SREG_BADVA] is set when the exception is raised */
            break;

        case HEX_CAUSE_DOUBLE_EXCEPT:
        case HEX_CAUSE_PRIV_USER_NO_SINSN:
        case HEX_CAUSE_PRIV_USER_NO_GINSN:
        case HEX_CAUSE_INVALID_OPCODE:
        case HEX_CAUSE_NO_COPROC_ENABLE:
        case HEX_CAUSE_NO_COPROC2_ENABLE:
        case HEX_CAUSE_UNSUPORTED_HVX_64B:
        case HEX_CAUSE_REG_WRITE_CONFLICT:
        case HEX_CAUSE_VWCTRL_WINDOW_MISS:
            hexagon_ssr_set_cause(env, env->cause_code);
            set_addresses(env, 0, cs->exception_index);
            break;

        case HEX_CAUSE_COPROC_LDST:
            hexagon_ssr_set_cause(env, env->cause_code);
            set_addresses(env, 0, cs->exception_index);
            break;

        case HEX_CAUSE_STACK_LIMIT:
            hexagon_ssr_set_cause(env, env->cause_code);
            set_addresses(env, 0, cs->exception_index);
            break;

        default:
            cpu_abort(cs,
                      "3:Hexagon exception %d/0x%x: "
                      "Unknown cause code %d/0x%x\n",
                      cs->exception_index, cs->exception_index, env->cause_code,
                      env->cause_code);
            break;
        }
        break;

    case HEX_EVENT_IMPRECISE:
        qemu_log_mask(LOG_UNIMP,
                "Imprecise exception: this case is not yet handled");
        break;

    default:
        qemu_log_mask(LOG_UNIMP,
                "Hexagon Unsupported exception 0x%x/0x%x\n",
                  cs->exception_index, env->cause_code);
        break;
    }

    cs->exception_index = HEX_EVENT_NONE;
}

void register_trap_exception(CPUHexagonState *env, int traptype, int imm,
                             target_ulong PC)
{
    CPUState *cs = env_cpu(env);

    cs->exception_index = (traptype == 0) ? HEX_EVENT_TRAP0 : HEX_EVENT_TRAP1;
    ASSERT_DIRECT_TO_GUEST_UNSET(env, cs->exception_index);

    env->cause_code = imm;
    env->gpr[HEX_REG_PC] = PC;
    cpu_loop_exit(cs);
}
