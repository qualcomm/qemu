/*
 * Hexagon User-DMA instruction helpers
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "cpu.h"
#include "dma.h"
#include "exec/helper-proto.h"
#include "trace.h"

void HELPER(dmstart)(CPUHexagonState *env, uint32_t RsV)
{
    trace_hexagon_dma_start(hexagon_dma_htid(env_archcpu(env)), RsV);
    hexagon_dma_run_chain(env_archcpu(env), RsV, GETPC());
}

void HELPER(dmresume)(CPUHexagonState *env, uint32_t RsV)
{
    trace_hexagon_dma_resume(hexagon_dma_htid(env_archcpu(env)), RsV);
    hexagon_dma_run_chain(env_archcpu(env), RsV & ~0xf, GETPC());
}

/* dmlink(Rs, Rt): Rs is the tail of the chain, Rt the descriptor to append. */
void HELPER(dmlink)(CPUHexagonState *env, uint32_t RsV, uint32_t RtV)
{
    HexagonCPU *cpu = env_archcpu(env);

    hexagon_dma_link(cpu, RsV, RtV, GETPC());
    if (cpu->dma.status == DM0_STATUS_IDLE) {
        hexagon_dma_run_chain(cpu, RtV, GETPC());
    }
}

uint32_t HELPER(dmpoll)(CPUHexagonState *env)
{
    return env_archcpu(env)->dma.status;
}

uint32_t HELPER(dmwait)(CPUHexagonState *env)
{
    /*
     * The engine is synchronous: there is never a "running" state to
     * wait for by the time dmwait executes.
     */
    return env_archcpu(env)->dma.status;
}

uint32_t HELPER(dmpause)(CPUHexagonState *env)
{
    return hexagon_dma_pause(env_archcpu(env));
}

uint32_t HELPER(dmcfgrd)(CPUHexagonState *env, uint32_t index)
{
    return hexagon_dma_cfg_read(env_archcpu(env), index);
}

void HELPER(dmcfgwr)(CPUHexagonState *env, uint32_t index, uint32_t value)
{
    hexagon_dma_cfg_write(env_archcpu(env), index, value);
}
