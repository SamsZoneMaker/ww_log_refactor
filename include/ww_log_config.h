/**
 * @file ww_log_config.h
 * @brief Log system configuration and type definitions
 * @date 2025-12-01
 */

#ifndef WW_LOG_CONFIG_H
#define WW_LOG_CONFIG_H

#include <stdint.h>

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
 * Module IDs and dynamic enable/disable control are defined in ww_log_modules.h
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

/* ========== Mode Selection ========== */

/**
 * Log mode is selected at compile time via Makefile:
 *   - WW_LOG_MODE_STR: String mode (printf-style, human-readable)
 *   - WW_LOG_MODE_ENCODE: Encode mode (binary encoding, minimal code size)
 *   - WW_LOG_MODE_DISABLED: All logging disabled
 *
 * The Makefile defines these via -D compiler flags.
 * See ww_log.h for mode-specific implementations.
 */

/* ========== Encode Mode RAM Buffer ========== */

/**
 * RAM buffer is configured in ww_log_encode.h:
 *   - WW_LOG_ENCODE_RAM_BUFFER_EN: Enable/disable RAM buffer (ifdef guard)
 *   - WW_LOG_RAM_BUFFER_SIZE: Number of 32-bit entries (default: 128)
 *
 * Total RAM usage = WW_LOG_RAM_BUFFER_SIZE * 4 bytes
 */

#endif /* WW_LOG_CONFIG_H */
