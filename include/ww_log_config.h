/**
 * @file ww_log_config.h
 * @brief Log system configuration
 * @date 2025-12-01
 */

#ifndef WW_LOG_CONFIG_H
#define WW_LOG_CONFIG_H

#include <stdint.h>

/* ========== Log Mode Selection (Choose ONE) ========== */

/* Uncomment ONE of the following modes: */
// #define CONFIG_WW_LOG_DISABLED       /* No logging output */
#define CONFIG_WW_LOG_STR_MODE          /* String mode (printf-style) */
// #define CONFIG_WW_LOG_ENCODE_MODE    /* Encode mode (binary encoding) */

/* ========== RAM Buffer Configuration (Encode Mode) ========== */

/**
 * Number of log entries in RAM circular buffer
 * Each entry is 32-bit (4 bytes)
 * Total RAM usage = CONFIG_WW_LOG_RAM_ENTRY_NUM * 4 bytes
 */
#define CONFIG_WW_LOG_RAM_ENTRY_NUM     1024

/**
 * Enable RAM buffer (for encode_mode)
 * When enabled, logs are stored in circular buffer
 */
#define CONFIG_WW_LOG_RAM_BUFFER_EN     1

/* ========== Global Log Level Threshold ========== */

/**
 * Global compile-time log level filter
 * Logs below this level will be compiled out
 * 0=ERR, 1=WRN, 2=INF, 3=DBG
 */
#define CONFIG_WW_LOG_LEVEL_THRESHOLD   3  /* DBG - allow all */

/* ========== Output Targets ========== */

/**
 * Enable UART output
 * Logs are printed to stdout/UART in real-time
 */
#define CONFIG_WW_LOG_OUTPUT_UART       1

/**
 * Enable RAM buffer output
 * Logs are stored in circular buffer (supports warm restart)
 */
#define CONFIG_WW_LOG_OUTPUT_RAM        1

/* ========== Type Definitions ========== */

typedef uint8_t  U8;
typedef uint16_t U16;
typedef uint32_t U32;
typedef int8_t   S8;
typedef int16_t  S16;
typedef int32_t  S32;

/* ========== Log Level Definitions ========== */

/**
 * Log levels are defined in ww_log.h as macros for compile-time comparison
 *
 * Values:
 *   0: ERR - Error (system failures, critical issues)
 *   1: WRN - Warning (potential problems)
 *   2: INF - Info (important state changes)
 *   3: DBG - Debug (detailed execution flow)
 */

/* ========== Module Control ========== */

/**
 * Module IDs and dynamic enable/disable control are now defined in ww_log_modules.h
 *
 * Use the following APIs for runtime control:
 *   - ww_log_set_module_mask(U32 mask)     // Set entire mask
 *   - ww_log_enable_module(U8 module_id)   // Enable one module
 *   - ww_log_disable_module(U8 module_id)  // Disable one module
 *
 * Module IDs:
 *   - WW_LOG_MODULE_DEFAULT   (0)
 *   - WW_LOG_MODULE_DEMO      (1)
 *   - WW_LOG_MODULE_TEST      (2)
 *   - WW_LOG_MODULE_APP       (3)
 *   - WW_LOG_MODULE_DRIVERS   (4)
 *   - WW_LOG_MODULE_BROM      (5)
 */

#endif /* WW_LOG_CONFIG_H */
