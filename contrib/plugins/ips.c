/*
 * Instructions Per Second (IPS) rate limiting plugin.
 *
 * This plugin can be used to restrict the execution of a system to a
 * particular number of Instructions Per Second (IPS). This controls
 * time as seen by the guest so while wall-clock time may be longer
 * from the guests point of view time will pass at the normal rate.
 *
 * This uses the new plugin API which allows the plugin to control
 * system time.
 *
 * Copyright (c) 2023 Linaro Ltd
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <stdio.h>
#include <glib.h>
#include <qemu-plugin.h>

QEMU_PLUGIN_EXPORT int qemu_plugin_version = QEMU_PLUGIN_VERSION;

/* how many times do we update time per sec */
#define NUM_TIME_UPDATE_PER_SEC 10
#define NSEC_IN_ONE_SEC (1000 * 1000 * 1000)

static GMutex global_time_lock;

static uint64_t max_insn_per_second = 1000 * 1000 * 1000; /* ips per core, per second */
static uint64_t max_insn_per_quantum; /* trap every N instructions */
static int64_t virtual_time_ns; /* last set virtual time */

static const void *time_handle;

static GThread *tickthread;
static gint tickthread_exit;

typedef struct {
    uint64_t total_insn;
    uint64_t quantum_insn; /* insn in last quantum */
    int64_t last_quantum_time; /* time when last quantum started */
    GMutex budget_lock;
    GCond budget_cond; /* signaled when budget_insn is replenished */
    uint64_t budget_insn; /* insn this vcpu is allowed to execute */
} vCPUTime;

struct qemu_plugin_scoreboard *vcpus;

/* return epoch time in ns */
static int64_t now_ns(void)
{
    return g_get_real_time() * 1000;
}

static int64_t time_for_insn(uint64_t num_insn)
{
    double num_secs = (double) num_insn / (double) max_insn_per_second;
    return num_secs * (double) NSEC_IN_ONE_SEC;
}

static void update_system_time(vCPUTime *vcpu)
{
    vcpu->total_insn += vcpu->quantum_insn;
    vcpu->last_quantum_time = now_ns();

    /* based on total number of instructions, what should be the new time? */
    int64_t new_virtual_time = time_for_insn(vcpu->total_insn);

    g_mutex_lock(&global_time_lock);

    /* Time only moves forward. Another vcpu might have updated it already. */
    if (new_virtual_time > virtual_time_ns) {
        if (time_handle) {
            qemu_plugin_update_ns(time_handle, new_virtual_time);
        }
        virtual_time_ns = new_virtual_time;
    }

    g_mutex_unlock(&global_time_lock);
}


static void update_budget(vCPUTime *vcpu)
{
    g_mutex_lock(&vcpu->budget_lock);

    if (vcpu->quantum_insn < vcpu->budget_insn) {
        vcpu->budget_insn -= vcpu->quantum_insn;
    } else {
        vcpu->budget_insn = 0;
    }

    while (!vcpu->budget_insn) {
        g_cond_wait(&vcpu->budget_cond, &vcpu->budget_lock);
    }

    g_mutex_unlock(&vcpu->budget_lock);
}

static void *tickthread_fn(void *userdata)
{
    int64_t quantum_us = time_for_insn(max_insn_per_quantum) / 1000;

    while (!g_atomic_int_get(&tickthread_exit)) {
        for (int i = 0; i < qemu_plugin_num_vcpus(); i++) {
            vCPUTime *vcpu = qemu_plugin_scoreboard_find(vcpus, i);
            g_mutex_lock(&vcpu->budget_lock);
            vcpu->budget_insn = max_insn_per_quantum;
            g_cond_signal(&vcpu->budget_cond);
            g_mutex_unlock(&vcpu->budget_lock);
        }
        g_usleep(quantum_us);
    }

    return NULL;
}

static void vcpu_init(unsigned int cpu_index, void *userdata)
{
    vCPUTime *vcpu = qemu_plugin_scoreboard_find(vcpus, cpu_index);
    vcpu->total_insn = 0;
    vcpu->quantum_insn = 0;
    vcpu->last_quantum_time = now_ns();
    g_mutex_init(&vcpu->budget_lock);
    g_cond_init(&vcpu->budget_cond);
    vcpu->budget_insn = 0;
}

static void every_quantum_insn(unsigned int cpu_index, void *udata)
{
    vCPUTime *vcpu = qemu_plugin_scoreboard_find(vcpus, cpu_index);
    update_system_time(vcpu);
    update_budget(vcpu);
    vcpu->quantum_insn = 0;
}

static void vcpu_tb_trans(struct qemu_plugin_tb *tb, void *userdata)
{
    size_t n_insns = qemu_plugin_tb_n_insns(tb);
    qemu_plugin_u64 quantum_insn =
        qemu_plugin_scoreboard_u64_in_struct(vcpus, vCPUTime, quantum_insn);
    /* count (and eventually trap) once per tb */
    qemu_plugin_register_vcpu_tb_exec_inline_per_vcpu(
        tb, QEMU_PLUGIN_INLINE_ADD_U64, quantum_insn, n_insns);
    qemu_plugin_register_vcpu_tb_exec_cond_cb(
        tb, every_quantum_insn,
        QEMU_PLUGIN_CB_NO_REGS, QEMU_PLUGIN_COND_GE,
        quantum_insn, max_insn_per_quantum, NULL);
}

static void plugin_exit(void *udata)
{
    g_atomic_int_set(&tickthread_exit, 1);
    g_thread_join(tickthread);
    qemu_plugin_scoreboard_free(vcpus);
}

typedef struct {
    const char *suffix;
    unsigned long multipler;
} ScaleEntry;

/* a bit like units.h but not binary */
static const ScaleEntry scales[] = {
    { "k", 1000 },
    { "m", 1000 * 1000 },
    { "g", 1000 * 1000 * 1000 },
};

QEMU_PLUGIN_EXPORT int qemu_plugin_install(qemu_plugin_id_t id,
                                           const qemu_info_t *info, int argc,
                                           char **argv)
{
    bool ipq_set = false;

    for (int i = 0; i < argc; i++) {
        char *opt = argv[i];
        g_auto(GStrv) tokens = g_strsplit(opt, "=", 2);
        if (g_strcmp0(tokens[0], "ips") == 0) {
            char *endptr = NULL;
            max_insn_per_second = g_ascii_strtoull(tokens[1], &endptr, 10);
            if (!max_insn_per_second && errno) {
                fprintf(stderr, "%s: couldn't parse %s (%s)\n",
                        __func__, tokens[1], g_strerror(errno));
                return -1;
            }

            if (endptr && *endptr != 0) {
                g_autofree gchar *lower = g_utf8_strdown(endptr, -1);
                unsigned long scale = 0;

                for (int j = 0; j < G_N_ELEMENTS(scales); j++) {
                    if (g_strcmp0(lower, scales[j].suffix) == 0) {
                        scale = scales[j].multipler;
                        break;
                    }
                }

                if (scale) {
                    max_insn_per_second *= scale;
                } else {
                    fprintf(stderr, "bad suffix: %s\n", endptr);
                    return -1;
                }
            }
        } else if (g_strcmp0(tokens[0], "ipq") == 0) {
            max_insn_per_quantum = g_ascii_strtoull(tokens[1], NULL, 10);

            if (!max_insn_per_quantum) {
                fprintf(stderr, "bad ipq value: %s\n", tokens[0]);
                return -1;
            }
            ipq_set = true;
        } else {
            fprintf(stderr, "option parsing failed: %s\n", opt);
            return -1;
        }
    }

    vcpus = qemu_plugin_scoreboard_new(sizeof(vCPUTime));

    if (!ipq_set) {
        max_insn_per_quantum = max_insn_per_second / NUM_TIME_UPDATE_PER_SEC;
    }

    if (max_insn_per_quantum == 0) {
        fprintf(stderr, "minimum of %d instructions per second needed\n",
                NUM_TIME_UPDATE_PER_SEC);
        return -1;
    }

    time_handle = qemu_plugin_request_time_control();
    g_assert(time_handle || !info->system_emulation);

    tickthread = g_thread_new("tickthread", tickthread_fn, NULL);

    qemu_plugin_register_vcpu_tb_trans_cb(id, vcpu_tb_trans, NULL);
    qemu_plugin_register_vcpu_init_cb(id, vcpu_init, NULL);
    qemu_plugin_register_vcpu_exit_cb(id, every_quantum_insn, NULL);
    qemu_plugin_register_atexit_cb(id, plugin_exit, NULL);

    return 0;
}
