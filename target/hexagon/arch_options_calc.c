/*
 *  Copyright(c) 2019-2020 Qualcomm Innovation Center, Inc. All Rights Reserved.
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

#include "qemu/osdep.h"
#include "cpu.h"
#define thread_t CPUHexagonState
#include "arch_options_calc.h"

int get_ext_contexts(processor_t *proc)
{
    int ext_contexts = 0;
    if (proc->arch_proc_options->QDSP6_VX_PRESENT) {
        ext_contexts = proc->arch_proc_options->QDSP6_VX_CONTEXTS;
    }
    return ext_contexts;
}
