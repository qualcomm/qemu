/*
 * QCOM Turing QDSP6SS register block
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef QCOM_TURING_LMH_H
#define QCOM_TURING_LMH_H

#include "hw/core/sysbus.h"

#define TYPE_TURING_LMH "turing-lmh"
#define TURING_LMH_SIZE 0x8
#define TURING_LMH_OFFSET 0x818

OBJECT_DECLARE_SIMPLE_TYPE(TuringLmhState, TURING_LMH)

typedef struct TuringLmhState TuringLmhState;

struct TuringLmhState {
    SysBusDevice parent_obj;
    MemoryRegion iomem;
    uint32_t reg0;
    uint32_t reg1;
};

#endif /* QCOM_TURING_LMH_H */
