#include "qemu/osdep.h"

#ifdef CONFIG_DEVLOG
#include "hw/core/devlog.h"

// blue
#define COLOR_DEBUG "34"
// gray
#define COLOR_TRACE "90"
// green
#define COLOR_INFO "32"
// orange
#define COLOR_WARNING "33"
// red
#define COLOR_ERROR "31"
// default terminal color
#define COLOR_RESET "0"

#define DEVLOG_PRE_COLOR "\e["
#define DEVLOG_POST_COLOR "m"

#define DEVLOG_COLOR_END DEVLOG_PRE_COLOR COLOR_RESET DEVLOG_POST_COLOR

#define MAX_FMT_SIZE 1024

struct devlog_state {
    GHashTable* table; // id -> entry

    devlog_id next_id;
    devlog_level default_init_lvl;
    char fmt[MAX_FMT_SIZE];
};

struct devlog_entry {
    enum devlog_level level;
    const char* type;

    const char* prefixes[DEVLOG_LEVEL_MAX];
};

static struct devlog_state dstate;

static void devlog_vprintf_level(devlog_id id, devlog_level lvl, const char* fmt, va_list ap);

void devlog_init(enum devlog_level default_init_level)
{
    dstate.table = g_hash_table_new(
        g_direct_hash,
        g_direct_equal
    );

    dstate.default_init_lvl = default_init_level;
}

struct level_info {
    const char* name;
    const char* color;
};

const struct level_info levels_info[] = {
    [DEVLOG_LEVEL_DEBUG] = {
        .name = "debug",
        .color = COLOR_DEBUG,
    },

    [DEVLOG_LEVEL_TRACE] = {
        .name = "trace",
        .color = COLOR_TRACE,
    },

    [DEVLOG_LEVEL_INFO] = {
        .name = "info",
        .color = COLOR_INFO,
    },

    [DEVLOG_LEVEL_WARNING] = {
        .name = "warn",
        .color = COLOR_WARNING,
    },

    [DEVLOG_LEVEL_ERROR] = {
        .name = "error",
        .color = COLOR_ERROR,
    },
};

devlog_id devlog_register(const char* type)
{
    size_t i;
    struct devlog_entry *entry = g_new0(struct devlog_entry, 1);
    const char* prefix_fmt = DEVLOG_PRE_COLOR "%s" DEVLOG_POST_COLOR "[%s - %s]" DEVLOG_COLOR_END;
    const struct level_info* info;

    entry->level = dstate.default_init_lvl;
    entry->type = type; // types are static, no need to copy

    g_hash_table_insert(
        dstate.table,
        GINT_TO_POINTER(dstate.next_id++),
        entry
    );

    for (i = DEVLOG_LEVEL_START; i < DEVLOG_LEVEL_MAX; ++i) {
        info = &levels_info[i];

        size_t prefix_bytes = snprintf(NULL, 0, prefix_fmt, info->color, info->name, type) + 1;
        char *prefix = g_new(char, prefix_bytes);
        sprintf(prefix, prefix_fmt, info->color, type, info->name);
        entry->prefixes[i] = prefix;
    }

    return dstate.next_id;
}

bool devlog_unregister(devlog_id id)
{
    struct devlog_entry *entry = (struct devlog_entry*) g_hash_table_lookup(
        dstate.table,
        (gpointer) id
    );

    g_hash_table_remove(dstate.table, (gpointer) id);

    g_free(entry);

    return entry != NULL;
}

struct table_data {
    // input
    const char* type;
    enum devlog_level lvl;

    // output
    bool updated;
};

static void set_dev_level(gpointer id, gpointer entry_ptr, gpointer data_ptr)
{
    struct devlog_entry *entry = entry_ptr;
    struct table_data *data = data_ptr;

    if (!strcmp(entry->type, data->type)) {
        printf("Setting level of %s to %d\n", entry->type, data->lvl);
        entry->level = data->lvl;
        data->updated = true;
    }
}

bool devlog_set_level(const char* type, enum devlog_level level)
{
    struct table_data data = {
        .type = type,
        .lvl = level
    };

    g_hash_table_foreach(dstate.table, set_dev_level, &data);

    return data.updated;
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

    if (lvl >= entry->level) {
        ret = snprintf(&dstate.fmt[0], MAX_FMT_SIZE, "%s %s", entry->prefixes[lvl], fmt);
        
        if (ret >= MAX_FMT_SIZE) {
            fprintf(stderr, "The maximum format string size is too small, it must be increased.\n");
            exit(1);
        }

        vprintf(dstate.fmt, ap);
    }
}

#endif
