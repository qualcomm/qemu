#ifdef CONFIG_DEVLOG

#include "qemu/osdep.h"

#include "hw/core/devlog.h"

#define MAX_FMT_SIZE 1024

struct devlog_state {
    // both tables point to the same pointer
    GHashTable* table; // id -> entry
    GHashTable* name_table; // name -> entry

    devlog_id next_id;
    devlog_level default_init_lvl;
    char fmt[MAX_FMT_SIZE];
};

struct devlog_entry {
    enum devlog_level level;
    const char* type;
    const char* prefix;
};

static struct devlog_state dstate;

static void devlog_vprintf_level(devlog_id id, devlog_level lvl, const char* fmt, va_list ap);

void devlog_init(enum devlog_level default_init_level)
{
    dstate.table = g_hash_table_new(
        g_int64_hash,
        NULL
    );

    dstate.name_table = g_hash_table_new(
        g_str_hash,
        g_str_equal
    );

    dstate.default_init_lvl = default_init_level;
}

devlog_id devlog_register(const char* type)
{
    struct devlog_entry *entry = g_new0(struct devlog_entry, 1);
    entry->level = DEVLOG_LEVEL_NONE;
    entry->type = type; // types are static, no need to copy

    g_hash_table_insert(
        dstate.table,
        (gpointer) dstate.next_id++,
        (gpointer) entry
    );

    g_hash_table_insert(
        dstate.name_table,
        (gpointer) type,
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

    g_hash_table_remove(dstate.table, (gpointer) id);
    g_hash_table_remove(dstate.name_table, (gpointer) entry->type);

    g_free(entry);

    return entry != NULL;
}

bool devlog_set_level(const char* type, enum devlog_level level)
{
    struct devlog_entry *entry = (struct devlog_entry*) g_hash_table_lookup(
        dstate.table,
        (gpointer) type
    );

    if (!entry) {
        return false;
    }

    entry->level = level;

    return true;
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
    devlog_vprintf_level(id, DEVLOG_LEVEL_WARNING, fmt, ap);
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

    if (lvl >= entry->level && lvl) {
        ret = snprintf(&dstate.fmt[0], MAX_FMT_SIZE, "[%s] %s", entry->type, fmt);
        
        if (ret >= MAX_FMT_SIZE) {
            fprintf(stderr, "The maximum format size is too small, it must be increased.\n");
            exit(1);
        }

        vprintf(dstate.fmt, ap);
    }
}
#endif
