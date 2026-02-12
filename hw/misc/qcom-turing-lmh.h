/*
 * Qualcomm Turing QDSP6SS LMH (Local Limit Manager)
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
    uint32_t lmh_ctl;     /* LMH_CTL: RW, bit 0 = LLM_DISABLE_REQ */
    uint32_t lmh_status;  /* LMH_STATUS: R, bit 0 = LLM_DISABLE_ACK */
};

#endif /* QCOM_TURING_LMH_H */
