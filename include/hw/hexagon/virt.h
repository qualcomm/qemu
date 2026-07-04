/*
 * Definitions for hexagon virt board.
 *
 * Copyright (c) 2024-2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef HW_HEXAGONVIRT_H
#define HW_HEXAGONVIRT_H

#include "hw/core/boards.h"
#include "hw/core/clock.h"
#include "hw/timer/qct-qtimer.h"
#include "target/hexagon/cpu.h"

typedef struct HexagonBootInfo {
    hwaddr kernel_start;
    hwaddr kernel_size;
    hwaddr initrd_start;
    hwaddr initrd_size;
    hwaddr image_low_addr;
    hwaddr image_high_addr;
} HexagonBootInfo;

struct HexagonVirtMachineState {
    /*< private >*/
    MachineState parent_obj;

    int fdt_size;
    MemoryRegion *sys;
    MemoryRegion cfgtable;
    MemoryRegion ram;
    MemoryRegion tcm;
    MemoryRegion vtcm;
    MemoryRegion bios;
    DeviceState *l2vic;
    Clock *apb_clk;
    DeviceState *gsregs;
    HexagonBootInfo bootinfo;
    QCTQtimerState *qtimer;
    uint64_t kernel_load_addr;
    hwaddr fdt_addr;
    char *firmware_path;
};

void hexagon_load_fdt(const struct HexagonVirtMachineState *vms);

enum {
    VIRT_UART0,
    VIRT_QUP_UART0,
    VIRT_QTMR0,
    VIRT_QTMR1,
    VIRT_GPT,
    VIRT_MMIO,
    VIRT_FDT,
    VIRT_BOOT,
    VIRT_PLL,
};

#define TYPE_HEXAGON_VIRT_MACHINE MACHINE_TYPE_NAME("virt")
OBJECT_DECLARE_SIMPLE_TYPE(HexagonVirtMachineState, HEXAGON_VIRT_MACHINE)

#endif /* HW_HEXAGONVIRT_H */
