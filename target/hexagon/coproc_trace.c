/*
 *  Copyright(c) 2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef CONFIG_USER_ONLY

#include "qemu/osdep.h"
#include "coproc.h"
#include "trace.h"

void coproc_trace_op(int opcode)
{
    trace_hexagon_coproc_op(opcode);
}

#else

void coproc_trace_op(int opcode) {}

#endif
