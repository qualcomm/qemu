/*
 * QEMU Plugin API - System specific implementations
 *
 * This provides the APIs that have a specific system implementation
 * or are only relevant to system-mode.
 *
 * Copyright (C) 2017, Emilio G. Cota <cota@braap.org>
 * Copyright (C) 2019-2025, Linaro
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "qemu/main-loop.h"
#include "qapi/error.h"
#include "migration/blocker.h"
#include "hw/core/boards.h"
#include "qemu/plugin-memory.h"
#include "qemu/plugin.h"
#include "system/cpus.h"

/*
 * In system mode we cannot trace the binary being executed so the
 * helpers all return NULL/0.
 */
const char *qemu_plugin_path_to_binary(void)
{
    return NULL;
}

uint64_t qemu_plugin_start_code(void)
{
    return 0;
}

uint64_t qemu_plugin_end_code(void)
{
    return 0;
}

uint64_t qemu_plugin_entry_code(void)
{
    return 0;
}

/*
 * Virtual Memory queries
 */

static __thread struct qemu_plugin_hwaddr hwaddr_info;

struct qemu_plugin_hwaddr *qemu_plugin_get_hwaddr(qemu_plugin_meminfo_t info,
                                                  uint64_t vaddr)
{
    CPUState *cpu = current_cpu;
    unsigned int mmu_idx = get_mmuidx(info);
    enum qemu_plugin_mem_rw rw = get_plugin_meminfo_rw(info);
    hwaddr_info.is_store = (rw & QEMU_PLUGIN_MEM_W) != 0;

    assert(mmu_idx < NB_MMU_MODES);

    if (!tlb_plugin_lookup(cpu, vaddr, mmu_idx,
                           hwaddr_info.is_store, &hwaddr_info)) {
        error_report("invalid use of qemu_plugin_get_hwaddr");
        return NULL;
    }

    return &hwaddr_info;
}

bool qemu_plugin_hwaddr_is_io(const struct qemu_plugin_hwaddr *haddr)
{
    return haddr->is_io;
}

uint64_t qemu_plugin_hwaddr_phys_addr(const struct qemu_plugin_hwaddr *haddr)
{
    if (haddr) {
        return haddr->phys_addr;
    }
    return 0;
}

const char *qemu_plugin_hwaddr_device_name(const struct qemu_plugin_hwaddr *h)
{
    if (h && h->is_io) {
        MemoryRegion *mr = h->mr;
        if (!mr->name) {
            unsigned maddr = (uintptr_t)mr;
            g_autofree char *temp = g_strdup_printf("anon%08x", maddr);
            return g_intern_string(temp);
        } else {
            return g_intern_string(mr->name);
        }
    } else {
        return g_intern_static_string("RAM");
    }
}

/*
 * Time control
 */
static bool has_control;
static Error *migration_blocker;

const void *qemu_plugin_request_time_control(void)
{
    if (!has_control) {
        has_control = true;
        error_setg(&migration_blocker,
                   "TCG plugin time control does not support migration");
        migrate_add_blocker(&migration_blocker, &error_fatal);
        return &has_control;
    }
    return NULL;
}

static void advance_virtual_time__async(CPUState *cpu, run_on_cpu_data data)
{
    int64_t new_time = data.host_ulong;
    qemu_clock_advance_virtual_time(new_time);
}

void qemu_plugin_update_ns(const void *handle, int64_t new_time)
{
    if (handle == &has_control) {
        /* Need to execute out of cpu_exec, so bql can be locked. */
        async_run_on_cpu(current_cpu,
                         advance_virtual_time__async,
                         RUN_ON_CPU_HOST_ULONG(new_time));
    }
}

/*
 * vCPU control
 */
static void vcpu_yield__async(CPUState *cpu, run_on_cpu_data data)
{
    if (!qatomic_read(&cpu->plugin_state->pause_requested)) {
        /* we were resumed before we had a chance to pause */
        return;
    }
    qatomic_set(&cpu->plugin_state->pause_requested, false);

    /*
     * Remember we are the one pausing this cpu, so we never resume a cpu
     * paused by qemu itself. We hold the bql for both, so the pause and the
     * flag can't be observed out of sync.
     */
    cpu_pause(cpu);
    cpu->plugin_state->paused = true;

    /*
     * Tell the plugin the cpu is now paused. Anything resuming it can only run
     * once we release the bql, so this can't be observed too early.
     */
    if (cpu->plugin_state->pause_cb) {
        cpu->plugin_state->pause_cb(cpu->cpu_index,
                                    cpu->plugin_state->pause_cb_udata);
    }
}

void qemu_plugin_vcpu_yield(qemu_plugin_vcpu_udata_cb_t cb, void *userdata)
{
    current_cpu->plugin_state->pause_cb = cb;
    current_cpu->plugin_state->pause_cb_udata = userdata;
    qatomic_set(&current_cpu->plugin_state->pause_requested, true);
    /*
     * Need to execute out of cpu_exec: cpu_pause() publishes cpu->stopped,
     * which must not become visible while we still execute guest code, else
     * the vm would consider us paused while we finish the current block.
     */
    async_run_on_cpu(current_cpu, vcpu_yield__async, RUN_ON_CPU_NULL);
}

void qemu_plugin_vcpu_resume(unsigned int vcpu_index)
{
    CPUState *cpu;

    /*
     * An exclusive section is always entered without the bql (see
     * process_queued_cpu_work()), so taking it here would invert the lock
     * order. Note that instrumentation of an instruction qemu emulates
     * serially, such as an atomic operation it can't inline, does run in an
     * exclusive context (see cpu_exec_step_atomic()).
     */
    g_assert(!current_cpu || !cpu_in_exclusive_context(current_cpu));

    /*
     * We can be called from any thread and both walking the cpu list and
     * resuming a cpu need the bql. The guard is a NOP if we already hold it.
     */
    BQL_LOCK_GUARD();

    cpu = qemu_get_cpu(vcpu_index);
    g_assert(cpu);

    /*
     * Cancel a pause the cpu didn't honour yet, else it would pause after we
     * resumed it, and the resume would be lost.
     */
    qatomic_set(&cpu->plugin_state->pause_requested, false);

    if (!cpu->plugin_state->paused) {
        /* not ours to resume: it either runs, or qemu stopped it */
        return;
    }
    if (cpu->stopped) {
        cpu_resume(cpu);
    }
    cpu->plugin_state->paused = false;
}
