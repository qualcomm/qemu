/*
 * Stubs for i386 Xen HVM functions, when no i386 target is built
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "hw/xen/arch_hvm.h"

void xen_i386_handle_ioreq(XenIOState *state, ioreq_t *req)
{
    g_assert_not_reached();
}

void xen_i386_set_memory(XenIOState *state, MemoryRegionSection *section,
                         bool add)
{
    g_assert_not_reached();
}

void xen_i386_hvm_modified_memory(ram_addr_t start, ram_addr_t length)
{
    g_assert_not_reached();
}

void xen_i386_set_global_dirty_log(bool enable, Error **errp)
{
    g_assert_not_reached();
}
