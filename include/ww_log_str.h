/**
 * @file ww_log_str.h
 * @brief String mode logging implementation (refactored)
 * @date 2025-12-01
 *
 * String mode features:
 * - Printf-style human-readable output
 * - Format: [LEVEL] filename:line - message
 * - Module ID parameter required (not displayed in output)
 * - Dynamic module filtering via g_ww_log_module_mask
 *
 * Usage:
 *   LOG_INF(WW_LOG_MODULE_DEFAULT, "Message");
 *   LOG_INF(WW_LOG_MODULE_BROM, "Message");
 *   LOG_INF(0, "Value: %d", value);           // 0 = DEFAULT module
 *   LOG_INF(WW_LOG_MODULE_DEMO, "Value: %d", value);
 *
 * Output format:
 *   [INF] app_main.c:42 - Application started
 *   [DBG] drv_uart.c:128 - Sending data, length=64
 *
 * Note: Module name is NOT displayed in output. If you need module name
 *       in the output, you can create custom wrappers:
 *       #define BROM_LOG_INF(fmt, ...) LOG_INF(WW_LOG_MODULE_BROM, "[BROM] " fmt, ##__VA_ARGS__)
 */

#ifndef WW_LOG_STR_H
#define WW_LOG_STR_H

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "ww_log_modules.h"

/* ========== Internal Helper Macros ========== */

/**
 * Extract filename from full path (compile-time)
 * Uses __FILE__ which contains the full path
 */
#define _WW_LOG_FILENAME(path) \
    (strrchr(path, '/') ? strrchr(path, '/') + 1 : \
     (strrchr(path, '\\') ? strrchr(path, '\\') + 1 : path))

/* ========== Output Function Declaration ========== */

/**
 * @brief Core string mode output function
 * @param filename Source filename (extracted from __FILE__)
 * @param line Line number
 * @param level Log level (WW_LOG_LEVEL_ERR/WRN/INF/DBG)
 * @param fmt Printf-style format string
 * @param ... Variable arguments for format string
 *
 * Output format: [LEVEL] filename:line - formatted_message
 */
void ww_log_str_output(const char *filename, U32 line, U8 level,
                       const char *fmt, ...);

/* ========== Public Log Macros ========== */

/**
 * LOG_ERR - Error level logging
 * @param module_id Module ID (0-31, see ww_log_modules.h)
 * @param fmt Printf-style format string
 * @param ... Format arguments (optional)
 */
#define LOG_ERR(module_id, fmt, ...) \
    do { \
        if (WW_LOG_MODULE_ENABLED(module_id) && (WW_LOG_LEVEL_THRESHOLD >= WW_LOG_LEVEL_ERR)) { \
            ww_log_str_output(_WW_LOG_FILENAME(__FILE__), __LINE__, WW_LOG_LEVEL_ERR, \
                             fmt, ##__VA_ARGS__); \
        } \
    } while(0)

/**
 * LOG_WRN - Warning level logging
 * @param module_id Module ID (0-31, see ww_log_modules.h)
 * @param fmt Printf-style format string
 * @param ... Format arguments (optional)
 */
#define LOG_WRN(module_id, fmt, ...) \
    do { \
        if (WW_LOG_MODULE_ENABLED(module_id) && (WW_LOG_LEVEL_THRESHOLD >= WW_LOG_LEVEL_WRN)) { \
            ww_log_str_output(_WW_LOG_FILENAME(__FILE__), __LINE__, WW_LOG_LEVEL_WRN, \
                             fmt, ##__VA_ARGS__); \
        } \
    } while(0)

/**
 * LOG_INF - Info level logging
 * @param module_id Module ID (0-31, see ww_log_modules.h)
 * @param fmt Printf-style format string
 * @param ... Format arguments (optional)
 */
#define LOG_INF(module_id, fmt, ...) \
    do { \
        if (WW_LOG_MODULE_ENABLED(module_id) && (WW_LOG_LEVEL_THRESHOLD >= WW_LOG_LEVEL_INF)) { \
            ww_log_str_output(_WW_LOG_FILENAME(__FILE__), __LINE__, WW_LOG_LEVEL_INF, \
                             fmt, ##__VA_ARGS__); \
        } \
    } while(0)

/**
 * LOG_DBG - Debug level logging
 * @param module_id Module ID (0-31, see ww_log_modules.h)
 * @param fmt Printf-style format string
 * @param ... Format arguments (optional)
 */
#define LOG_DBG(module_id, fmt, ...) \
    do { \
        if (WW_LOG_MODULE_ENABLED(module_id) && (WW_LOG_LEVEL_THRESHOLD >= WW_LOG_LEVEL_DBG)) { \
            ww_log_str_output(_WW_LOG_FILENAME(__FILE__), __LINE__, WW_LOG_LEVEL_DBG, \
                             fmt, ##__VA_ARGS__); \
        } \
    } while(0)

/* ========== Convenience Macros (Optional) ========== */

/**
 * If users want module names in output, they can define custom macros:
 *
 * Example:
 *   #define BROM_LOG_INF(fmt, ...) \
 *       LOG_INF(WW_LOG_MODULE_BROM, "[BROM] " fmt, ##__VA_ARGS__)
 *
 * Usage:
 *   BROM_LOG_INF("Boot complete");
 *   Output: [INF] brom_boot.c:45 - [BROM] Boot complete
 */

#endif /* WW_LOG_STR_H */
