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
#include "hw/core/cpu.h"

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

typedef struct MemOpsWrapper {
    LibQemuMrReadCb read_cb;
    LibQemuMrWriteCb write_cb;
    void *opaque;

    MemoryRegionOps ops;
} MemOpsWrapper;

static MemTxResult libqemu_read_generic_cb(void *opaque,
        hwaddr addr, uint64_t *data, unsigned int size, MemTxAttrs attrs)
{
    CPUCoroutineIOInfo *io;

    current_cpu->coroutine_yield_info.reason = YIELD_IO;
    io = &current_cpu->coroutine_yield_info.io_info;

    io->is_read = true;
    io->addr = addr;
    io->data = data;
    io->size = size;
    io->attrs = &attrs;
    io->opaque = opaque;
    io->done = false;

    while (!io->done) {
        qemu_coroutine_yield();
    }

    current_cpu->coroutine_yield_info.reason = YIELD_LOOP_END;
    return io->result;
}

static MemTxResult libqemu_write_generic_cb(void *opaque,
        hwaddr addr, uint64_t data, unsigned int size, MemTxAttrs attrs)
{
    CPUCoroutineIOInfo *io;

    current_cpu->coroutine_yield_info.reason = YIELD_IO;
    io = &current_cpu->coroutine_yield_info.io_info;

    io->is_read = false;
    io->addr = addr;
    io->data = &data;
    io->size = size;
    io->attrs = &attrs;
    io->opaque = opaque;
    io->done = false;

    while (!io->done) {
        qemu_coroutine_yield();
    }

    current_cpu->coroutine_yield_info.reason = YIELD_LOOP_END;
    return io->result;
}

void libqemu_memory_region_init_io(MemoryRegion *mr, Object *obj, const MemoryRegionOps *ops,
                                   void *opaque, const char *name, uint64_t size)
{
    MemOpsWrapper *wrapper = g_new0(MemOpsWrapper, 1);

    wrapper->read_cb = ops->read_with_attrs;
    wrapper->write_cb = ops->write_with_attrs;
    wrapper->opaque = opaque;

    wrapper->ops.read_with_attrs = libqemu_read_generic_cb;
    wrapper->ops.write_with_attrs = libqemu_write_generic_cb;

    memory_region_init_io(mr, obj, &wrapper->ops, wrapper, name, size);
}

void libqemu_cpu_do_io(void)
{
    CPUCoroutineIOInfo *io;
    MemOpsWrapper *ops;

    io = &current_cpu->coroutine_yield_info.io_info;
    ops = (MemOpsWrapper *) io->opaque;

    if (io->is_read) {
        io->result = ops->read_cb(ops->opaque, io->addr, io->data, io->size, *io->attrs);
    } else {
        io->result = ops->write_cb(ops->opaque, io->addr, *io->data, io->size, *io->attrs);
    }

    io->done = true;
}
