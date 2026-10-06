/*
 * Hexagon User-DMA Engine
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef HEXAGON_DMA_H
#define HEXAGON_DMA_H

#include "cpu-qom.h"
#include "exec/target_long.h"

/* DM0 status, as observed by dmpoll/dmwait/dmpause. */
#define DM0_STATUS_IDLE   0x00000000
#define DM0_STATUS_RUN    0x00000001
#define DM0_STATUS_ERROR  0x00000002

typedef struct HexagonDMAState {
    /* DM0: idle / run / error, observed by dmpoll/dmwait/dmpause. */
    uint32_t status;
    /* DM1-shaped syndrome for the last error, valid while status is error. */
    uint32_t syndrome;
    /* Guest VA of the descriptor last started/resumed from. */
    uint32_t desc_ptr;
} HexagonDMAState;

void hexagon_dma_reset(HexagonDMAState *dma);

/* The "dma-version" CPU property has not been specified. */
#define HEX_DMA_VERSION_UNSET UINT32_MAX

/*
 * User-DMA architecture version: 1, 2, 4, 6 or 8, or 0 if the core has no
 * User DMA engine.  System emulation takes it from the config table and
 * treats an unspecified version as no engine.
 */
uint32_t hexagon_dma_version(HexagonCPU *cpu);

#ifdef CONFIG_USER_ONLY
/* linux-user has no config table: the version follows the CPU model. */
uint32_t hexagon_dma_model_version(HexagonCPU *cpu);
#endif

/* Hardware thread id of the CPU, for tracing and DM4. */
uint32_t hexagon_dma_htid(HexagonCPU *cpu);

/*
 * Run the descriptor chain starting at desc_va to completion.  Guest memory
 * faults during descriptor or payload access longjmp out through the normal
 * exception path; ra is the helper's return address for that unwind.
 */
void hexagon_dma_run_chain(HexagonCPU *cpu, target_ulong desc_va,
                           uintptr_t ra);

/* Patch tail_va's next-descriptor field to point at new_va. */
void hexagon_dma_link(HexagonCPU *cpu, target_ulong tail_va,
                      target_ulong new_va, uintptr_t ra);

/* DMPause: return the status and return the engine to idle. */
uint32_t hexagon_dma_pause(HexagonCPU *cpu);

/* Monitor-mode config-space access (DMCfgRd/DMCfgWr). */
uint32_t hexagon_dma_cfg_read(HexagonCPU *cpu, uint32_t index);
void hexagon_dma_cfg_write(HexagonCPU *cpu, uint32_t index, uint32_t value);

#endif /* HEXAGON_DMA_H */
