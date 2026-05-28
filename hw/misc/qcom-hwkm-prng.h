/*
 * QEMU device header: HWKM PRNG emulator
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef HWKM_PRNG_H
#define HWKM_PRNG_H

#include "hw/core/sysbus.h"

#define HWKM_PRNG_SIZE 0x1000
#define TYPE_HWKM_PRNG "hwkm-prng"

OBJECT_DECLARE_SIMPLE_TYPE(HwkmPrngState, HWKM_PRNG)

/* Device state structure forward declaration */
typedef struct HwkmPrngState HwkmPrngState;

struct HwkmPrngState {
    SysBusDevice parent_obj;
    MemoryRegion iomem;
    uint32_t fifo_data;
    bool data_avail;
};

#endif /* HWKM_PRNG_H */
