/*
 * libqemu
 *
 * Copyright (c) 2021 Luc Michel <luc.michel@greensocs.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "qemu/osdep.h"

#include "riscv.h"
#include "cpu-qom.h"
#include "riscv-callbacks.h"
#include "system/reset.h"

typedef struct MeipCallbackState {
    LibQemuRiscvMipUpdateCallbackFn cb;
    void *opaque;
} MeipCallbackState;

static MeipCallbackState meip_cb_state;

void libqemu_cpu_riscv_register_mip_update_callback(LibQemuRiscvMipUpdateCallbackFn cb,
                                                  void *opaque)
{
    meip_cb_state.cb = cb;
    meip_cb_state.opaque = opaque;
}

void libqemu_cpu_riscv_mip_update_cb(CPUState *cpu, uint32_t value)
{
    if (meip_cb_state.cb == NULL) {
        return;
    }

    meip_cb_state.cb((QemuObject *)cpu, value, meip_cb_state.opaque);
}

static void do_cpu_reset(void *opaque)
{
    RISCVCPU *cpu = opaque;
    CPUState *cs = CPU(cpu);
    cpu_reset(cs);
}


void libqemu_cpu_riscv_register_reset(Object *cpu)
{
    qemu_register_reset(do_cpu_reset, RISCV_CPU(cpu));
}
