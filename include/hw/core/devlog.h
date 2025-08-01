#ifndef DEVLOG_H
#define DEVLOG_H

#include <stdint.h>
#include <stdbool.h>

#define DEVLOG_INVALID_ID UINT64_MAX

enum devlog_level {
    // inclusive, always keep the the beginning
    DEVLOG_LEVEL_START,

    DEVLOG_LEVEL_DEBUG = DEVLOG_LEVEL_START,
    DEVLOG_LEVEL_TRACE,
    DEVLOG_LEVEL_INFO,
    DEVLOG_LEVEL_WARNING,
    DEVLOG_LEVEL_ERROR,

    // exclusive, always keep at the end
    DEVLOG_LEVEL_END,
};


typedef uint64_t devlog_id;
typedef unsigned int devlog_level;

#ifdef CONFIG_DEVLOG

void devlog_init(enum devlog_level default_init_level);
devlog_id devlog_register(const char* type);
bool devlog_unregister(devlog_id id);
bool devlog_set_level(const char* type, enum devlog_level level);

#ifdef CONFIG_DEVLOG_DEBUG
G_GNUC_PRINTF(2, 0) void devlog_vprintf_trace(devlog_id id, const char* fmt, va_list ap);
G_GNUC_PRINTF(2, 0) void devlog_vprintf_debug(devlog_id id, const char* fmt, va_list ap);
G_GNUC_PRINTF(2, 0) void devlog_vprintf_info(devlog_id id, const char* fmt, va_list ap);
#else
G_GNUC_PRINTF(2, 0) static inline void devlog_vprintf_trace(devlog_id id, const char* fmt, va_list ap) {}
G_GNUC_PRINTF(2, 0) static inline void devlog_vprintf_debug(devlog_id id, const char* fmt, va_list ap) {}
G_GNUC_PRINTF(2, 0) static inline void devlog_vprintf_info(devlog_id id, const char* fmt, va_list ap) {}
#endif

G_GNUC_PRINTF(2, 0) void devlog_vprintf_warn(devlog_id id, const char* fmt, va_list ap);
G_GNUC_PRINTF(2, 0) void devlog_vprintf_error(devlog_id id, const char* fmt, va_list ap);

#else

static inline void devlog_init(enum devlog_level default_init_level) {}
static inline devlog_id devlog_register(const char* type) { return DEVLOG_INVALID_ID; }
static inline bool devlog_unregister(devlog_id id) { return true; }
static inline bool devlog_set_level(const char* type, enum devlog_level level) { return true; }

G_GNUC_PRINTF(2, 0) static inline void devlog_vprintf_trace(devlog_id id, const char* fmt, va_list ap) {}
G_GNUC_PRINTF(2, 0) static inline void devlog_vprintf_debug(devlog_id id, const char* fmt, va_list ap) {}
G_GNUC_PRINTF(2, 0) static inline void devlog_vprintf_info(devlog_id id, const char* fmt, va_list ap) {}
G_GNUC_PRINTF(2, 0) static inline void devlog_vprintf_warn(devlog_id id, const char* fmt, va_list ap) {}
G_GNUC_PRINTF(2, 0) static inline void devlog_vprintf_error(devlog_id id, const char* fmt, va_list ap) {}

#endif
#endif
