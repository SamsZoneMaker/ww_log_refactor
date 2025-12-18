/**
 * @file ww_log_str.h
 * @brief String mode logging implementation (refactored with auto module ID)
 * @date 2025-12-17
 *
 * String mode features:
 * - Printf-style human-readable output
 * - Format: [LEVEL] filename:line - message
 * - Module ID automatically determined from CURRENT_MODULE_ID (injected by Makefile)
 * - Dynamic module filtering via g_ww_log_module_mask
 *
 * Usage:
 *   LOG_INF("Message");
 *   LOG_INF("Value: %d", value);
 *   LOG_DBG("x=%d y=%d", x, y);
 *
 * Output format:
 *   [INF] app_main.c:42 - Application started
 *   [DBG] drv_uart.c:128 - Sending data, length=64
 *
 * Note: Module ID is automatically injected by the build system based on
 *       the file's location in log_config.json. No manual module parameter needed!
 */

#ifndef WW_LOG_STR_H
#define WW_LOG_STR_H

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "type.h"
#include "ww_log_modules.h"

/* ========== Internal Helper Macros ========== */

/**
 * Extract filename from full path
 *
 * If __NOTDIR_FILE__ is defined (by Makefile), use it directly.
 * This is the optimized path - filename is determined at compile time
 * and no runtime string processing is needed.
 *
 * Otherwise, fall back to runtime extraction using strrchr().
 */
#ifdef __NOTDIR_FILE__/* Optimized: Makefile provides short filename at compile time */
    #define _WW_LOG_FILENAME(path) __NOTDIR_FILE__
#else
    /* Fallback: Runtime extraction (for compatibility) */
    #define _WW_LOG_FILENAME(path) \
        (strrchr(path, '/') ? strrchr(path, '/') + 1 : \
         (strrchr(path, '\\') ? strrchr(path, '\\') + 1 : path))
#endif

/* ========== Output Function Declaration ========== */

/**
 * @brief Core string mode output function (with internal filtering)
 * @param module_id Module ID for filtering (0-31)
 * @param filename Source filename (extracted from __FILE__)
 * @param line Line number
 * @param level Log level (WW_LOG_LEVEL_ERR/WRN/INF/DBG)
 * @param fmt Printf-style format string
 * @param ... Variable arguments for format string
 *
 * This function performs all filtering checks internally:
 * - Module enable/disable check (via g_ww_log_module_mask)
 * - Log level threshold check (via g_ww_log_level_threshold)
 *
 * Output format: [LEVEL] filename:line - formatted_message
 */
void ww_log_str_output(U8 module_id, const char *filename, U32 line, U8 level,
                       const char *fmt, ...);

/* ========== Public Log Macros ========== */

/**
 * LOG_ERR/WRN/INF/DBG - Logging macros with automatic module ID
 * @param fmt Printf-style format string
 * @param ... Format arguments (optional)
 *
 * Module ID is automatically determined from CURRENT_MODULE_ID macro,
 * which is injected by the Makefile based on log_config.json.
 *
 * If CURRENT_MODULE_ID is not defined (file not in log_config.json),
 * defaults to WW_LOG_MODULE_DEFAULT (0).
 *
 * All filtering (module mask, level threshold) is done inside the function
 * to minimize code size at each call site.
 */

/* Default to module 0 if CURRENT_MODULE_ID not defined */
// #ifndef CURRENT_MODULE_ID
// #define CURRENT_MODULE_ID  WW_LOG_MODULE_DEFAULT
// #endif

/* Default: assume module is DISABLED if CURRENT_MODULE_STATIC_EN not defined */
/* This ensures that files not in log_config.json will not output logs */
/* Same behavior as encode mode for consistency */
// #ifndef CURRENT_MODULE_STATIC_EN
// #define CURRENT_MODULE_STATIC_EN  0
// #endif

/* Helper macros for conditional expansion based on static switch */
#define _WW_LOG_STR_IF_0(...)  do {} while (0)  /* Expands to empty when disabled */
#define _WW_LOG_STR_IF_1(...)  __VA_ARGS__      /* Expands to code when enabled */

/* Token concatenation helpers for proper macro expansion */
#define _WW_LOG_STR_CAT(a, b)  _WW_LOG_STR_CAT_IMPL(a, b)
#define _WW_LOG_STR_CAT_IMPL(a, b)  a##b

/* Conditional expansion - expands to _WW_LOG_STR_IF_0 or _WW_LOG_STR_IF_1 */
#define _WW_LOG_STR_IF(cond)  _WW_LOG_STR_CAT(_WW_LOG_STR_IF_, cond)

/* Internal macro to call log output function */
#define _WW_LOG_STR_CALL(level, fmt, ...) \
    ww_log_str_output(CURRENT_MODULE_ID, _WW_LOG_FILENAME(__FILE__), __LINE__, \
                      level, fmt, ##__VA_ARGS__)

/* Main expansion macro that checks static switch and conditionally expands */
#define _WW_LOG_STR_STATIC_EXPAND(level, fmt, ...) \
    _WW_LOG_STR_IF(CURRENT_MODULE_STATIC_EN)(_WW_LOG_STR_CALL(level, fmt, ##__VA_ARGS__))

/* Static compile-time filtering for string mode */
/* Two-level filtering: compile threshold + static module switch */
#if (WW_LOG_COMPILE_THRESHOLD >= WW_LOG_LEVEL_ERR)
    #define LOG_ERR(fmt, ...) \
        _WW_LOG_STR_STATIC_EXPAND(WW_LOG_LEVEL_ERR, fmt, ##__VA_ARGS__)
#else
    #define LOG_ERR(fmt, ...) do {} while (0)
#endif

#if (WW_LOG_COMPILE_THRESHOLD >= WW_LOG_LEVEL_WRN)
    #define LOG_WRN(fmt, ...) \
        _WW_LOG_STR_STATIC_EXPAND(WW_LOG_LEVEL_WRN, fmt, ##__VA_ARGS__)
#else
    #define LOG_WRN(fmt, ...) do {} while (0)
#endif

#if (WW_LOG_COMPILE_THRESHOLD >= WW_LOG_LEVEL_INF)
    #define LOG_INF(fmt, ...) \
        _WW_LOG_STR_STATIC_EXPAND(WW_LOG_LEVEL_INF, fmt, ##__VA_ARGS__)
#else
    #define LOG_INF(fmt, ...) do {} while (0)
#endif

#if (WW_LOG_COMPILE_THRESHOLD >= WW_LOG_LEVEL_DBG)
    #define LOG_DBG(fmt, ...) \
        _WW_LOG_STR_STATIC_EXPAND(WW_LOG_LEVEL_DBG, fmt, ##__VA_ARGS__)
#else
    #define LOG_DBG(fmt, ...) do {} while (0)
#endif

/* ========== Convenience Macros (Optional) ========== */

/**
 * If users want module names in output, they can define custom macros:
 *
 * Example:
 *   #define BROM_LOG_INF(fmt, ...) \
 *       LOG_INF("[BROM] " fmt, ##__VA_ARGS__)
 *
 * Usage:
 *   BROM_LOG_INF("Boot complete");
 *   Output: [INF] brom_boot.c:45 - [BROM] Boot complete
 */

#endif /* WW_LOG_STR_H */
