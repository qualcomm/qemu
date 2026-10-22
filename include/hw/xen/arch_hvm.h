#ifndef HW_XEN_ARCH_HVM_H
#define HW_XEN_ARCH_HVM_H

#include <xen/hvm/ioreq.h>
#include "hw/xen/xen-hvm-common.h"

/* i386 implementations, see dispatchers in xen-hvm-common.c */
void xen_i386_handle_ioreq(XenIOState *state, ioreq_t *req);
void xen_i386_set_memory(XenIOState *state,
                         MemoryRegionSection *section,
                         bool add);
void xen_i386_hvm_modified_memory(ram_addr_t start, ram_addr_t length);
void xen_i386_set_global_dirty_log(bool enable, Error **errp);
#endif
