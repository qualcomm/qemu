/*
 *  Copyright(c) 2023-2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifdef COPROC_STANDALONE_BUILD
#include "hex_arch_types.h"
#else
#include "qemu/osdep.h"
#include "exec/exec-all.h"
#include "exec/cpu_ldst.h"
#include "cpu.h"
#include "trace.h"
#endif
#include "coproc.h"

/* this is called from the client side */
void coproc(const CoprocArgs *args)
{
#if !defined(CONFIG_USER_ONLY) && !defined(_WIN32)
    trace_hexagon_coproc_op(args->opcode);
    hexagon_coproc_rpclib_call((const void *)args);
#endif
}

