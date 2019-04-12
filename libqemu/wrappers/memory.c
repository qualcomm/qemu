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

#include "qemu/osdep.h"
#include "exec/memory.h"

#include "memory.h"

MemoryRegionOps * libqemu_mr_ops_new(void)
{
    return g_new0(MemoryRegionOps, 1);
}

void libqemu_mr_ops_free(MemoryRegionOps *ops)
{
    g_free(ops);
}

void libqemu_mr_ops_set_read_cb(MemoryRegionOps *ops, LibQemuMrReadCb cb)
{
    ops->read_with_attrs = cb;
}

void libqemu_mr_ops_set_write_cb(MemoryRegionOps *ops, LibQemuMrWriteCb cb)
{
    ops->write_with_attrs = cb;
}

MemoryRegion* libqemu_memory_region_new(void)
{
    return g_new0(MemoryRegion, 1);
}
