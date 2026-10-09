/*
 * Local-only HVF dirty-log race and unmapped-RAM diagnostics.
 *
 * This file is deliberately not a production interface.  It provides a
 * small QMP control plane and a condition-variable gate used to hold a vCPU
 * immediately after HVF reports the target write fault and before QEMU
 * reacquires the BQL.
 */

#include "qemu/osdep.h"
#include "qemu/main-loop.h"
#include "qemu/thread.h"
#include "monitor/monitor-internal.h"
#include "qapi/error.h"
#include "qapi/qmp-registry.h"
#include "qobject/qdict.h"
#include "qobject/qjson.h"
#include "qobject/qobject.h"
#include "system/memory.h"
#include "system/hvf.h"
#include "system/hvf_int.h"
#include "hw/core/cpu.h"
#include "trace.h"

typedef enum HVFTestState {
    HVF_TEST_IDLE,
    HVF_TEST_ARMED,
    HVF_TEST_EXIT_HELD,
    HVF_TEST_RELEASED,
    HVF_TEST_DONE,
} HVFTestState;

static struct {
    QemuMutex lock;
    QemuCond cond;
    bool initialized;
    bool dirty_started;
    bool unmap_issued;
    HVFTestState state;
    unsigned cpu;
    uint64_t pc;
    uint64_t va;
    uint64_t pa;
    uint64_t syndrome;
    uint32_t dfsc;
    void *as;
    char as_name[128];
    void *mr;
    bool mr_is_ram;
    bool mr_readonly;
    bool mr_rom_device;
    bool mapping_known;
    bool mapped;
    char mr_name[128];
    uint64_t xlat;
    uint8_t dirty_mask;
    uint64_t abort_count;
    int unmap_result;
    char action[32];
    uint64_t target_page;
    uint64_t target_size;
} hvf_test;

static const char *hvf_test_state_name(HVFTestState state)
{
    switch (state) {
    case HVF_TEST_IDLE:       return "idle";
    case HVF_TEST_ARMED:      return "armed";
    case HVF_TEST_EXIT_HELD:  return "exit-held";
    case HVF_TEST_RELEASED:   return "released";
    case HVF_TEST_DONE:       return "done";
    default:                  return "unknown";
    }
}

void hvf_test_init(void)
{
    if (hvf_test.initialized) {
        return;
    }
    qemu_mutex_init(&hvf_test.lock);
    qemu_cond_init(&hvf_test.cond);
    hvf_test.target_page = 0x0a00001000;
    hvf_test.target_size = qemu_real_host_page_size();
    hvf_test.initialized = true;
}

bool hvf_test_pa_matches(uint64_t pa)
{
    uint64_t target = hvf_test.target_page;

    return pa >= target && pa - target < hvf_test.target_size;
}

static void hvf_test_reset_locked(void)
{
    hvf_test.dirty_started = false;
    hvf_test.unmap_issued = false;
    hvf_test.cpu = UINT_MAX;
    hvf_test.pc = 0;
    hvf_test.va = 0;
    hvf_test.pa = 0;
    hvf_test.syndrome = 0;
    hvf_test.dfsc = 0;
    hvf_test.as = NULL;
    hvf_test.as_name[0] = '\0';
    hvf_test.mr = NULL;
    hvf_test.mr_is_ram = false;
    hvf_test.mr_readonly = false;
    hvf_test.mr_rom_device = false;
    hvf_test.mapping_known = false;
    hvf_test.mapped = false;
    hvf_test.mr_name[0] = '\0';
    hvf_test.xlat = 0;
    hvf_test.dirty_mask = 0;
    hvf_test.abort_count = 0;
    hvf_test.unmap_result = HV_SUCCESS;
    hvf_test.action[0] = '\0';
}

bool hvf_test_target_page_overlaps(hwaddr start, uint64_t size)
{
    uint64_t target = hvf_test.target_page;
    uint64_t target_end;
    uint64_t end;

    if (!size || !hvf_test.target_size ||
        target > UINT64_MAX - hvf_test.target_size ||
        start > UINT64_MAX - size) {
        return false;
    }
    target_end = target + hvf_test.target_size;
    end = start + size;
    return start < target_end && target < end;
}

void hvf_test_trace_set_phys_mem(MemoryListener *listener,
                                 MemoryRegionSection *section, bool add,
                                 uint64_t transaction, const char *action,
                                 int ret)
{
    MemoryRegion *mr = section->mr;
    const char *as_name = listener->address_space &&
        listener->address_space->name ? listener->address_space->name : "(none)";
    const char *mr_name = mr ? memory_region_name(mr) : "(none)";
    uint64_t paddr = section->offset_within_address_space;
    uint64_t size = int128_get64(section->size);
    uint64_t page_size = qemu_real_host_page_size();
    bool ram = mr && memory_region_is_ram(mr);
    bool readonly = mr && mr->readonly;
    bool rom_device = mr && mr->rom_device;
    bool writable = mr && !readonly && !rom_device;
    bool aligned = QEMU_IS_ALIGNED(paddr, page_size) &&
        QEMU_IS_ALIGNED(size, page_size);

    if (hvf_test_target_page_overlaps(paddr, size)) {
        trace_hvf_test_set_phys_mem(transaction, as_name, add, action,
                                    paddr, size, section->offset_within_region,
                                    mr, mr_name, ret);
        trace_hvf_test_set_phys_mem_state(transaction, page_size, ram, readonly,
                                          rom_device, writable, aligned);
    }
}

void hvf_test_trace_log(MemoryListener *listener, MemoryRegionSection *section,
                        uint64_t transaction, int old, int new,
                        const char *action)
{
    MemoryRegion *mr = section->mr;
    const char *as_name = listener->address_space &&
        listener->address_space->name ? listener->address_space->name : "(none)";
    const char *mr_name = mr ? memory_region_name(mr) : "(none)";
    uint64_t paddr = section->offset_within_address_space;
    uint64_t size = int128_get64(section->size);

    if (hvf_test_target_page_overlaps(paddr, size)) {
        trace_hvf_test_log(transaction, as_name, action, paddr, size,
                           section->offset_within_region, mr, mr_name, old,
                           new);
        trace_hvf_test_log_state(transaction,
                                  mr ? memory_region_get_dirty_log_mask(mr) : 0);
    }
}

void hvf_test_trace_map(hwaddr start, uint64_t size, const char *action,
                        const char *name)
{
    if (hvf_test_target_page_overlaps(start, size)) {
        trace_hvf_test_map(start, size, action, name ?: "(none)");
    }
}

void hvf_test_trace_protect(hwaddr start, uint64_t size,
                            hv_memory_flags_t flags, const char *action)
{
    if (hvf_test_target_page_overlaps(start, size)) {
        trace_hvf_test_protect(start, size, flags, action);
    }
}

bool hvf_test_record_abort(CPUState *cpu, uint64_t pc, uint64_t va,
                           uint64_t pa, uint64_t syndrome, uint32_t dfsc,
                           AddressSpace *as, MemoryRegion *mr,
                           bool mapping_known, bool mapped, hwaddr xlat,
                           uint8_t dirty_mask, const char *action)
{
    hvf_test_init();
    if (!hvf_test_pa_matches(pa)) {
        return false;
    }

    qemu_mutex_lock(&hvf_test.lock);
    hvf_test.cpu = cpu->cpu_index;
    hvf_test.pc = pc;
    hvf_test.va = va;
    hvf_test.pa = pa;
    hvf_test.syndrome = syndrome;
    hvf_test.dfsc = dfsc;
    hvf_test.as = as;
    g_strlcpy(hvf_test.as_name, as ? as->name : "(none)",
              sizeof(hvf_test.as_name));
    hvf_test.mr = mr;
    hvf_test.mr_is_ram = mr && memory_region_is_ram(mr);
    hvf_test.mr_readonly = mr && mr->readonly;
    hvf_test.mr_rom_device = mr && mr->rom_device;
    hvf_test.mapping_known = mapping_known;
    hvf_test.mapped = mapped;
    g_strlcpy(hvf_test.mr_name, mr ? memory_region_name(mr) : "(none)",
              sizeof(hvf_test.mr_name));
    hvf_test.xlat = xlat;
    hvf_test.dirty_mask = dirty_mask;
    hvf_test.abort_count++;
    g_strlcpy(hvf_test.action, action ?: "unknown", sizeof(hvf_test.action));
    qemu_mutex_unlock(&hvf_test.lock);
    return true;
}

bool hvf_test_gate(CPUState *cpu, bool matching, uint64_t pc, uint64_t pa)
{
    hvf_test_init();
    if (!matching) {
        return false;
    }

    qemu_mutex_lock(&hvf_test.lock);
    if (hvf_test.state != HVF_TEST_ARMED) {
        qemu_mutex_unlock(&hvf_test.lock);
        return false;
    }

    hvf_test.cpu = cpu->cpu_index;
    hvf_test.pc = pc;
    hvf_test.pa = pa;
    hvf_test.state = HVF_TEST_EXIT_HELD;
    trace_hvf_test_gate("exit-held", cpu->cpu_index, pc, pa);
    qemu_cond_broadcast(&hvf_test.cond);

    while (hvf_test.state == HVF_TEST_EXIT_HELD) {
        qemu_cond_wait(&hvf_test.cond, &hvf_test.lock);
    }
    trace_hvf_test_gate(hvf_test_state_name(hvf_test.state),
                        cpu->cpu_index, pc, pa);
    qemu_mutex_unlock(&hvf_test.lock);
    return true;
}

void hvf_test_mark_done(void)
{
    hvf_test_init();
    qemu_mutex_lock(&hvf_test.lock);
    if (hvf_test.state == HVF_TEST_RELEASED) {
        hvf_test.state = HVF_TEST_DONE;
        qemu_cond_broadcast(&hvf_test.cond);
    }
    qemu_mutex_unlock(&hvf_test.lock);
}

static void hvf_test_bql_lock(bool *was_locked)
{
    *was_locked = bql_locked();
    if (!*was_locked) {
        bql_lock();
    }
}

static void hvf_test_bql_unlock(bool was_locked)
{
    if (!was_locked) {
        bql_unlock();
    }
}

static void qmp_hvf_test_arm_race(QDict *args, QObject **ret, Error **errp)
{
    Error *local_err = NULL;
    bool was_locked;

    hvf_test_init();
    hvf_test_bql_lock(&was_locked);
    qemu_mutex_lock(&hvf_test.lock);
    if (hvf_test.state != HVF_TEST_IDLE && hvf_test.state != HVF_TEST_DONE) {
        qemu_mutex_unlock(&hvf_test.lock);
        hvf_test_bql_unlock(was_locked);
        error_setg(errp, "HVF test is already %s",
                   hvf_test_state_name(hvf_test.state));
        return;
    }
    hvf_test_reset_locked();
    qemu_mutex_unlock(&hvf_test.lock);

    if (!memory_global_dirty_log_start(GLOBAL_DIRTY_MIGRATION, &local_err)) {
        hvf_test_bql_unlock(was_locked);
        error_propagate(errp, local_err);
        return;
    }

    qemu_mutex_lock(&hvf_test.lock);
    hvf_test.dirty_started = true;
    hvf_test.state = HVF_TEST_ARMED;
    hvf_test.action[0] = '\0';
    trace_hvf_test_dirty_log("start");
    qemu_cond_broadcast(&hvf_test.cond);
    qemu_mutex_unlock(&hvf_test.lock);
    hvf_test_bql_unlock(was_locked);

    *ret = QOBJECT(qdict_from_jsonf_nofail("{ 'state': 'armed' }"));
}

static void qmp_hvf_test_arm_unmap(QDict *args, QObject **ret,
                                   Error **errp)
{
    bool was_locked;
    uint64_t page_size = qemu_real_host_page_size();
    uint64_t window = HVF_TEST_TARGET_PAGE;
    bool do_unmap = true;
    hv_return_t result = HV_SUCCESS;
    QDict *rsp;

    hvf_test_init();
    if (args) {
        window = qdict_get_try_int(args, "page", HVF_TEST_TARGET_PAGE);
        do_unmap = qdict_get_try_bool(args, "unmap", true);
    }
    hvf_test_bql_lock(&was_locked);
    qemu_mutex_lock(&hvf_test.lock);
    if (hvf_test.state != HVF_TEST_IDLE && hvf_test.state != HVF_TEST_DONE) {
        qemu_mutex_unlock(&hvf_test.lock);
        hvf_test_bql_unlock(was_locked);
        error_setg(errp, "HVF test is already %s",
                   hvf_test_state_name(hvf_test.state));
        return;
    }
    if (hvf_test.unmap_issued) {
        qemu_mutex_unlock(&hvf_test.lock);
        hvf_test_bql_unlock(was_locked);
        error_setg(errp, "HVF test target page is already unmapped");
        return;
    }
    hvf_test_reset_locked();
    hvf_test.target_page = window;
    hvf_test.target_size = page_size;
    hvf_test.unmap_issued = true;
    qemu_mutex_unlock(&hvf_test.lock);

    if (do_unmap) {
        trace_hvf_vm_unmap(window, page_size);
        result = hv_vm_unmap(window, page_size);
        trace_hvf_test_unmap(window, page_size, result);
    }

    qemu_mutex_lock(&hvf_test.lock);
    hvf_test.unmap_result = result;
    qemu_mutex_unlock(&hvf_test.lock);
    hvf_test_bql_unlock(was_locked);

    rsp = qdict_new();
    qdict_put_int(rsp, "page", window);
    qdict_put_int(rsp, "size", page_size);
    qdict_put_int(rsp, "result", result);
    qdict_put_str(rsp, "result-name", hvf_return_string(result));
    *ret = QOBJECT(rsp);
}

static void qmp_hvf_test_stop_and_release(QDict *args, QObject **ret,
                                          Error **errp)
{
    bool was_locked = bql_locked();
    bool timed_out = false;
    gint64 deadline;

    hvf_test_init();
    if (was_locked) {
        bql_unlock();
    }

    deadline = g_get_monotonic_time() + 30 * G_USEC_PER_SEC;
    qemu_mutex_lock(&hvf_test.lock);
    while (hvf_test.state == HVF_TEST_ARMED) {
        gint64 left = deadline - g_get_monotonic_time();
        if (left <= 0) {
            timed_out = true;
            break;
        }
        qemu_cond_timedwait(&hvf_test.cond, &hvf_test.lock,
                            MIN((gint64)100, (left + 999) / 1000));
    }
    if (hvf_test.state != HVF_TEST_EXIT_HELD) {
        qemu_mutex_unlock(&hvf_test.lock);
        if (was_locked) {
            bql_lock();
        }
        if (timed_out) {
            error_setg(errp, "timed out waiting for target HVF exit");
        } else {
            error_setg(errp, "HVF test is %s",
                       hvf_test_state_name(hvf_test.state));
        }
        return;
    }
    qemu_mutex_unlock(&hvf_test.lock);

    bql_lock();
    trace_hvf_test_dirty_log("stop");
    memory_global_dirty_log_stop(GLOBAL_DIRTY_MIGRATION);

    qemu_mutex_lock(&hvf_test.lock);
    hvf_test.state = HVF_TEST_RELEASED;
    qemu_cond_broadcast(&hvf_test.cond);
    trace_hvf_test_gate("released", hvf_test.cpu, hvf_test.pc, hvf_test.pa);
    qemu_mutex_unlock(&hvf_test.lock);
    if (!was_locked) {
        bql_unlock();
    }

    *ret = QOBJECT(qdict_from_jsonf_nofail("{ 'state': 'released' }"));
}

static void qmp_hvf_test_set_target_page(QDict *args, QObject **ret,
                                         Error **errp)
{
    int64_t page_arg = qdict_get_try_int(args, "page", HVF_TEST_TARGET_PAGE);
    int64_t size_arg = qdict_get_try_int(args, "size",
                                         qemu_real_host_page_size());
    bool was_locked;
    QDict *rsp;

    if (page_arg < 0 || size_arg <= 0 ||
        (uint64_t)page_arg > UINT64_MAX - (uint64_t)size_arg) {
        error_setg(errp, "invalid target page or size");
        return;
    }

    hvf_test_init();
    hvf_test_bql_lock(&was_locked);
    qemu_mutex_lock(&hvf_test.lock);
    if (hvf_test.state != HVF_TEST_IDLE && hvf_test.state != HVF_TEST_DONE) {
        qemu_mutex_unlock(&hvf_test.lock);
        hvf_test_bql_unlock(was_locked);
        error_setg(errp, "HVF test is already %s",
                   hvf_test_state_name(hvf_test.state));
        return;
    }
    hvf_test_reset_locked();
    hvf_test.target_page = page_arg;
    hvf_test.target_size = size_arg;
    hvf_test.state = HVF_TEST_IDLE;
    qemu_mutex_unlock(&hvf_test.lock);
    hvf_test_bql_unlock(was_locked);

    rsp = qdict_new();
    qdict_put_int(rsp, "page", page_arg);
    qdict_put_int(rsp, "size", size_arg);
    *ret = QOBJECT(rsp);
}

static void qmp_hvf_test_status(QDict *args, QObject **ret, Error **errp)
{
    QDict *rsp = qdict_new();

    hvf_test_init();
    qemu_mutex_lock(&hvf_test.lock);
    qdict_put_str(rsp, "state", hvf_test_state_name(hvf_test.state));
    qdict_put_bool(rsp, "dirty-started", hvf_test.dirty_started);
    qdict_put_bool(rsp, "unmap-issued", hvf_test.unmap_issued);
    qdict_put_int(rsp, "cpu", hvf_test.cpu);
    qdict_put_int(rsp, "pc", hvf_test.pc);
    qdict_put_int(rsp, "va", hvf_test.va);
    qdict_put_int(rsp, "pa", hvf_test.pa);
    qdict_put_int(rsp, "syndrome", hvf_test.syndrome);
    qdict_put_int(rsp, "dfsc", hvf_test.dfsc);
    qdict_put_int(rsp, "as", (int64_t)(uintptr_t)hvf_test.as);
    qdict_put_str(rsp, "as-name", hvf_test.as_name);
    qdict_put_int(rsp, "mr", (int64_t)(uintptr_t)hvf_test.mr);
    qdict_put_bool(rsp, "mr-is-ram", hvf_test.mr_is_ram);
    qdict_put_bool(rsp, "mr-readonly", hvf_test.mr_readonly);
    qdict_put_bool(rsp, "mr-rom-device", hvf_test.mr_rom_device);
    qdict_put_bool(rsp, "mapping-known", hvf_test.mapping_known);
    qdict_put_bool(rsp, "mapped", hvf_test.mapped);
    qdict_put_str(rsp, "mr-name", hvf_test.mr_name);
    qdict_put_int(rsp, "xlat", hvf_test.xlat);
    qdict_put_int(rsp, "dirty-mask", hvf_test.dirty_mask);
    qdict_put_int(rsp, "abort-count", hvf_test.abort_count);
    qdict_put_int(rsp, "target-page", hvf_test.target_page);
    qdict_put_int(rsp, "target-size", hvf_test.target_size);
    qdict_put_int(rsp, "unmap-result", hvf_test.unmap_result);
    qdict_put_str(rsp, "unmap-result-name",
                  hvf_return_string(hvf_test.unmap_result));
    qdict_put_str(rsp, "action", hvf_test.action);
    qemu_mutex_unlock(&hvf_test.lock);
    *ret = QOBJECT(rsp);
}

void hvf_test_register_qmp_commands(void)
{
    static bool registered;

    if (registered) {
        return;
    }
    qmp_register_command(&qmp_commands, "x-hvf-test-arm-race",
                         qmp_hvf_test_arm_race, QCO_COROUTINE, 0);
    qmp_register_command(&qmp_commands, "x-hvf-test-arm-unmap",
                         qmp_hvf_test_arm_unmap, QCO_COROUTINE, 0);
    qmp_register_command(&qmp_commands, "x-hvf-test-stop-and-release",
                         qmp_hvf_test_stop_and_release, QCO_COROUTINE, 0);
    qmp_register_command(&qmp_commands, "x-hvf-test-set-target-page",
                         qmp_hvf_test_set_target_page, 0, 0);
    qmp_register_command(&qmp_commands, "x-hvf-test-status",
                         qmp_hvf_test_status, 0, 0);
    registered = true;
}
