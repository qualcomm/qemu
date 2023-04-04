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

#ifndef _LIBQEMU_WRAPPERS_MEMORY_H
#define _LIBQEMU_WRAPPERS_MEMORY_H

#include "exec/hwaddr.h"
#include "exec/memattrs.h"

typedef struct MemoryRegionOps MemoryRegionOps;
typedef struct MemoryRegion MemoryRegion;
typedef struct Object Object;
typedef struct AddressSpace AddressSpace;
typedef struct MemoryListener MemoryListener;

typedef MemTxResult (*LibQemuMrReadCb)(void *opaque,
                                       hwaddr addr,
                                       uint64_t *data,
                                       unsigned int size,
                                       MemTxAttrs attrs);

typedef MemTxResult (*LibQemuMrWriteCb)(void *opaque,
                                        hwaddr addr,
                                        uint64_t data,
                                        unsigned int size,
                                        MemTxAttrs attrs);

MemoryRegionOps *libqemu_mr_ops_new(void);
void libqemu_mr_ops_free(MemoryRegionOps *);
void libqemu_mr_ops_set_read_cb(MemoryRegionOps *ops, LibQemuMrReadCb cb);
void libqemu_mr_ops_set_write_cb(MemoryRegionOps *ops, LibQemuMrWriteCb cb);
void libqemu_mr_ops_max_access_size(MemoryRegionOps *ops, unsigned int size);

MemoryRegion* libqemu_memory_region_new(void);
void libqemu_memory_region_init_io(MemoryRegion *mr, Object *obj, const MemoryRegionOps *ops,
                                   void *opaque, const char *name, uint64_t size);

MemTxResult libqemu_memory_region_dispatch_read(MemoryRegion *mr, hwaddr addr, uint64_t *data,
                                                unsigned int size, MemTxAttrs attrs);
MemTxResult libqemu_memory_region_dispatch_write(MemoryRegion *mr, hwaddr addr, uint64_t data,
                                                unsigned int size, MemTxAttrs attrs);

void libqemu_cpu_do_io(void);

AddressSpace *libqemu_address_space_new(void);
void libqemu_address_space_free(AddressSpace *);
AddressSpace *libqemu_address_space_get_system_memory(void);

typedef void (*LibQemuMlMapCb)(void *opaque, hwaddr addr, hwaddr len);

MemoryListener *libqemu_memory_listener_new(void *opaque, const char *name);
void libqemu_memory_listener_free(MemoryListener *ml);
void libqemu_memory_listener_set_map_cb(MemoryListener *ml, LibQemuMlMapCb cb);

#endif
