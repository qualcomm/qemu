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
    char* type;

    char* prefixes[DEVLOG_LEVEL_END];
};

struct level_info {
    const char* name;
    const char* color;
};

// used to iterate over the hash table
struct table_data {
    // iter input
    const char* type;
    enum devlog_level lvl;

    // iter output
    bool updated;
};

static struct devlog_state dstate;

static void devlog_vprintf_level(devlog_id id, devlog_level lvl, const char* fmt, va_list ap);

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

void devlog_init(enum devlog_level default_init_level)
{
    dstate.table = g_hash_table_new(
        g_direct_hash,
        g_direct_equal
    );

    dstate.default_init_lvl = default_init_level;
}

devlog_id devlog_register(const char* type)
{
    devlog_level lvl;
    struct devlog_entry *entry = g_new0(struct devlog_entry, 1);
    const char* prefix_fmt = DEVLOG_PRE_COLOR "%s" DEVLOG_POST_COLOR "[%s - %s]" DEVLOG_COLOR_END;
    const struct level_info* info;
    devlog_id new_id = dstate.next_id++;

    if (!dstate.table) {
        return DEVLOG_INVALID_ID;
    }
    
    assert(dstate.table);

    entry->level = dstate.default_init_lvl;

    entry->type = g_new0(char, strlen(type) + 1);
    strcpy(entry->type, type);

    g_hash_table_insert(
        dstate.table,
        GINT_TO_POINTER(new_id),
        entry
    );

    for (lvl = DEVLOG_LEVEL_START; lvl < DEVLOG_LEVEL_END; ++lvl) {
        info = &levels_info[lvl];

        size_t prefix_bytes = snprintf(NULL, 0, prefix_fmt, info->color, info->name, type) + 1;
        char *prefix = g_new(char, prefix_bytes);
        sprintf(prefix, prefix_fmt, info->color, type, info->name);
        entry->prefixes[lvl] = prefix;
    }

    return new_id;
}

bool devlog_unregister(devlog_id id)
{

    enum devlog_level lvl;
    
    struct devlog_entry *entry = (struct devlog_entry*) g_hash_table_lookup(
        dstate.table,
        (gpointer) id
    );

    if (!entry) {
        return true;
    }

    for (lvl = DEVLOG_LEVEL_START; lvl < DEVLOG_LEVEL_END; ++lvl) {
        g_free(entry->prefixes[lvl]);
    }

    g_hash_table_remove(dstate.table, (gpointer) id);

    g_free(entry);

    return entry != NULL;
}

static void set_dev_level(gpointer id, gpointer entry_ptr, gpointer data_ptr)
{
    struct devlog_entry *entry = entry_ptr;
    struct table_data *data = data_ptr;

    if (!strcmp(entry->type, data->type)) {
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

    if (!dstate.table) {
        return true;
    }

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
    
    struct devlog_entry *entry = id == DEVLOG_INVALID_ID ? NULL : (struct devlog_entry*) g_hash_table_lookup(
        dstate.table,
        (gpointer) id
    );

    if (entry && lvl >= entry->level) {
        ret = snprintf(dstate.fmt, MAX_FMT_SIZE, "%s %s", entry->prefixes[lvl], fmt);
        
        if (ret >= MAX_FMT_SIZE) {
            fprintf(stderr, "The maximum format string size is too small, it must be increased.\n");
            exit(1);
        }

        if (lvl >= DEVLOG_LEVEL_WARNING) {
            vfprintf(stderr, dstate.fmt, ap);
        } else {
            vfprintf(stdout, dstate.fmt, ap);
        }
    }
}

#endif
