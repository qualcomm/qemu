#include "qemu/osdep.h"

#include "hw/core/devlog.h"

#define MAX_FMT_SIZE 1024

struct devlog_state {
    devlog_id next_id;
    GHashTable* table;
    devlog_level current_lvl;
    char fmt[MAX_FMT_SIZE];
};

struct devlog_entry {
    bool enabled;
    const char* type;
    const char* prefix;
};

static struct devlog_state dstate;

static void devlog_vprintf_level(devlog_id id, devlog_level lvl, const char* fmt, va_list ap);

static void devlog_init(void)
{
    dstate.table = g_hash_table_new(
        g_int64_hash,
        NULL
    );

    dstate.current_lvl = DEVLOG_DEFAULT_LEVEL;
}

devlog_id devlog_register(const char* type)
{
    if (!dstate.table) {
        devlog_init();
    }
    
    struct devlog_entry *entry = g_new0(struct devlog_entry, 1);
    entry->enabled = false;
    entry->type = type; // types are static, no need to copy

    g_hash_table_insert(
        dstate.table,
        (gpointer) dstate.next_id++,
        (gpointer) entry
    );

    return dstate.next_id;
}

bool devlog_unregister(devlog_id id)
{
    struct devlog_entry *entry = (struct devlog_entry*) g_hash_table_lookup(
        dstate.table,
        (gpointer) id
    );

    g_free(entry);

    return entry != NULL;
}

#ifdef CONFIG_DEVLOG_DEBUG
void devlog_vprintf_trace(devlog_id id, const char* fmt, va_list ap)
{
    devlog_vprintf_level(id, DEVLOG_LEVEL_TRACE, fmt, ap);
}

void devlog_vprintf_debug(devlog_id id, const char* fmt, va_list ap)
{
    devlog_vprintf_level(id, DEVLOG_LEVEL_DEBUG, fmt, ap);
}

void devlog_vprintf_info(devlog_id id, const char* fmt, va_list ap)
{
    devlog_vprintf_level(id, DEVLOG_LEVEL_INFO, fmt, ap);
}
#endif

void devlog_vprintf_warn(devlog_id id, const char* fmt, va_list ap)
{
    devlog_vprintf_level(id, DEVLOG_LEVEL_WARN, fmt, ap);
}

void devlog_vprintf_error(devlog_id id, const char* fmt, va_list ap)
{
    devlog_vprintf_level(id, DEVLOG_LEVEL_ERROR, fmt, ap);
}

G_GNUC_PRINTF(3, 0) static void devlog_vprintf_level(devlog_id id, devlog_level lvl, const char* fmt, va_list ap)
{
    int ret;
    
    struct devlog_entry *entry = (struct devlog_entry*) g_hash_table_lookup(
        dstate.table,
        (gpointer) id
    );

    if (entry->enabled && lvl >= dstate.current_lvl) {
        ret = snprintf(&dstate.fmt[0], MAX_FMT_SIZE, "[%s] %s", entry->type, fmt);
        
        if (ret >= MAX_FMT_SIZE) {
            fprintf(stderr, "The maximum format size is too small, it must be increased.\n");
            exit(1);
        }

        vprintf(dstate.fmt, ap);
    }
}
