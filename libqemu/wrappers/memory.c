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

extern AddressSpace address_space_memory;

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

void libqemu_mr_ops_max_access_size(MemoryRegionOps *ops, unsigned size)
{
    ops->valid.max_access_size = size;
    ops->impl.max_access_size = size;
}

MemoryRegion* libqemu_memory_region_new(void)
{
    return g_new0(MemoryRegion, 1);
}

MemTxResult libqemu_memory_region_dispatch_read(MemoryRegion *mr, hwaddr addr, uint64_t *data,
                                                unsigned int size, MemTxAttrs attrs)
{
    MemOp op = size_memop(size);
    return memory_region_dispatch_read(mr, addr, data, op, attrs);
}

MemTxResult libqemu_memory_region_dispatch_write(MemoryRegion *mr, hwaddr addr, uint64_t data,
                                                unsigned int size, MemTxAttrs attrs)
{
    MemOp op = size_memop(size);
    return memory_region_dispatch_write(mr, addr, data, op, attrs);
}

static MemTxResult libqemu_read_generic_cb(void *opaque,
        hwaddr addr, uint64_t *data, unsigned int size, MemTxAttrs attrs);
static MemTxResult libqemu_write_generic_cb(void *opaque,
        hwaddr addr, uint64_t data, unsigned int size, MemTxAttrs attrs);

typedef struct MemOpsWrapper {
    LibQemuMrReadCb read_cb;
    LibQemuMrWriteCb write_cb;
    void *opaque;

    MemoryRegionOps ops;
} MemOpsWrapper;

static void do_io_access(CPUCoroutineIOInfo *io)
{
    MemOpsWrapper *ops;

    ops = (MemOpsWrapper *) io->opaque;

    if (io->is_read) {
        io->result = ops->read_cb(ops->opaque, io->addr, io->data, io->size, *io->attrs);
    } else {
        io->result = ops->write_cb(ops->opaque, io->addr, *io->data, io->size, *io->attrs);
    }

    io->done = true;
}

static MemTxResult handle_async_mem_access(bool is_read, void *opaque,
        hwaddr addr, uint64_t *data, unsigned int size, MemTxAttrs attrs)
{
    CPUCoroutineIOInfo io;

    io.is_read = is_read;
    io.addr = addr;
    io.data = data;
    io.size = size;
    io.attrs = &attrs;
    io.opaque = opaque;
    io.done = false;

    do_io_access(&io);

    return io.result;
}

static MemTxResult libqemu_read_generic_cb(void *opaque,
        hwaddr addr, uint64_t *data, unsigned int size, MemTxAttrs attrs)
{
    CPUCoroutineIOInfo *io;

    if (current_cpu == NULL) {
        return handle_async_mem_access(true, opaque, addr, data, size, attrs);
    }

    current_cpu->coroutine_yield_info.reason = YIELD_IO;
    io = &current_cpu->coroutine_yield_info.io_info;

    io->is_read = true;
    io->addr = addr;
    io->data = data;
    io->size = size;
    io->attrs = &attrs;
    io->opaque = opaque;
    io->done = false;

    if (coroutine_tcg) {
#ifndef _WIN32
        qemu_coroutine_yield();
        assert(io->done);
#else
        while (!io->done) {
            qemu_coroutine_yield();
        }
#endif
    } else {
        libqemu_cpu_do_io();
    }

    current_cpu->coroutine_yield_info.reason = YIELD_LOOP_END;
    return io->result;
}

static MemTxResult libqemu_write_generic_cb(void *opaque,
        hwaddr addr, uint64_t data, unsigned int size, MemTxAttrs attrs)
{
    CPUCoroutineIOInfo *io;

    if (current_cpu == NULL) {
        return handle_async_mem_access(false, opaque, addr, &data, size, attrs);
    }

    current_cpu->coroutine_yield_info.reason = YIELD_IO;
    io = &current_cpu->coroutine_yield_info.io_info;

    io->is_read = false;
    io->addr = addr;
    io->data = &data;
    io->size = size;
    io->attrs = &attrs;
    io->opaque = opaque;
    io->done = false;

    if (coroutine_tcg) {
#ifndef _WIN32
        qemu_coroutine_yield();
        assert(io->done);
#else
        while (!io->done) {
            qemu_coroutine_yield();
        }
#endif
    } else {
        libqemu_cpu_do_io();
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
    wrapper->ops.valid.max_access_size = ops->valid.max_access_size;
    wrapper->ops.impl.max_access_size = ops->impl.max_access_size;
    wrapper->opaque = opaque;

    wrapper->ops.read_with_attrs = libqemu_read_generic_cb;
    wrapper->ops.write_with_attrs = libqemu_write_generic_cb;

    memory_region_init_io(mr, obj, &wrapper->ops, wrapper, name, size);
}

void libqemu_cpu_do_io(void)
{
    CPUCoroutineIOInfo *io;

    io = &current_cpu->coroutine_yield_info.io_info;
    do_io_access(io);
}

AddressSpace *libqemu_address_space_new(void)
{
    return g_new0(AddressSpace, 1);
}

void libqemu_address_space_free(AddressSpace *as)
{
    g_free(as);
}

AddressSpace *libqemu_address_space_get_system_memory(void)
{
    return &address_space_memory;
}

MemoryListener *libqemu_memory_listener_new(void *opaque, const char *name)
{
    MemoryListener *ret = g_new0(MemoryListener, 1);
    ret->opaque = opaque;
    ret->name = name;
    return ret;
}

void libqemu_memory_listener_free(MemoryListener *ml)
{
    g_free(ml);
}

void libqemu_memory_listener_set_map_cb(MemoryListener *ml, LibQemuMlMapCb cb)
{
    ml->map = cb;
}
