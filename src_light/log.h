
#pragma once

#include "wiced_bt_trace.h"
#include "wiced_hal_wdog.h"

#if _ANDONDEBUG
void write_multi_routine(const char *format, const char *file, int line, ...);
char *file_name_of(char *file);
#endif

#define lOGLEVEL_ERROR (1)
#define LOGLEVEL_WARNNING (2)
#define LOGLEVEL_INFO (3)
#define LOGLEVEL_DEBUG (4)
#define LOGLEVEL_VERBOSE (5)

// #if _ANDONDEBUG
// #define LOGLEVEL _ANDONDEBUG
// #else
// #define LOGLEVEL -1
// #endif

// #ifndef LOGLEVEL
// #define LOGLEVEL LOGLEVEL_WARNNING
// #endif

#define LOG_APPEND(format, ...) WICED_BT_TRACE(format, ##__VA_ARGS__)

#if LOGLEVEL >= lOGLEVEL_ERROR
#define LOG_ERROR(format, ...)                                                     \
    do                                                                             \
    {                                                                              \
        write_multi_routine("[ERROR] " format, __FILE__, __LINE__, ##__VA_ARGS__); \
    } while (0)
    // do                                                                             \
    // {                                                                              \
    //     WICED_BT_TRACE("[ERROR] %s %d " format, __func__, __LINE__, ##__VA_ARGS__); \
    // } while (0)
#define LOG_ERROR_APPEND(format, ...) WICED_BT_TRACE(format, ##__VA_ARGS__)
#else
#define LOG_ERROR(format, ...)
#define LOG_ERROR_APPEND(format, ...)
#endif

#if LOGLEVEL >= LOGLEVEL_WARNNING
#define LOG_WARNING(format, ...)                                                     \
    do                                                                               \
    {                                                                                \
        write_multi_routine("[WARNING] " format, __FILE__, __LINE__, ##__VA_ARGS__);\
    } while (0)
    // do                                                                               \
    // {                                                                                \
    //     WICED_BT_TRACE("[WARNING] %s %d " format, __func__, __LINE__, ##__VA_ARGS__);\
    // } while (0)
#define LOG_WARNING_APPEND(format, ...) WICED_BT_TRACE(format, ##__VA_ARGS__)

#else
#define LOG_WARNING(format, ...)
#define LOG_WARNING_APPEND(format, ...)
#endif

#if LOGLEVEL >= LOGLEVEL_INFO
#define LOG_INFO(format, ...)                                                     \
    do                                                                            \
    {                                                                             \
        write_multi_routine("[INFO] " format, __FILE__, __LINE__, ##__VA_ARGS__); \
    } while (0)
    // do                                                                            \
    // {                                                                             \
    //     WICED_BT_TRACE("[DEBUG] %s %d " format, __func__, __LINE__, ##__VA_ARGS__);\
    // } while (0)

#define LOG_INFO_APPEND(format, ...) WICED_BT_TRACE(format, ##__VA_ARGS__)
#else
#define LOG_INFO(format, ...)
#define LOG_INFO_APPEND(format, ...)
#endif

#if LOGLEVEL >= LOGLEVEL_DEBUG
#define LOG_DEBUG(format, ...)                                                     \
    do                                                                               \
    {                                                                                \
        wiced_printf(NULL,0,"%s %d [DEBUG] " format, file_name_of(__FILE__), __LINE__, ##__VA_ARGS__); \
    } while (0)
    // do                                                                             \
    // {                                                                              \
    //     WICED_BT_TRACE("[DEBUG] %s %d " format, __func__, __LINE__, ##__VA_ARGS__); \
    // } while (0)

#define LOG_DEBUG_APPEND(format, ...) WICED_BT_TRACE(format, ##__VA_ARGS__)

#else
#define LOG_DEBUG(format, ...)
#define LOG_DEBUG_APPEND(format, ...)
#endif

#if LOGLEVEL >= LOGLEVEL_VERBOSE
#define LOG_VERBOSE(format, ...)                                                     \
    do                                                                               \
    {                                                                                \
        wiced_printf(NULL,0,"[VERBOSE] %s %d " format, file_name_of(__FILE__), __LINE__, ##__VA_ARGS__); \
    } while (0)
    // do                                                                               \
    // {                                                                                \
    //     WICED_BT_TRACE("[VERBOSE] %s %d " format, file_name_of(__FILE__), __LINE__, ##__VA_ARGS__); \
    // } while (0)
#define LOG_VERBOSE_APPEND(format, ...) WICED_BT_TRACE(format, ##__VA_ARGS__)

#else
#define LOG_VERBOSE(format, ...)
#define LOG_VERBOSE_APPEND(format, ...)
#endif

#define WICED_LOG_ERROR(...)        LOG_ERROR(__VA_ARGS__)
#define WICED_LOG_WARNING(...)      LOG_WARNING(__VA_ARGS__)
#define WICED_LOG_INF(...)          LOG_INFO(__VA_ARGS__)
#define WICED_LOG_DEBUG(...)        LOG_DEBUG(__VA_ARGS__)
#define WICED_LOG_VERBOSE(...)      LOG_VERBOSE(__VA_ARGS__)

#define ASSERT(expr)                       \
    do                                     \
    {                                      \
        if (!expr)                         \
        {                                  \
            LOG_ERROR("ASSERT ERROR!");    \
            LOG_ERROR(#expr);              \
            wiced_hal_wdog_reset_system(); \
        }                                  \
    } while (0)
