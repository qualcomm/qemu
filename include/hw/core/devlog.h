#ifndef DEVLOG_H
#define DEVLOG_H

#include <stdint.h>
#include <stdbool.h>

#define DEVLOG_INVALID_ID UINT64_MAX

#define DEVLOG_LEVEL_TRACE 0
#define DEVLOG_LEVEL_DEBUG 1
#define DEVLOG_LEVEL_INFO  2
#define DEVLOG_LEVEL_WARN  3
#define DEVLOG_LEVEL_ERROR 4

#define DEVLOG_DEFAULT_LEVEL DEVLOG_LEVEL_WARN

typedef uint64_t devlog_id;
typedef unsigned int devlog_level;

#ifdef CONFIG_DEVLOG

devlog_id devlog_register(const char* type);
bool devlog_unregister(devlog_id id);

#ifdef CONFIG_DEVLOG_DEBUG
void devlog_vprintf_trace(devlog_id id, const char* fmt, va_list ap) G_GNUC_PRINTF(2, 0);
void devlog_vprintf_debug(devlog_id id, const char* fmt, va_list ap) G_GNUC_PRINTF(2, 0);
void devlog_vprintf_info(devlog_id id, const char* fmt, va_list ap) G_GNUC_PRINTF(2, 0);
#else
G_GNUC_PRINTF(2, 0) static inline void devlog_vprintf_trace(devlog_id id, const char* fmt, va_list ap) {}
G_GNUC_PRINTF(2, 0) static inline void devlog_vprintf_debug(devlog_id id, const char* fmt, va_list ap) {}
G_GNUC_PRINTF(2, 0) static inline void devlog_vprintf_info(devlog_id id, const char* fmt, va_list ap) {}
#endif

void devlog_vprintf_warn(devlog_id id, const char* fmt, va_list ap) G_GNUC_PRINTF(2, 0);
void devlog_vprintf_error(devlog_id id, const char* fmt, va_list ap) G_GNUC_PRINTF(2, 0);

#else

static inline devlog_id devlog_register(const char* type) { return 0; }
static inline bool devlog_unregister(devlog_id id) { return true; }

G_GNUC_PRINTF(2, 0) static inline void devlog_vprintf_trace(devlog_id id, const char* fmt, va_list ap) {}
G_GNUC_PRINTF(2, 0) static inline void devlog_vprintf_debug(devlog_id id, const char* fmt, va_list ap) {}
G_GNUC_PRINTF(2, 0) static inline void devlog_vprintf_info(devlog_id id, const char* fmt, va_list ap) {}
G_GNUC_PRINTF(2, 0) static inline void devlog_vprintf_trace(devlog_id id, const char* fmt, va_list ap) {}
G_GNUC_PRINTF(2, 0) static inline void devlog_vprintf_debug(devlog_id id, const char* fmt, va_list ap) {}
G_GNUC_PRINTF(2, 0) static inline void devlog_vprintf_info(devlog_id id, const char* fmt, va_list ap) {}

#endif
#endif
