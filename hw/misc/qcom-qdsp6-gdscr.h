/*
 * QCOM Turing GDSCR Registers — QEMU sysbus device
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef HW_MISC_QCOM_GDSCR_H
#define HW_MISC_QCOM_GDSCR_H

#include "qemu/osdep.h"
#include "hw/core/sysbus.h"

#define TYPE_QCOM_GDSCR "qcom-turing-gdscr"
OBJECT_DECLARE_SIMPLE_TYPE(QcomTuringGdscrState, QCOM_GDSCR)

/* ------------------------------------------------------------------------- */
#define QCOM_GDSCR_MMIO_SIZE 0x8

/* ------------------------------------------------------------------------- */
/* Device state */
typedef struct QcomTuringGdscrState {
    SysBusDevice parent_obj;
    MemoryRegion mmio;

    uint32_t values[2];
} QcomTuringGdscrState;

#endif /* HW_MISC_QCOM_GDSCR_H */
