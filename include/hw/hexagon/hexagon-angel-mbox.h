/*
 * Hexagon H2 hypervisor angel semihosting mailbox stub
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef HW_HEXAGON_ANGEL_MBOX_H
#define HW_HEXAGON_ANGEL_MBOX_H

#include "hw/core/sysbus.h"

#define TYPE_HEXAGON_ANGEL_MBOX "hexagon-angel-mbox"
OBJECT_DECLARE_SIMPLE_TYPE(HexagonAngelMboxState, HEXAGON_ANGEL_MBOX)

struct HexagonAngelMboxState {
    SysBusDevice parent_obj;

    MemoryRegion iomem;
};

#endif /* HW_HEXAGON_ANGEL_MBOX_H */
