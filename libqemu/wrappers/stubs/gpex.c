/*
 * libqemu
 *
 * Copyright (c) 2021 Greensocs
 * Author Damien Hedde
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
#include "qapi/error.h"
#include "hw/sysbus.h"

#include "../gpex.h"

int libqemu_gpex_set_irq_num(SysBusDevice *s, int index, int gsi)
{
    g_assert(false); /* gpex_set_irq_num is not shipped in */
    return -1;
}
