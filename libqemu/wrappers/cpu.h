/*
 * libqemu
 *
 * Copyright (c) 2019 Luc Michel <luc.michel@greensocs.com>
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

#ifndef _LIBQEMU_WRAPPERS_CPU_H
#define _LIBQEMU_WRAPPERS_CPU_H

#include <stdbool.h>

typedef void (*LibQemuAsyncCpuJobFn)(void *);

void libqemu_cpu_loop(Object *cpu);
bool libqemu_cpu_loop_is_busy(Object *cpu);
bool libqemu_cpu_can_run(Object *cpu);
void libqemu_cpu_register_thread(Object *cpu);
void libqemu_cpu_reset(Object *cpu);
void libqemu_cpu_halt(Object *cpu, bool halted);
void libqemu_cpu_set_soft_stopped(Object *cpu, bool stopped);
bool libqemu_cpu_get_soft_stopped(Object *cpu);
void libqemu_cpu_set_unplug(Object *cpu, bool unplug);

Object *libqemu_current_cpu_get(void);
void libqemu_current_cpu_set(Object *cpu);

void libqemu_async_run_on_cpu(Object *cpu, LibQemuAsyncCpuJobFn, void *arg);
void libqemu_async_safe_run_on_cpu(Object *cpu, LibQemuAsyncCpuJobFn, void *arg);

int libqemu_cpu_get_index(const Object *obj);
uintptr_t libqemu_cpu_get_mem_io_pc(Object *cpu);

void libqemu_vm_stop_paused(void);

#endif
