/*
 * QEMU RISC-V CLIC Interrupt Controller
 *
 * Copyright(c) Qualcomm Innovation Center, Inc. All Rights Reserved.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef HW_INTC_RISCV_CLIC_H
#define HW_INTC_RISCV_CLIC_H

#include "hw/core/sysbus.h"
#include "hw/core/irq.h"

#define TYPE_RISCV_CLIC "riscv.clic"
OBJECT_DECLARE_SIMPLE_TYPE(RiscvCLICState, RISCV_CLIC)

enum {
    RISCV_CLIC_MAX_SOURCES = 1024,
    RISCV_CLIC_MMIO_SIZE = 0x1000000,
    RISCV_CLICINTIP_OFFSET = 0x800000,
    RISCV_CLICINTIE_OFFSET = 0x800400,
    RISCV_CLICINTCFG_OFFSET = 0x800800,
    RISCV_CLICCFG_OFFSET = 0x800c00,
    RISCV_CLIC_CONTROL_OFFSET = 0x7ff000,
    RISCV_CLIC_WINDOW_SIZE = 0x1000,
};

struct RiscvCLICState {
    SysBusDevice parent_obj;
    MemoryRegion control_mmio;
    MemoryRegion interrupt_mmio;
    uint32_t num_sources;
    uint32_t aperture_size;
    uint32_t cliccfg;
    uint32_t control;
    uint8_t pending[RISCV_CLIC_MAX_SOURCES];
    uint8_t input_level[RISCV_CLIC_MAX_SOURCES];
    uint8_t software_pending[16];
    uint8_t enable[RISCV_CLIC_MAX_SOURCES];
    uint8_t config[RISCV_CLIC_MAX_SOURCES];
    qemu_irq output;
};

static inline int riscv_clic_pending(const RiscvCLICState *s)
{
    int selected = -1;
    for (unsigned int i = 0; i < s->num_sources; i++) {
        if (s->pending[i] && s->enable[i] &&
            (selected < 0 || s->config[i] > s->config[selected])) {
            selected = i;
        }
    }
    return selected;
}

#endif
