/*
 * libqemu hexagon
 *
 * Copyright (c) 2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "qemu/osdep.h"
#include "qapi/error.h"
#include "target/hexagon/cpu.h"
#include "hw/qdev-properties.h"
#include "sysemu/reset.h"

#include "hexagon.h"

static void do_cpu_reset(void *opaque)
{
    HexagonCPU *cpu = opaque;
    CPUState *cs = CPU(cpu);
    cpu_reset(cs);
}

void libqemu_cpu_hexagon_register_reset(Object *cpu)
{
    qemu_register_reset(do_cpu_reset, HEXAGON_CPU(cpu));
}
