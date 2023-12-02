/*
 *  Copyright(c) 2022 Qualcomm Innovation Center, Inc. All Rights Reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef HEX_INTERRUPTS_H
#define HEX_INTERRUPTS_H

bool hex_check_interrupts(CPUHexagonState *env);
void hex_clear_interrupts(CPUHexagonState *env, uint32_t mask, uint32_t type);
void hex_raise_interrupts(CPUHexagonState *env, uint32_t mask, uint32_t type);
void hex_interrupt_update(CPUHexagonState *env);

#endif
