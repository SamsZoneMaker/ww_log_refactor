/**
 * @file ww_log_str.h
 * @brief String mode logging implementation
 * @date 2025-11-29
 *
 * String mode features:
 * - Printf-style human-readable output
 * - Format: [MODULE][LEVEL][filename:line] message
 * - Supports optional module tag parameter
 *
 * Usage:
 *   LOG_INF("Message");                    -> [DEFA][INF][file.c:42] Message
 *   LOG_INF("[BROM]", "Message");          -> [BROM][INF][file.c:42] Message
 *   LOG_INF("Value: %d", value);           -> [DEFA][INF][file.c:42] Value: 123
 *   LOG_INF("[DEMO]", "Value: %d", value); -> [DEMO][INF][file.c:42] Value: 123
 */

#ifndef WW_LOG_STR_H
#define WW_LOG_STR_H

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

/* ========== Internal Helper Macros ========== */

/**
 * Extract filename from full path (compile-time)
 * Uses __FILE__ which contains the full path
 */
#define _WW_LOG_FILENAME(path) \
    (strrchr(path, '/') ? strrchr(path, '/') + 1 : \
     (strrchr(path, '\\') ? strrchr(path, '\\') + 1 : path))

/**
 * Argument counting macros
 * These macros count the number of arguments passed to a variadic macro
 */
#define _WW_LOG_NARG(...) \
    _WW_LOG_NARG_(__VA_ARGS__, _WW_LOG_RSEQ_N())

#define _WW_LOG_NARG_(...) \
    _WW_LOG_ARG_N(__VA_ARGS__)

#define _WW_LOG_ARG_N( \
    _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, \
    _11, _12, _13, _14, _15, _16, N, ...) N

#define _WW_LOG_RSEQ_N() \
    16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0

/**
 * Check if first argument looks like a module tag
 * A module tag starts with '[' and is a string literal
 * This is a simple heuristic check
 */
#define _WW_LOG_IS_MODULE_TAG(first_arg) \
    (_Generic((first_arg), \
        char*: 1, \
        const char*: 1, \
        default: 0) && \
     sizeof(#first_arg) > 3 && \
     #first_arg[1] == '[')

/* ========== Output Function Declaration ========== */

/**
 * @brief Core string mode output function
 * @param module Module tag string (e.g., "[DEMO]", "[BROM]")
 * @param level Log level (WW_LOG_LEVEL_ERR/WRN/INF/DBG)
 * @param filename Source filename (without path)
 * @param line Line number
 * @param fmt Printf-style format string
 * @param ... Variable arguments for format string
 */
void ww_log_str_output(const char *module, U8 level,
                       const char *filename, U32 line,
                       const char *fmt, ...);

/* ========== Level-Specific Macro Implementations ========== */

/**
 * Internal macros with explicit module parameter
 */
#define _LOG_ERR_WITH_MODULE(module, fmt, ...) \
    do { \
        if (WW_LOG_LEVEL_THRESHOLD >= WW_LOG_LEVEL_ERR) { \
            ww_log_str_output(module, WW_LOG_LEVEL_ERR, \
                _WW_LOG_FILENAME(__FILE__), __LINE__, fmt, ##__VA_ARGS__); \
        } \
    } while(0)

#define _LOG_WRN_WITH_MODULE(module, fmt, ...) \
    do { \
        if (WW_LOG_LEVEL_THRESHOLD >= WW_LOG_LEVEL_WRN) { \
            ww_log_str_output(module, WW_LOG_LEVEL_WRN, \
                _WW_LOG_FILENAME(__FILE__), __LINE__, fmt, ##__VA_ARGS__); \
        } \
    } while(0)

#define _LOG_INF_WITH_MODULE(module, fmt, ...) \
    do { \
        if (WW_LOG_LEVEL_THRESHOLD >= WW_LOG_LEVEL_INF) { \
            ww_log_str_output(module, WW_LOG_LEVEL_INF, \
                _WW_LOG_FILENAME(__FILE__), __LINE__, fmt, ##__VA_ARGS__); \
        } \
    } while(0)

#define _LOG_DBG_WITH_MODULE(module, fmt, ...) \
    do { \
        if (WW_LOG_LEVEL_THRESHOLD >= WW_LOG_LEVEL_DBG) { \
            ww_log_str_output(module, WW_LOG_LEVEL_DBG, \
                _WW_LOG_FILENAME(__FILE__), __LINE__, fmt, ##__VA_ARGS__); \
        } \
    } while(0)

/* ========== Macro Overloading Helper ========== */

/**
 * Count arguments (up to 16)
 */
#define _WW_LOG_ARG_COUNT(...) \
    _WW_LOG_ARG_COUNT_IMPL(__VA_ARGS__, 16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0)

#define _WW_LOG_ARG_COUNT_IMPL( \
    _1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,_12,_13,_14,_15,_16,N,...) N

/**
 * Concatenate helper
 */
#define _WW_LOG_CONCAT(a, b) a##b
#define _WW_LOG_CONCAT2(a, b) _WW_LOG_CONCAT(a, b)

/**
 * Overload dispatcher - select macro based on argument count
 */
#define _WW_LOG_OVERLOAD(prefix, ...) \
    _WW_LOG_CONCAT2(prefix, _WW_LOG_ARG_COUNT(__VA_ARGS__))(__VA_ARGS__)

/* ========== Per-Level Implementations ========== */

/**
 * 1 argument: LOG_ERR("message") -> use default [DEFA]
 * 2+ arguments: LOG_ERR("[MOD]", "message", ...) -> use specified module
 */
#define _LOG_ERR_1(fmt) \
    _LOG_ERR_WITH_MODULE("[DEFA]", fmt)

#define _LOG_ERR_2(module, fmt) \
    _LOG_ERR_WITH_MODULE(module, fmt)

#define _LOG_ERR_3(module, fmt, a1) \
    _LOG_ERR_WITH_MODULE(module, fmt, a1)

#define _LOG_ERR_4(module, fmt, a1, a2) \
    _LOG_ERR_WITH_MODULE(module, fmt, a1, a2)

#define _LOG_ERR_5(module, fmt, a1, a2, a3) \
    _LOG_ERR_WITH_MODULE(module, fmt, a1, a2, a3)

#define _LOG_ERR_6(module, fmt, a1, a2, a3, a4) \
    _LOG_ERR_WITH_MODULE(module, fmt, a1, a2, a3, a4)

/* Repeat for WRN */
#define _LOG_WRN_1(fmt) \
    _LOG_WRN_WITH_MODULE("[DEFA]", fmt)

#define _LOG_WRN_2(module, fmt) \
    _LOG_WRN_WITH_MODULE(module, fmt)

#define _LOG_WRN_3(module, fmt, a1) \
    _LOG_WRN_WITH_MODULE(module, fmt, a1)

#define _LOG_WRN_4(module, fmt, a1, a2) \
    _LOG_WRN_WITH_MODULE(module, fmt, a1, a2)

#define _LOG_WRN_5(module, fmt, a1, a2, a3) \
    _LOG_WRN_WITH_MODULE(module, fmt, a1, a2, a3)

#define _LOG_WRN_6(module, fmt, a1, a2, a3, a4) \
    _LOG_WRN_WITH_MODULE(module, fmt, a1, a2, a3, a4)

/* Repeat for INF */
#define _LOG_INF_1(fmt) \
    _LOG_INF_WITH_MODULE("[DEFA]", fmt)

#define _LOG_INF_2(module, fmt) \
    _LOG_INF_WITH_MODULE(module, fmt)

#define _LOG_INF_3(module, fmt, a1) \
    _LOG_INF_WITH_MODULE(module, fmt, a1)

#define _LOG_INF_4(module, fmt, a1, a2) \
    _LOG_INF_WITH_MODULE(module, fmt, a1, a2)

#define _LOG_INF_5(module, fmt, a1, a2, a3) \
    _LOG_INF_WITH_MODULE(module, fmt, a1, a2, a3)

#define _LOG_INF_6(module, fmt, a1, a2, a3, a4) \
    _LOG_INF_WITH_MODULE(module, fmt, a1, a2, a3, a4)

/* Repeat for DBG */
#define _LOG_DBG_1(fmt) \
    _LOG_DBG_WITH_MODULE("[DEFA]", fmt)

#define _LOG_DBG_2(module, fmt) \
    _LOG_DBG_WITH_MODULE(module, fmt)

#define _LOG_DBG_3(module, fmt, a1) \
    _LOG_DBG_WITH_MODULE(module, fmt, a1)

#define _LOG_DBG_4(module, fmt, a1, a2) \
    _LOG_DBG_WITH_MODULE(module, fmt, a1, a2)

#define _LOG_DBG_5(module, fmt, a1, a2, a3) \
    _LOG_DBG_WITH_MODULE(module, fmt, a1, a2, a3)

#define _LOG_DBG_6(module, fmt, a1, a2, a3, a4) \
    _LOG_DBG_WITH_MODULE(module, fmt, a1, a2, a3, a4)

/* ========== Public API Macros ========== */

/**
 * Public logging macros with optional module parameter
 *
 * Usage:
 *   LOG_ERR("message")                    -> [ERR][DEFA] file:line - message
 *   LOG_ERR("[BROM]", "message")          -> [ERR][BROM] file:line - message
 *   LOG_ERR("[BROM]", "val=%d", 123)      -> [ERR][BROM] file:line - val=123
 *
 * Note: If you want to use default module WITH parameters, you must specify [DEFA]:
 *   LOG_ERR("[DEFA]", "val=%d", 123)      -> [ERR][DEFA] file:line - val=123
 */
#define LOG_ERR(...) _WW_LOG_OVERLOAD(_LOG_ERR_, __VA_ARGS__)
#define LOG_WRN(...) _WW_LOG_OVERLOAD(_LOG_WRN_, __VA_ARGS__)
#define LOG_INF(...) _WW_LOG_OVERLOAD(_LOG_INF_, __VA_ARGS__)
#define LOG_DBG(...) _WW_LOG_OVERLOAD(_LOG_DBG_, __VA_ARGS__)

#endif /* WW_LOG_STR_H */
