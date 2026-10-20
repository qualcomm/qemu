/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef HEXSWI_H
#define HEXSWI_H


#include "cpu.h"

void hexagon_cpu_do_interrupt(CPUState *cpu);
void register_trap_exception(CPUHexagonState *env, int type, int imm,
                             uint32_t PC);
void hexagon_vmrte(CPUHexagonState *env);
/* Take a trap0(#0) semihosting call without vectoring to the monitor. */
void hexagon_semihost_trap0(CPUHexagonState *env);
/* The guest turned its virtual interrupts on; one may be waiting for it. */
void hexagon_hvm_ie_enabled(CPUHexagonState *env);

/* Offsets of the guest event vectors from GEVB, as the VM interface has them */
#define HEX_GEVB_ERROR      8
#define HEX_GEVB_TRAP0      20
#define HEX_GEVB_INTERRUPT  28

#endif /* HEXSWI_H */
