/**
 * @file ww_log_config.h
 * @brief Log system configuration
 * @date 2025-11-18
 */

#ifndef WW_LOG_CONFIG_H
#define WW_LOG_CONFIG_H

#include <stdint.h>

/* ========== Log Mode Selection (Choose ONE) ========== */

/* Uncomment ONE of the following modes: */
// #define CONFIG_WW_LOG_DISABLED       /* No logging output */
#define CONFIG_WW_LOG_STR_MODE          /* String mode (printf-style) */
// #define CONFIG_WW_LOG_ENCODE_MODE    /* Encode mode (binary encoding) */

/* ========== Module Static Enable/Disable Switches ========== */

/**
 * Static switches: Compile-time control
 * When disabled, all log code for that module is removed from binary
 */
#define CONFIG_WW_LOG_MOD_DEMO_EN       1
#define CONFIG_WW_LOG_MOD_TEST_EN       1
#define CONFIG_WW_LOG_MOD_APP_EN        1
#define CONFIG_WW_LOG_MOD_DRIVERS_EN    1
#define CONFIG_WW_LOG_MOD_BROM_EN       1

/* ========== RAM Buffer Configuration ========== */

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

/* ========== Type Definitions ========== */

typedef uint8_t  U8;
typedef uint16_t U16;
typedef uint32_t U32;
typedef int8_t   S8;
typedef int16_t  S16;
typedef int32_t  S32;

/* ========== Log Level Enumeration ========== */

/**
 * Log levels (2-bit encoding in encode_mode)
 * 0: ERR - Error (system failures, critical issues)
 * 1: WRN - Warning (potential problems)
 * 2: INF - Info (important state changes)
 * 3: DBG - Debug (detailed execution flow)
 */
typedef enum {
    WW_LOG_LEVEL_ERR = 0,
    WW_LOG_LEVEL_WRN = 1,
    WW_LOG_LEVEL_INF = 2,
    WW_LOG_LEVEL_DBG = 3,
} WW_LOG_LEVEL_E;

/* ========== Module ID Enumeration ========== */

/**
 * Module IDs for dynamic enable/disable control
 */
typedef enum {
    WW_LOG_MOD_DEMO = 0,
    WW_LOG_MOD_TEST = 1,
    WW_LOG_MOD_APP = 2,
    WW_LOG_MOD_DRIVERS = 3,
    WW_LOG_MOD_BROM = 4,
    WW_LOG_MOD_MAX
} WW_LOG_MODULE_E;

#endif /* WW_LOG_CONFIG_H */
