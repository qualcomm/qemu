/*
 * QEMU Hypervisor.framework support
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 *
 * Contributions after 2012-01-13 are licensed under the terms of the
 * GNU GPL, version 2 or (at your option) any later version.
 */

#include "qemu/osdep.h"
#include "qemu/error-report.h"
#include "qemu/main-loop.h"
#include "qapi/error.h"
#include "qapi/qapi-visit-common.h"
#include "accel/accel-ops.h"
#include "exec/cpu-common.h"
#include "system/address-spaces.h"
#include "system/memory.h"
#include "system/hvf.h"
#include "system/hvf_int.h"
#include "hw/core/cpu.h"
#include "hw/core/boards.h"
#include "trace.h"

bool hvf_allowed;
bool hvf_kernel_irqchip;
bool hvf_nested_virt;
static bool hvf_kernel_irqchip_override;

void hvf_nested_virt_enable(bool nested_virt)
{
    hvf_nested_virt = nested_virt;
}

const char *hvf_return_string(hv_return_t ret)
{
    switch (ret) {
    case HV_SUCCESS:      return "HV_SUCCESS";
    case HV_ERROR:        return "HV_ERROR";
    case HV_BUSY:         return "HV_BUSY";
    case HV_BAD_ARGUMENT: return "HV_BAD_ARGUMENT";
    case HV_NO_RESOURCES: return "HV_NO_RESOURCES";
    case HV_NO_DEVICE:    return "HV_NO_DEVICE";
    case HV_UNSUPPORTED:  return "HV_UNSUPPORTED";
    case HV_DENIED:       return "HV_DENIED";
    default:              return "[unknown hv_return value]";
    }
}

void assert_hvf_ok_impl(hv_return_t ret, const char *file, unsigned int line,
                        const char *exp)
{
    if (ret == HV_SUCCESS) {
        return;
    }

    error_report("Error: %s = %s (0x%x, at %s:%u)",
        exp, hvf_return_string(ret), ret, file, line);

    abort();
}

static void do_hv_vm_protect(hwaddr start, size_t size,
                             hv_memory_flags_t flags)
{
    intptr_t page_mask = qemu_real_host_page_mask();
    hv_return_t ret;

    trace_hvf_vm_protect(start, size, flags,
                         flags & HV_MEMORY_READ  ? 'R' : '-',
                         flags & HV_MEMORY_WRITE ? 'W' : '-',
                         flags & HV_MEMORY_EXEC  ? 'X' : '-');
    hvf_test_trace_protect(start, size, flags,
                           flags & HV_MEMORY_WRITE ? "unprotect-dirty" :
                           "protect-clean");
    g_assert(!((uintptr_t)start & ~page_mask));
    g_assert(!(size & ~page_mask));

    ret = hv_vm_protect(start, size, flags);
    assert_hvf_ok(ret);
}

void hvf_protect_clean_range(hwaddr addr, size_t size)
{
    do_hv_vm_protect(addr, size, HV_MEMORY_READ | HV_MEMORY_EXEC);
}

void hvf_unprotect_dirty_range(hwaddr addr, size_t size)
{
    do_hv_vm_protect(addr, size,
                     HV_MEMORY_READ | HV_MEMORY_WRITE | HV_MEMORY_EXEC);
}

static uint64_t hvf_memory_transaction_id;
static uint64_t hvf_memory_transaction;

typedef struct HvfMapping {
    hwaddr gpa;
    uintptr_t hva;
    hv_memory_flags_t flags;
    GPtrArray *owners; /* MemoryListener* */
} HvfMapping;

static GHashTable *hvf_mappings;

static HvfMapping *hvf_mapping_lookup(hwaddr gpa)
{
    if (!hvf_mappings) {
        return NULL;
    }
    return g_hash_table_lookup(hvf_mappings, &gpa);
}

static void hvf_mapping_free(gpointer opaque)
{
    HvfMapping *mapping = opaque;

    g_ptr_array_unref(mapping->owners);
    g_free(mapping);
}

static void hvf_mapping_insert(hwaddr gpa, uintptr_t hva,
                               hv_memory_flags_t flags,
                               MemoryListener *listener)
{
    HvfMapping *mapping = g_new0(HvfMapping, 1);
    gint64 *key = g_new(gint64, 1);

    *key = gpa;
    mapping->gpa = gpa;
    mapping->hva = hva;
    mapping->flags = flags;
    mapping->owners = g_ptr_array_new();
    g_ptr_array_add(mapping->owners, listener);

    if (!hvf_mappings) {
        hvf_mappings = g_hash_table_new_full(g_int64_hash, g_int64_equal,
                                             g_free, hvf_mapping_free);
    }
    g_hash_table_insert(hvf_mappings, key, mapping);
}

static void hvf_mapping_remove(hwaddr gpa)
{
    gint64 key = gpa;

    g_hash_table_remove(hvf_mappings, &key);
}

static bool hvf_mapping_owned_by(HvfMapping *mapping, MemoryListener *listener)
{
    guint i;

    for (i = 0; i < mapping->owners->len; i++) {
        if (g_ptr_array_index(mapping->owners, i) == listener) {
            return true;
        }
    }
    return false;
}

static void hvf_mapping_add_owner(HvfMapping *mapping, MemoryListener *listener)
{
    if (!hvf_mapping_owned_by(mapping, listener)) {
        g_ptr_array_add(mapping->owners, listener);
    }
}

static bool hvf_mapping_remove_owner(HvfMapping *mapping,
                                     MemoryListener *listener)
{
    guint i;

    for (i = 0; i < mapping->owners->len; i++) {
        if (g_ptr_array_index(mapping->owners, i) == listener) {
            g_ptr_array_remove_index(mapping->owners, i);
            return true;
        }
    }
    return false;
}

static void hvf_mapping_check(HvfMapping *mapping, hwaddr gpa, uintptr_t hva,
                              hv_memory_flags_t flags, const char *name)
{
    if (mapping->hva == hva && mapping->flags == flags) {
        return;
    }

    error_report("HVF: conflicting mapping for gpa 0x%" HWADDR_PRIx
                 " (%s): existing host %p/0x%" PRIx64
                 ", requested host %p/0x%" PRIx64,
                 gpa, name, (void *)mapping->hva, (uint64_t)mapping->flags,
                 (void *)hva, (uint64_t)flags);
    abort();
}

bool hvf_gpa_page_is_mapped(uint64_t gpa)
{
    uint64_t page_size = qemu_real_host_page_size();
    hwaddr page = gpa & ~(hwaddr)(page_size - 1);
    bool mapped;

    assert(bql_locked());
    mapped = hvf_mapping_lookup(page) != NULL;

    trace_hvf_vm_page_mapped(page, page_size, mapped);
    return mapped;
}

static void hvf_map_section(MemoryListener *listener,
                            MemoryRegionSection *section,
                            uintptr_t hva, hv_memory_flags_t flags)
{
    uint64_t page_size = qemu_real_host_page_size();
    uint64_t gpa = section->offset_within_address_space;
    uint64_t size = int128_get64(section->size);
    uint64_t nr_pages = size / page_size;
    const char *name = memory_region_name(section->mr);
    uint64_t i = 0;
    bool reused = false;

    while (i < nr_pages) {
        hwaddr page_gpa = gpa + i * page_size;
        HvfMapping *mapping = hvf_mapping_lookup(page_gpa);
        uint64_t run;
        hv_return_t ret;

        if (mapping) {
            hvf_mapping_check(mapping, page_gpa, hva + i * page_size, flags,
                              name);
            hvf_mapping_add_owner(mapping, listener);
            reused = true;
            i++;
            continue;
        }

        for (run = 1; i + run < nr_pages; run++) {
            if (hvf_mapping_lookup(page_gpa + run * page_size)) {
                break;
            }
        }

        trace_hvf_vm_map(page_gpa, run * page_size,
                         (void *)(hva + i * page_size), flags,
                         flags & HV_MEMORY_READ  ? 'R' : '-',
                         flags & HV_MEMORY_WRITE ? 'W' : '-',
                         flags & HV_MEMORY_EXEC  ? 'X' : '-');
        ret = hv_vm_map((void *)(hva + i * page_size), page_gpa,
                        run * page_size, flags);
        hvf_test_trace_set_phys_mem(listener, section, true,
                                    hvf_memory_transaction,
                                    ret == HV_SUCCESS ? "map-success" :
                                    "map-failed", ret);
        assert_hvf_ok(ret);

        for (uint64_t j = 0; j < run; j++) {
            hvf_mapping_insert(page_gpa + j * page_size,
                               hva + (i + j) * page_size, flags, listener);
        }
        i += run;
    }

    if (reused) {
        hvf_test_trace_set_phys_mem(listener, section, true,
                                    hvf_memory_transaction, "map-success",
                                    HV_SUCCESS);
    }
}

static void hvf_unmap_section(MemoryListener *listener,
                              MemoryRegionSection *section)
{
    uint64_t page_size = qemu_real_host_page_size();
    uint64_t gpa = section->offset_within_address_space;
    uint64_t size = int128_get64(section->size);
    uint64_t nr_pages = size / page_size;
    uint64_t i = 0;

    while (i < nr_pages) {
        hwaddr page_gpa = gpa + i * page_size;
        HvfMapping *mapping = hvf_mapping_lookup(page_gpa);
        uint64_t run;
        hv_return_t ret;

        if (!mapping || !hvf_mapping_owned_by(mapping, listener)) {
            /* Not owned by this listener (e.g. forced delete of a page that
             * was never mapped): nothing to remove. */
            i++;
            continue;
        }

        hvf_mapping_remove_owner(mapping, listener);
        if (mapping->owners->len) {
            /* Other listeners still need this page: keep it mapped. */
            hvf_test_trace_set_phys_mem(listener, section, false,
                                        hvf_memory_transaction, "unmap-retain",
                                        HV_SUCCESS);
            i++;
            continue;
        }

        for (run = 1; i + run < nr_pages; run++) {
            HvfMapping *next = hvf_mapping_lookup(page_gpa + run * page_size);

            if (!next || !hvf_mapping_owned_by(next, listener) ||
                next->owners->len != 1) {
                break;
            }
            hvf_mapping_remove_owner(next, listener);
        }

        trace_hvf_vm_unmap(page_gpa, run * page_size);
        ret = hv_vm_unmap(page_gpa, run * page_size);
        hvf_test_trace_set_phys_mem(listener, section, false,
                                    hvf_memory_transaction,
                                    ret == HV_SUCCESS ? "unmap-success" :
                                    "unmap-failed", ret);
        assert_hvf_ok(ret);

        for (uint64_t j = 0; j < run; j++) {
            hvf_mapping_remove(page_gpa + j * page_size);
        }
        i += run;
    }
}

static void hvf_set_phys_mem(MemoryListener *listener,
                             MemoryRegionSection *section, bool add)
{
    MemoryRegion *area = section->mr;
    bool writable = !area->readonly && !area->rom_device;
    hv_memory_flags_t flags;
    uint64_t page_size = qemu_real_host_page_size();
    uint64_t gpa = section->offset_within_address_space;
    uint64_t size = int128_get64(section->size);
    uintptr_t hva;

    hvf_test_trace_set_phys_mem(listener, section, add,
                                hvf_memory_transaction, "enter", HV_SUCCESS);

    if (!memory_region_is_ram(area)) {
        if (writable) {
            hvf_test_trace_set_phys_mem(listener, section, add,
                                        hvf_memory_transaction, "skip-nonram",
                                        HV_SUCCESS);
            hvf_test_trace_map(gpa, size, "skip-nonram",
                               memory_region_name(area));
            return;
        } else if (!memory_region_is_romd(area)) {
            /*
             * If the memory device is not in romd_mode, then we actually want
             * to remove the hvf memory slot so all accesses will trap.
             */
            hvf_test_trace_set_phys_mem(listener, section, add,
                                        hvf_memory_transaction, "force-delete",
                                        HV_SUCCESS);
             add = false;
        }
    }

    if (!QEMU_IS_ALIGNED(size, page_size) ||
        !QEMU_IS_ALIGNED(gpa, page_size)) {
        /* Not host-page aligned, so do not map or unmap it. */
        hvf_test_trace_set_phys_mem(listener, section, add,
                                    hvf_memory_transaction, "skip-unaligned",
                                    HV_SUCCESS);
        hvf_test_trace_map(gpa, size, "skip-unaligned",
                           memory_region_name(area));
        return;
    }

    if (!add) {
        hvf_test_trace_set_phys_mem(listener, section, add,
                                    hvf_memory_transaction, "unmap-attempt",
                                    -1);
        hvf_test_trace_map(gpa, size, "unmap", memory_region_name(area));
        hvf_unmap_section(listener, section);
        return;
    }

    flags = HV_MEMORY_READ | HV_MEMORY_EXEC | (writable ? HV_MEMORY_WRITE : 0);
    hva = (uintptr_t)memory_region_get_ram_ptr(area) +
          section->offset_within_region;

    hvf_test_trace_set_phys_mem(listener, section, add,
                                hvf_memory_transaction, "map-attempt", -1);
    hvf_test_trace_map(gpa, size, "map", memory_region_name(area));
    hvf_map_section(listener, section, hva, flags);
}

static void hvf_log_start(MemoryListener *listener,
                          MemoryRegionSection *section, int old, int new)
{
    hvf_test_trace_log(listener, section, hvf_memory_transaction,
                       old, new, "log-start");
    assert(new != 0);
    if (old == 0) {
        hvf_protect_clean_range(section->offset_within_address_space,
                                int128_get64(section->size));
    }
}

static void hvf_log_stop(MemoryListener *listener,
                         MemoryRegionSection *section, int old, int new)
{
    hvf_test_trace_log(listener, section, hvf_memory_transaction,
                       old, new, "log-stop");
    assert(old != 0);
    if (new == 0) {
        hvf_unprotect_dirty_range(section->offset_within_address_space,
                                  int128_get64(section->size));
    }
}

static void hvf_log_clear(MemoryListener *listener,
                          MemoryRegionSection *section)
{
    hvf_test_trace_log(listener, section, hvf_memory_transaction,
                       -1, -1, "log-clear");
    /*
     * The dirty page bits within section are being cleared.
     * Some number of those pages may have been dirtied and
     * the write permission enabled.  Reset the range read-only.
     */
    hvf_protect_clean_range(section->offset_within_address_space,
                            int128_get64(section->size));
}

static void hvf_region_add(MemoryListener *listener,
                           MemoryRegionSection *section)
{
    hvf_set_phys_mem(listener, section, true);
}

static void hvf_region_del(MemoryListener *listener,
                           MemoryRegionSection *section)
{
    hvf_set_phys_mem(listener, section, false);
}

static void hvf_begin(MemoryListener *listener)
{
    /* TODO: Called at the beginning of an address space update transaction */
    hvf_memory_transaction = ++hvf_memory_transaction_id;
    trace_hvf_memory_transaction(hvf_memory_transaction,
                                 listener->address_space &&
                                 listener->address_space->name ?
                                 listener->address_space->name : "(none)",
                                 "begin");
}

static void hvf_commit(MemoryListener *listener)
{
    /* TODO: Called at the end of an address space update transaction */
    trace_hvf_memory_transaction(hvf_memory_transaction,
                                 listener->address_space &&
                                 listener->address_space->name ?
                                 listener->address_space->name : "(none)",
                                 "commit");
    hvf_memory_transaction = 0;
}

#define HVF_MEMORY_LISTENER_FIELDS(_name) \
    .name = (_name), \
    .priority = MEMORY_LISTENER_PRIORITY_ACCEL, \
    .region_add = hvf_region_add, \
    .region_del = hvf_region_del, \
    .log_start = hvf_log_start, \
    .log_stop = hvf_log_stop, \
    .log_clear = hvf_log_clear, \
    .begin = hvf_begin, \
    .commit = hvf_commit

static MemoryListener hvf_memory_listener = {
    HVF_MEMORY_LISTENER_FIELDS("hvf"),
};

static GHashTable *hvf_cpu_listeners;

void hvf_cpu_address_space_register(AddressSpace *as, int asidx)
{
    MemoryListener *listener;

    if (!hvf_enabled() || asidx != 0) {
        return;
    }

    if (!hvf_cpu_listeners) {
        hvf_cpu_listeners = g_hash_table_new(g_direct_hash, g_direct_equal);
    }
    if (g_hash_table_contains(hvf_cpu_listeners, as)) {
        return;
    }

    listener = g_new(MemoryListener, 1);
    *listener = (MemoryListener) {
        HVF_MEMORY_LISTENER_FIELDS(as->name),
    };
    memory_listener_register(listener, as);
    g_hash_table_insert(hvf_cpu_listeners, as, listener);
}

void hvf_cpu_address_space_unregister(AddressSpace *as)
{
    MemoryListener *listener;

    if (!hvf_cpu_listeners) {
        return;
    }
    listener = g_hash_table_lookup(hvf_cpu_listeners, as);
    if (!listener) {
        return;
    }
    g_hash_table_steal(hvf_cpu_listeners, as);
    memory_listener_unregister(listener);
    g_free(listener);
}

static int hvf_accel_init(AccelState *as, MachineState *ms)
{
    hv_return_t ret;
    HVFState *s = HVF_STATE(as);
    int pa_range = 36;
    MachineClass *mc = MACHINE_GET_CLASS(ms);


    if (mc->get_physical_address_range) {
        pa_range = mc->get_physical_address_range(ms,
            hvf_arch_get_default_ipa_bit_size(), hvf_arch_get_max_ipa_bit_size());
        if (pa_range < 0) {
            return -EINVAL;
        }
    }

    if (mc->get_kernel_irqchip_default) {
        bool kernel_irqchip_default = mc->get_kernel_irqchip_default(ms);
        if (!hvf_kernel_irqchip_override) {
            hvf_kernel_irqchip = kernel_irqchip_default;
        }
    }

    ret = hvf_arch_vm_create(ms, (uint32_t)pa_range);
    if (ret == HV_DENIED) {
        error_report("Could not access HVF. Is the executable signed"
                     " with com.apple.security.hypervisor entitlement?");
        exit(1);
    }
    assert_hvf_ok(ret);

    as->gdbstub.sstep_flags = SSTEP_ENABLE | SSTEP_NOIRQ;

    QTAILQ_INIT(&s->hvf_sw_breakpoints);

    hvf_state = s;
    hvf_test_init();
    hvf_test_register_qmp_commands();
    memory_listener_register(&hvf_memory_listener, &address_space_memory);

    return hvf_arch_init();
}

static void hvf_set_kernel_irqchip(Object *obj, Visitor *v,
                                   const char *name, void *opaque,
                                   Error **errp)
{
    OnOffSplit mode;

    hvf_kernel_irqchip_override = true;
    if (!visit_type_OnOffSplit(v, name, &mode, errp)) {
        return;
    }

    switch (mode) {
    case ON_OFF_SPLIT_ON:
#ifdef HOST_X86_64
        /* macOS 12 onwards exposes an HVF virtual APIC. */
        error_setg(errp, "HVF: kernel irqchip is not currently implemented for x86.");
        break;
#else
        hvf_kernel_irqchip = true;
        break;
#endif

    case ON_OFF_SPLIT_OFF:
        hvf_kernel_irqchip = false;
        break;

    case ON_OFF_SPLIT_SPLIT:
        error_setg(errp, "HVF: split irqchip is not supported on HVF.");
        break;

    default:
        /*
         * The value was checked in visit_type_OnOffSplit() above. If
         * we get here, then something is wrong in QEMU.
         */
        abort();
    }
}

static void hvf_accel_class_init(ObjectClass *oc, const void *data)
{
    AccelClass *ac = ACCEL_CLASS(oc);
    ac->name = "HVF";
    ac->init_machine = hvf_accel_init;
    ac->allowed = &hvf_allowed;
    hvf_kernel_irqchip_override = false;
    hvf_kernel_irqchip = false;
    object_class_property_add(oc, "kernel-irqchip", "on|off|split",
        NULL, hvf_set_kernel_irqchip,
        NULL, NULL);
    object_class_property_set_description(oc, "kernel-irqchip",
        "Configure HVF irqchip");
}

static const TypeInfo hvf_accel_type = {
    .name = TYPE_HVF_ACCEL,
    .parent = TYPE_ACCEL,
    .instance_size = sizeof(HVFState),
    .class_init = hvf_accel_class_init,
};

static void hvf_type_init(void)
{
    type_register_static(&hvf_accel_type);
}

type_init(hvf_type_init);
