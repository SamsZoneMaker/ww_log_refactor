/**
 * @file ww_log.h
 * @brief Unified logging system - main entry point
 * @date 2025-12-01
 *
 * Unified logging API supporting two modes:
 * - WW_LOG_MODE_STR: String mode (printf-style, human-readable)
 * - WW_LOG_MODE_ENCODE: Encode mode (binary encoding, minimal code size)
 *
 * Usage:
 *   LOG_ERR(module_id, fmt, ...)   - Error level (str mode uses module_id, encode mode uses tag)
 *   LOG_WRN(module_id, fmt, ...)   - Warning level
 *   LOG_INF(module_id, fmt, ...)   - Info level
 *   LOG_DBG(module_id, fmt, ...)   - Debug level
 *
 * Mode selection is done at compile time via Makefile or build system.
 */

#ifndef WW_LOG_H
#define WW_LOG_H

#include "ww_log_config.h"

/**
 * Log level definitions (using macros for compile-time comparison)
 */
#define WW_LOG_LEVEL_ERR  0  /* Error: system failures, critical issues */
#define WW_LOG_LEVEL_WRN  1  /* Warning: potential problems */
#define WW_LOG_LEVEL_INF  2  /* Info: important state changes */
#define WW_LOG_LEVEL_DBG  3  /* Debug: detailed execution flow */

/**
 * Global compile-time log level threshold
 * Logs below this level will be compiled out
 * Define this in your build system or use default
 */
#ifndef WW_LOG_LEVEL_THRESHOLD
#define WW_LOG_LEVEL_THRESHOLD  WW_LOG_LEVEL_DBG  /* Default: allow all */
#endif

/**
 * Mode selection: Choose one of the following
 * - WW_LOG_MODE_STR: String mode (default)
 * - WW_LOG_MODE_ENCODE: Encode mode
 * - WW_LOG_MODE_DISABLED: All logging disabled
 */
#if defined(WW_LOG_MODE_ENCODE)
    #include "ww_log_encode.h"
#elif defined(WW_LOG_MODE_STR)
    #include "ww_log_str.h"
#elif defined(WW_LOG_MODE_DISABLED)
    /* All logging macros become no-ops */
    #define LOG_ERR(...)  do { } while(0)
    #define LOG_WRN(...)  do { } while(0)
    #define LOG_INF(...)  do { } while(0)
    #define LOG_DBG(...)  do { } while(0)
#else
    /* Default to string mode if not specified */
    #define WW_LOG_MODE_STR
    #include "ww_log_str.h"
#endif

/**
 * @brief Initialize log system (optional, for future extensions)
 */
void ww_log_init(void);

#endif /* WW_LOG_H */
