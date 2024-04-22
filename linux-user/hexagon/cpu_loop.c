/*
 *  qemu user cpu loop
 *
 *  Copyright (c) 2003-2008 Fabrice Bellard
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

#include "qemu/osdep.h"
#include "qemu.h"
#include "user-internals.h"
#include "cpu_loop-common.h"
#include "target/hexagon/internal.h"
#include "signal-common.h"
#include "internal.h"

void cpu_loop(CPUHexagonState *env)
{
    CPUState *cs = env_cpu(env);
    int trapnr, signum, sigcode;
    target_ulong sigaddr;
    target_ulong syscallnum;
    target_ulong ret;

    for (;;) {
        cpu_exec_start(cs);
        trapnr = cpu_exec(cs);
        cpu_exec_end(cs);
        process_queued_cpu_work(cs);
        signum = 0;
        sigcode = 0;
        sigaddr = 0;

        switch (trapnr) {
        case EXCP_INTERRUPT:
            /* just indicate that signals should be handled asap */
            break;
        case HEX_EVENT_TRAP0:
            syscallnum = env->gpr[6];
            env->gpr[HEX_REG_PC] += 4;
#if COUNT_HEX_HELPERS
            if (syscallnum == TARGET_NR_exit_group) {
                print_helper_counts();
            }
#endif
            ret = do_syscall(env,
                             syscallnum,
                             env->gpr[0],
                             env->gpr[1],
                             env->gpr[2],
                             env->gpr[3],
                             env->gpr[4],
                             env->gpr[5],
                             0, 0);
            if (ret == -QEMU_ERESTARTSYS) {
                env->gpr[HEX_REG_PC] -= 4;
            } else if (ret != -QEMU_ESIGRETURN) {
                env->gpr[0] = ret;
            }
            break;
        case HEX_EVENT_TRAP1:
            EXCP_DUMP(env, "\nqemu: trap1 exception %#x - aborting\n",
                     trapnr);
            exit(EXIT_FAILURE);
            break;
        case HEX_EVENT_PRECISE:
            switch(env->cause_code) {
            case HEX_CAUSE_FETCH_NO_UPAGE:
            case HEX_CAUSE_PRIV_NO_UREAD:
            case HEX_CAUSE_PRIV_NO_UWRITE:
            signum = TARGET_SIGSEGV;
            sigcode = TARGET_SEGV_MAPERR;
            break;
            default:
                EXCP_DUMP(env, "\nqemu: unhandled CPU precise exception "
                    "0x%x/0x%x - aborting\n",
                    trapnr, env->cause_code);
                exit(EXIT_FAILURE);
            }
            break;
        case EXCP_ATOMIC:
            cpu_exec_step_atomic(cs);
            break;
        case EXCP_DEBUG:
            force_sig_fault(TARGET_SIGTRAP, TARGET_TRAP_BRKPT, 0);
            break;
        case EXCP_YIELD:
            /* nothing to do here for user-mode, just resume guest code */
            break;
        default:
            EXCP_DUMP(env, "\nqemu: unhandled CPU exception "
                "0x%x/0x%x - aborting\n",
                trapnr, env->cause_code);
            exit(EXIT_FAILURE);
        }
        if (signum) {
            target_siginfo_t info = {
                .si_signo = signum,
                .si_errno = 0,
                .si_code = sigcode,
                ._sifields._sigfault._addr = sigaddr
            };
            queue_signal(env, info.si_signo, QEMU_SI_KILL, &info);
        }
        process_pending_signals(env);
    }
}

void target_cpu_copy_regs(CPUArchState *env, struct target_pt_regs *regs)
{
    env->gpr[HEX_REG_PC] = regs->sepc;
    env->gpr[HEX_REG_SP] = regs->sp;
    env->gpr[HEX_REG_USR] = 0x56000;
}
