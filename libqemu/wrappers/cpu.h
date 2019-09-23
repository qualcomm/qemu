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

void libqemu_cpu_loop(Object *cpu);
bool libqemu_cpu_loop_is_busy(Object *cpu);
bool libqemu_cpu_can_run(Object *cpu);
void libqemu_cpu_register_thread(Object *cpu);
void libqemu_cpu_reset(Object *cpu);
void libqemu_cpu_halt(Object *cpu, bool halted);

Object *libqemu_current_cpu_get(void);
void libqemu_current_cpu_set(Object *cpu);

#endif
