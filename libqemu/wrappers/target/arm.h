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

#ifndef _LIBQEMU_WRAPPERS_TARGET_ARM_H
#define _LIBQEMU_WRAPPERS_TARGET_ARM_H

#include <stdbool.h>
#include <stdint.h>

void libqemu_cpu_arm_set_cp15_cbar(Object *cpu, uint64_t cbar);
void libqemu_cpu_aarch64_set_aarch64_mode(Object *cpu, bool aarch64_mode);

void libqemu_cpu_arm_add_nvic_link(Object *cpu);
void libqemu_arm_nvic_add_cpu_link(Object *cpu);

uint64_t libqemu_cpu_arm_get_exclusive_addr(const Object *cpu);
uint64_t libqemu_cpu_arm_get_exclusive_val(const Object *cpu);
void libqemu_cpu_arm_set_exclusive_val(Object *cpu, uint64_t val);

#endif
