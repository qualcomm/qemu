#include <inttypes.h>
#include <assert.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <glib.h>
#include <time.h>

#include <qemu-plugin.h>
#include <math.h>

#define MAX_CPUS 32
#define TIME_ARRAY_ELT_COUNT 1024
#define ACTIVITY_ARRAY_ELT_COUNT (1024*16)

QEMU_PLUGIN_EXPORT int qemu_plugin_version = QEMU_PLUGIN_VERSION;
static GArray *cpu_idle_times[MAX_CPUS];
static GArray *cpu_resume_times[MAX_CPUS];
static GArray *cpu_activity[MAX_CPUS];
static GMutex lock;
static GHashTable *times_by_dev;

typedef double timestamp_sec;
static timestamp_sec prev_tb_start[MAX_CPUS];
static double to_usec(timestamp_sec t) { return t * 1e6; }
static double to_msec(timestamp_sec t) { return t * 1e3; }

typedef struct {
    const char *name;
    timestamp_sec ts;
    unsigned int cpu;
} DeviceTimes;

typedef enum {
    HEXAGON_SCALAR,
    HEXAGON_BUS,
    HEXAGON_VEC,
    HEXAGON_MATRIX,
    HEXAGON_SUPER,
} InstType;

static const char *get_tb_type_name(InstType inst_type) {
    switch (inst_type) {
    case HEXAGON_SCALAR: return "scalar";
    case HEXAGON_BUS: return "bus";
    case HEXAGON_VEC: return "HVX";
    case HEXAGON_MATRIX: return "COPROC";
    case HEXAGON_SUPER: return "superuser";
    }
    g_assert_not_reached();
}

static InstType prev_tb_type[MAX_CPUS];

typedef struct {
   InstType tb_type;
   timestamp_sec time_start;
   timestamp_sec dur_usec;
} ProcActivity;

static timestamp_sec tstamp() {
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + (ts.tv_nsec * 1e-9);
}

static void plugin_init(void)
{
    times_by_dev = g_hash_table_new(NULL, NULL);
}

static void new_io_time(GArray *times, const char *name, unsigned int cpu)
{
    DeviceTimes io_time;
    io_time.name = name;
    io_time.ts = tstamp();
    io_time.cpu = cpu;

    g_array_append_val(times, io_time);
}
static void vcpu_haddr(unsigned int cpu_index, qemu_plugin_meminfo_t meminfo,
                       uint64_t vaddr, void *udata)
{
    struct qemu_plugin_hwaddr *hwaddr = qemu_plugin_get_hwaddr(meminfo, vaddr);

    if (!hwaddr || !qemu_plugin_hwaddr_is_io(hwaddr)) {
        return;
    } else {
        const char *name = qemu_plugin_hwaddr_device_name(hwaddr);
        GArray *times;

        g_mutex_lock(&lock);
        times = (GArray *) g_hash_table_lookup(times_by_dev, name);
        if (!times) {
            times = g_array_sized_new(FALSE, TRUE,
                sizeof(DeviceTimes), TIME_ARRAY_ELT_COUNT);
            g_hash_table_insert(times_by_dev, (gpointer) name, times);
        }
        g_mutex_unlock(&lock);

        new_io_time(times, name, cpu_index);
    }
}

static void vcpu_tb_executed(unsigned int cpu_index, void *udata)
{
    InstType tb_type = *(InstType *)udata;
    if (prev_tb_type[cpu_index] != tb_type && !isnan(prev_tb_start[cpu_index])) {
        timestamp_sec t0 = prev_tb_start[cpu_index];
        ProcActivity entry;
        entry.tb_type = tb_type;
        entry.time_start = t0;
        entry.dur_usec = (tstamp() - t0) * 1e6;

        g_array_append_val(cpu_activity[cpu_index], entry);
        prev_tb_start[cpu_index] = tstamp();
    }
    else if (isnan(prev_tb_start[cpu_index])) {
        prev_tb_start[cpu_index] = tstamp();
    }
    prev_tb_type[cpu_index] = tb_type;
}

static void activity_end(unsigned int cpu_index) {
    if (isnan(prev_tb_start[cpu_index])) {
        return;
    }

    const timestamp_sec t_end = tstamp();

    timestamp_sec t0 = prev_tb_start[cpu_index];
    ProcActivity entry;
    entry.tb_type = prev_tb_type[cpu_index];
    entry.time_start = t0;
    entry.dur_usec = (t_end - t0) * 1e6;

    g_array_append_val(cpu_activity[cpu_index], entry);
    prev_tb_start[cpu_index] = NAN;
    prev_tb_type[cpu_index] = HEXAGON_SCALAR;
}

static void activity_exit() {
    int cpu_index;
    for (cpu_index = 0; cpu_index < MAX_CPUS; cpu_index++) {
        activity_end(cpu_index);
    }
}

static const uint32_t HEXAGON_INST_PARSE_MASK       = 0x0000c000;
#define ITYPE_ST_MASK 0xf9800000
#define ITYPE_MASK 0xf8000000
#define ITYPE_HVX  0x18000000
#define ITYPE_HVX_VMEM 0x28000000
#define ITYPE_ST_BUS   0xa8000000

static InstType get_inst_type(struct qemu_plugin_insn *inst)
{
    uint32_t opcode = *((uint32_t *)qemu_plugin_insn_data(inst));
    if ((opcode & ITYPE_ST_MASK) == ITYPE_ST_BUS)
        return HEXAGON_BUS;

    uint32_t itype = ITYPE_MASK & opcode;

    /* TODO: pluck out COPROC from ST/LD_MISC, SUPER from various */

    switch (itype) {
        case ITYPE_HVX:
        case ITYPE_HVX_VMEM:
            return HEXAGON_VEC;
        default:
            return HEXAGON_SCALAR;
    }
    return HEXAGON_SCALAR;
}

static void plugin_exit(qemu_plugin_id_t id, void *p)
{
    int i, cpu_index;
    bool is_first_entry = true;
    qemu_plugin_outs("{");
    qemu_plugin_outs("   \"traceEvents\": [\n");
    g_autoptr(GString) entry = g_string_new("");

    activity_exit();

    GHashTableIter iter;
    gpointer dev_name, time_stamps;

    g_hash_table_iter_init(&iter, times_by_dev);
    while (g_hash_table_iter_next (&iter, &dev_name, &time_stamps))
    {
        GArray *times = (GArray *)time_stamps;
        unsigned times_len = times->len;
        int i;
        for (i = 0; i < times_len; i++) {
            DeviceTimes *io_time = &g_array_index(times, DeviceTimes, i);

            g_string_printf(entry, "{\"name\": \"%s\", \"cat\": \"io\", \"ph\": \"i\","
                                   "\"ts\": %f, \"pid\": 0, \"tid\": %d, \"s\": \"t\" },\n",
                                   io_time->name, io_time->ts, io_time->cpu);
            qemu_plugin_outs(entry->str);
        }
    }

    for (cpu_index = 0; cpu_index < MAX_CPUS; cpu_index++) {
        for (i = 0; i < TIME_ARRAY_ELT_COUNT; i++) {

            timestamp_sec *t = &g_array_index(cpu_idle_times[cpu_index], timestamp_sec, i);
            /* {"name": "myFunction", "cat": "foo", "ph": "B", "ts": 123, "pid": 2343, "tid": 2347,
             */
            if (!isnan(*t)) {
              g_string_printf(entry, "{\"name\": \"active\", \"ph\": \"E\", \"ts\": %f, \"pid\": 0, \"tid\": %d },\n", to_usec(*t), cpu_index);
              qemu_plugin_outs(entry->str);
              is_first_entry = false;
            }
            t = &g_array_index(cpu_resume_times[cpu_index], timestamp_sec, i);
            if (!isnan(*t)) {
              g_string_printf(entry, "{\"name\": \"active\", \"ph\": \"B\", \"ts\": %f, \"pid\": 0, \"tid\": %d },\n", to_usec(*t), cpu_index);
              qemu_plugin_outs(entry->str);
            }
        }

        ProcActivity *activity;
        for (i = 0; i < cpu_activity[cpu_index]->len; i++) {
            activity = &g_array_index(cpu_activity[cpu_index], ProcActivity, i);
            g_string_printf(entry, "{\"name\": \"%s\", \"ph\": \"X\", \"ts\": %f, \"dur\": %f,"
                    "\"pid\": 1, \"tid\": %d },\n",
                    get_tb_type_name(activity->tb_type),
                    to_usec(activity->time_start),
                    activity->dur_usec,
                    cpu_index);
            qemu_plugin_outs(entry->str);
        }
    }
    qemu_plugin_outs("{\"name\": \"thread_name\", \"ph\": \"M\", \"pid\": 0, \"tid\": 0, \"args\": {\"name\": \"HW Thread\"}},\n");
    qemu_plugin_outs("{\"name\": \"thread_name\", \"ph\": \"M\", \"pid\": 0, \"tid\": 1, \"args\": {\"name\": \"HW Thread\"}},\n");
    qemu_plugin_outs("{\"name\": \"thread_name\", \"ph\": \"M\", \"pid\": 0, \"tid\": 2, \"args\": {\"name\": \"HW Thread\"}},\n");
    qemu_plugin_outs("{\"name\": \"thread_name\", \"ph\": \"M\", \"pid\": 0, \"tid\": 3, \"args\": {\"name\": \"HW Thread\"}},\n");

    qemu_plugin_outs("{\"name\": \"process_name\", \"ph\": \"M\", \"args\": {\"name\": \"DSP Core\"}}\n");
    qemu_plugin_outs("]\n");
    qemu_plugin_outs("}\n");
}
static void vcpu_idle(qemu_plugin_id_t id, unsigned int cpu_index) {
    timestamp_sec now = tstamp();
    g_array_append_val(cpu_idle_times[cpu_index], now);
    activity_end(cpu_index);
}

static void vcpu_resume(qemu_plugin_id_t id, unsigned int cpu_index) {
    timestamp_sec now = tstamp();
    g_array_append_val(cpu_resume_times[cpu_index], now);
}

static void vcpu_tb_trans(qemu_plugin_id_t id, struct qemu_plugin_tb *tb)
{
    size_t n = qemu_plugin_tb_n_insns(tb);
    size_t i;
    InstType *tb_type = g_new0(InstType, 1);
    *tb_type = HEXAGON_SCALAR;
    InstType inst_type;

    for (i = 0; i < n; i++) {
        struct qemu_plugin_insn *insn = qemu_plugin_tb_get_insn(tb, i);
        inst_type = get_inst_type(insn);

        /* The types values indicate their precedence for categorizing
         * the TB.  Except for scalar, most types will be mutually exclusive.
         */
        if (inst_type > *tb_type)
            *tb_type = inst_type;

        /* FIXME - shouldn't this only apply if the instruction could trigger
         * a load/store?
         */
        qemu_plugin_register_vcpu_mem_cb(insn, vcpu_haddr,
                                         QEMU_PLUGIN_CB_NO_REGS,
                                         QEMU_PLUGIN_MEM_RW, 0);
    }
    qemu_plugin_register_vcpu_tb_exec_cb(tb,
            vcpu_tb_executed,
            QEMU_PLUGIN_CB_NO_REGS,
            (gpointer) tb_type);
}

QEMU_PLUGIN_EXPORT
int qemu_plugin_install(qemu_plugin_id_t id, const qemu_info_t *info,
                        int argc, char **argv)
{
    int i,j;

    plugin_init();

    if (info->system.smp_vcpus > MAX_CPUS ||
        info->system.max_vcpus > MAX_CPUS) {
        fprintf(stderr, "actprofile: can only track up to %d CPUs\n", MAX_CPUS);
    }
    if (!info->system_emulation) {
        /* FIXME: there might end up being some use cases for linux-user
         * as the design of this plugin evolves.
         */
        fprintf(stderr, "actprofile: plugin only useful in sysemu mode\n");
    }
    for (i = 0; i < MAX_CPUS; i++) {
        cpu_idle_times[i] = g_array_sized_new(FALSE, TRUE, sizeof(timestamp_sec), TIME_ARRAY_ELT_COUNT);
        for (j = 0; j < TIME_ARRAY_ELT_COUNT; j++) {
            g_array_index(cpu_idle_times[i], timestamp_sec, j) = NAN;
        }
        cpu_resume_times[i] = g_array_sized_new(FALSE, TRUE, sizeof(timestamp_sec), TIME_ARRAY_ELT_COUNT);
        for (j = 0; j < TIME_ARRAY_ELT_COUNT; j++) {
            g_array_index(cpu_resume_times[i], timestamp_sec, j) = NAN;
        }

        cpu_activity[i] = g_array_sized_new(FALSE, TRUE, sizeof(ProcActivity),
            ACTIVITY_ARRAY_ELT_COUNT);
        prev_tb_start[i] = NAN;
        prev_tb_type[i] = HEXAGON_SCALAR;
    }
    qemu_plugin_register_vcpu_tb_trans_cb(id, vcpu_tb_trans);
    qemu_plugin_register_atexit_cb(id, plugin_exit, NULL);
    qemu_plugin_register_vcpu_idle_cb(id, vcpu_idle);
    qemu_plugin_register_vcpu_resume_cb(id, vcpu_resume);
    return 0;
}
