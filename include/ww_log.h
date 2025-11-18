/**
 * @file ww_log.h
 * @brief Unified logging system API
 * @date 2025-11-18
 *
 * Provides unified logging interface supporting multiple modes:
 * - CONFIG_WW_LOG_DISABLED: No output (all code removed)
 * - CONFIG_WW_LOG_STR_MODE: String mode (printf-style)
 * - CONFIG_WW_LOG_ENCODE_MODE: Encode mode (binary encoding)
 */

#ifndef WW_LOG_H
#define WW_LOG_H

#include "ww_log_config.h"
#include "log_file_id.h"
#include <stdio.h>

/* ========== Global Variables ========== */

/**
 * Dynamic module enable/disable control
 * Runtime switches for each module (0=disabled, 1=enabled)
 */
extern U8 g_ww_log_mod_enable[WW_LOG_MOD_MAX];

/**
 * Global dynamic log level threshold
 * Runtime control of minimum log level (0=ERR, 1=WRN, 2=INF, 3=DBG)
 */
extern U8 g_ww_log_level_threshold;

/* ========== RAM Buffer Structure (encode_mode) ========== */

#if defined(CONFIG_WW_LOG_ENCODE_MODE) && defined(CONFIG_WW_LOG_RAM_BUFFER_EN)

/**
 * Circular buffer for encoded log entries
 */
typedef struct {
    U16 head;                                   /* Read pointer */
    U16 tail;                                   /* Write pointer */
    U32 entries[CONFIG_WW_LOG_RAM_ENTRY_NUM];  /* Log data buffer */
} WW_LOG_RAM_T;

extern WW_LOG_RAM_T g_ww_log_ram;

#endif

/* ========== Function Declarations ========== */

/**
 * @brief Initialize log system
 */
void ww_log_init(void);

/**
 * @brief Check if module logging is enabled
 * @param module_id Module ID (WW_LOG_MODULE_E)
 * @return 1 if enabled, 0 if disabled
 */
U8 ww_log_is_mod_enabled(WW_LOG_MODULE_E module_id);

/**
 * @brief Check if log level is enabled
 * @param level Log level (WW_LOG_LEVEL_E)
 * @return 1 if enabled, 0 if disabled
 */
U8 ww_log_is_level_enabled(WW_LOG_LEVEL_E level);

#ifdef CONFIG_WW_LOG_STR_MODE

/**
 * @brief String mode logging function
 * @param module_id Module ID
 * @param level Log level
 * @param file_id File ID
 * @param line Line number
 * @param fmt Format string
 */
void ww_log_str_output(WW_LOG_MODULE_E module_id, WW_LOG_LEVEL_E level,
                       U16 file_id, U16 line, const char *fmt, ...);

#endif /* CONFIG_WW_LOG_STR_MODE */

#ifdef CONFIG_WW_LOG_ENCODE_MODE

/**
 * @brief Encode mode logging with 0 parameters
 */
void ww_log_encode_0(WW_LOG_MODULE_E module_id, WW_LOG_LEVEL_E level,
                     U16 file_id, U16 line);

/**
 * @brief Encode mode logging with 1 parameter
 */
void ww_log_encode_1(WW_LOG_MODULE_E module_id, WW_LOG_LEVEL_E level,
                     U16 file_id, U16 line, U32 param1);

/**
 * @brief Encode mode logging with 2 parameters
 */
void ww_log_encode_2(WW_LOG_MODULE_E module_id, WW_LOG_LEVEL_E level,
                     U16 file_id, U16 line, U32 param1, U32 param2);

/**
 * @brief Encode mode logging with 3 parameters
 */
void ww_log_encode_3(WW_LOG_MODULE_E module_id, WW_LOG_LEVEL_E level,
                     U16 file_id, U16 line, U32 param1, U32 param2, U32 param3);

#ifdef CONFIG_WW_LOG_RAM_BUFFER_EN
/**
 * @brief Get number of entries in RAM buffer
 * @return Number of entries
 */
U16 ww_log_ram_get_count(void);

/**
 * @brief Dump all entries in RAM buffer to UART
 */
void ww_log_ram_dump(void);

/**
 * @brief Clear RAM buffer
 */
void ww_log_ram_clear(void);
#endif /* CONFIG_WW_LOG_RAM_BUFFER_EN */

#endif /* CONFIG_WW_LOG_ENCODE_MODE */

/* ========== Encoding Format ========== */

/**
 * 32-bit Log Entry Encoding (encode_mode):
 *
 *  31                    20 19                8 7        2 1      0
 * ┌──────────────────────┬────────────────────┬──────────┬────────┐
 * │   File ID (12 bits)  │  Line No (12 bits) │next_len  │ Level  │
 * │      0-4095          │     0-4095         │ (6 bits) │(2 bits)│
 * └──────────────────────┴────────────────────┴──────────┴────────┘
 *
 * - File ID: Bits 31-20 (12 bits, range 0-4095)
 * - Line No: Bits 19-8  (12 bits, range 0-4095)
 * - next_len: Bits 7-2  (6 bits, range 0-63, number of U32 params following)
 * - Level:   Bits 1-0   (2 bits, 0=ERR, 1=WRN, 2=INF, 3=DBG)
 */

#define WW_LOG_ENCODE_HEADER(file_id, line, level, param_count) \
    (((U32)(file_id) << 20) | ((U32)(line) << 8) | ((U32)(param_count) << 2) | ((U32)(level)))

/* Helper macros to extract fields from encoded header */
#define WW_LOG_GET_FILE_ID(header)    (((header) >> 20) & 0xFFF)
#define WW_LOG_GET_LINE_NO(header)    (((header) >> 8) & 0xFFF)
#define WW_LOG_GET_PARAM_COUNT(header) (((header) >> 2) & 0x3F)
#define WW_LOG_GET_LEVEL(header)      ((header) & 0x03)

/* ========== Unified Logging Macros ========== */

/**
 * Macro helpers to count arguments
 * These macros help determine how many parameters are passed
 */
#define WW_LOG_NARGS_(_10, _9, _8, _7, _6, _5, _4, _3, _2, _1, N, ...) N
#define WW_LOG_NARGS(...) WW_LOG_NARGS_(__VA_ARGS__, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0)
#define WW_LOG_HAS_ARGS(...) WW_LOG_NARGS(__VA_ARGS__)

/* ========== Mode-Specific Macro Implementations ========== */

#if defined(CONFIG_WW_LOG_DISABLED)

    /* DISABLED MODE: All logging code removed */
    #define TEST_LOG_ERR_MSG(fmt, ...)  do { } while(0)
    #define TEST_LOG_WRN_MSG(fmt, ...)  do { } while(0)
    #define TEST_LOG_INF_MSG(fmt, ...)  do { } while(0)
    #define TEST_LOG_DBG_MSG(fmt, ...)  do { } while(0)

#elif defined(CONFIG_WW_LOG_STR_MODE)

    /* STRING MODE: Printf-style logging */
    #define TEST_LOG_ERR_MSG(fmt, ...) \
        ww_log_str_output(CURRENT_MODULE_ID, WW_LOG_LEVEL_ERR, CURRENT_FILE_ID, __LINE__, fmt, ##__VA_ARGS__)

    #define TEST_LOG_WRN_MSG(fmt, ...) \
        ww_log_str_output(CURRENT_MODULE_ID, WW_LOG_LEVEL_WRN, CURRENT_FILE_ID, __LINE__, fmt, ##__VA_ARGS__)

    #define TEST_LOG_INF_MSG(fmt, ...) \
        ww_log_str_output(CURRENT_MODULE_ID, WW_LOG_LEVEL_INF, CURRENT_FILE_ID, __LINE__, fmt, ##__VA_ARGS__)

    #define TEST_LOG_DBG_MSG(fmt, ...) \
        ww_log_str_output(CURRENT_MODULE_ID, WW_LOG_LEVEL_DBG, CURRENT_FILE_ID, __LINE__, fmt, ##__VA_ARGS__)

#elif defined(CONFIG_WW_LOG_ENCODE_MODE)

    /* ENCODE MODE: Binary encoding with parameter detection */

    /* Macro to select correct encode function based on argument count */
    #define WW_LOG_ENCODE_SELECT_0(mod, lvl, fid, ln, fmt) \
        ww_log_encode_0(mod, lvl, fid, ln)

    #define WW_LOG_ENCODE_SELECT_1(mod, lvl, fid, ln, fmt, p1) \
        ww_log_encode_1(mod, lvl, fid, ln, (U32)(p1))

    #define WW_LOG_ENCODE_SELECT_2(mod, lvl, fid, ln, fmt, p1, p2) \
        ww_log_encode_2(mod, lvl, fid, ln, (U32)(p1), (U32)(p2))

    #define WW_LOG_ENCODE_SELECT_3(mod, lvl, fid, ln, fmt, p1, p2, p3) \
        ww_log_encode_3(mod, lvl, fid, ln, (U32)(p1), (U32)(p2), (U32)(p3))

    /* Overload resolution based on argument count (after format string) */
    #define WW_LOG_ENCODE_DISPATCH(_1, _2, _3, _4, NAME, ...) NAME

    #define WW_LOG_ENCODE_CALL(mod, lvl, fid, ln, fmt, ...) \
        WW_LOG_ENCODE_DISPATCH(dummy, ##__VA_ARGS__, \
            WW_LOG_ENCODE_SELECT_3, \
            WW_LOG_ENCODE_SELECT_2, \
            WW_LOG_ENCODE_SELECT_1, \
            WW_LOG_ENCODE_SELECT_0, \
            unused)(mod, lvl, fid, ln, fmt, ##__VA_ARGS__)

    #define TEST_LOG_ERR_MSG(fmt, ...) \
        WW_LOG_ENCODE_CALL(CURRENT_MODULE_ID, WW_LOG_LEVEL_ERR, CURRENT_FILE_ID, __LINE__, fmt, ##__VA_ARGS__)

    #define TEST_LOG_WRN_MSG(fmt, ...) \
        WW_LOG_ENCODE_CALL(CURRENT_MODULE_ID, WW_LOG_LEVEL_WRN, CURRENT_FILE_ID, __LINE__, fmt, ##__VA_ARGS__)

    #define TEST_LOG_INF_MSG(fmt, ...) \
        WW_LOG_ENCODE_CALL(CURRENT_MODULE_ID, WW_LOG_LEVEL_INF, CURRENT_FILE_ID, __LINE__, fmt, ##__VA_ARGS__)

    #define TEST_LOG_DBG_MSG(fmt, ...) \
        WW_LOG_ENCODE_CALL(CURRENT_MODULE_ID, WW_LOG_LEVEL_DBG, CURRENT_FILE_ID, __LINE__, fmt, ##__VA_ARGS__)

#else
    #error "No log mode selected! Define one of: CONFIG_WW_LOG_DISABLED, CONFIG_WW_LOG_STR_MODE, CONFIG_WW_LOG_ENCODE_MODE"
#endif

#endif /* WW_LOG_H */
