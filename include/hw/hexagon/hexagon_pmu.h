/*
 * Hexagon PMU QOM Object
 *
 * Models the guest-configurable, cross-thread PMU state: which event each
 * of the 8 counters is watching (PMUEVTCFG/PMUEVTCFG1/PMUCFG) and each
 * counter's rebase offset (so a guest write to PMUCNTn, or switching a
 * counter's event, changes the value read back without discarding the
 * live per-vCPU tally). The live tallies themselves are per-vCPU fast
 * counters embedded in CPUHexagonState (see target/hexagon/pmu.h) --
 * translated code addresses them directly, so they can't live in this
 * device. This device is optional: a CPU's "pmu" link property is unset
 * unless the machine was started with `pmu=on`, and every access is
 * NULL-checked so an absent PMU costs nothing beyond that check.
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef HEXAGON_PMU_H
#define HEXAGON_PMU_H

#include "hw/core/sysbus.h"
#include "qom/object.h"
#include "target/hexagon/cpu.h"
#include "target/hexagon/pmu.h"

#define TYPE_HEXAGON_PMU "hexagon-pmu"
OBJECT_DECLARE_SIMPLE_TYPE(HexagonPMUState, HEXAGON_PMU)

struct HexagonPMUState {
    SysBusDevice parent_obj;

    /* Event selected for each of the NUM_PMU_CTRS counters. */
    uint16_t events[NUM_PMU_CTRS];

    /* Per-counter rebase offset; see hexagon_get_pmu_counter(). */
    uint32_t ctr_offset[NUM_PMU_CTRS];

    /* PMUCFG thread mask; only mask 0 (all threads) is implemented. */
    uint32_t thread_mask;

    /* Raw PMUSTID0/PMUSTID1; not yet implemented beyond being stored. */
    uint32_t stid[2];
};

uint16_t hexagon_pmu_get_event(HexagonPMUState *s, unsigned int index);
void hexagon_pmu_set_event(HexagonPMUState *s, unsigned int index,
                           uint16_t event);
uint32_t hexagon_pmu_get_offset(HexagonPMUState *s, unsigned int index);
void hexagon_pmu_set_offset(HexagonPMUState *s, unsigned int index,
                            uint32_t offset);
uint32_t hexagon_pmu_get_thread_mask(HexagonPMUState *s);
void hexagon_pmu_set_thread_mask(HexagonPMUState *s, uint32_t mask);
uint32_t hexagon_pmu_get_stid(HexagonPMUState *s, unsigned int index);
void hexagon_pmu_set_stid(HexagonPMUState *s, unsigned int index,
                          uint32_t value);

#endif /* HEXAGON_PMU_H */
