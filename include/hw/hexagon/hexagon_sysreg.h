/*
 * Hexagon System Registers QOM Object
 *
 * Copyright(c) 2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef HEXAGON_SYSREG_H
#define HEXAGON_SYSREG_H

#include "hw/qdev-core.h"
#include "qom/object.h"
#include "target/hexagon/cpu.h"

#define TYPE_HEXAGON_SYSREG "hexagon-sysreg"
OBJECT_DECLARE_SIMPLE_TYPE(HexagonSysregState, HEXAGON_SYSREG)

struct HexagonSysregState {
    DeviceState parent_obj;

    /* Array of system registers */
    target_ulong regs[NUM_SREGS];
};

/* Public interface functions */
uint32_t hexagon_sysreg_read(HexagonSysregState *s, uint32_t reg);
void hexagon_sysreg_write(HexagonSysregState *s, uint32_t reg, uint32_t value);
void hexagon_sysreg_reset(HexagonSysregState *s);

#endif /* HEXAGON_SYSREG_H */
