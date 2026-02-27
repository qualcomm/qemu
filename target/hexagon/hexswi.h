/*
 * Copyright(c) 2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef HEXSWI_H
#define HEXSWI_H


#include "cpu.h"

void hexagon_cpu_do_interrupt(CPUState *cpu);
void register_trap_exception(CPUHexagonState *env, int type, int imm,
                             target_ulong PC);
void guest_event_entry(CPUHexagonState *env, uint32_t cause,
                       target_ulong event_pc, int guest_event_num,
                       bool set_gbadva);
void hexagon_vmrte(CPUHexagonState *env);

#endif /* HEXSWI_H */
