/**
 * @file ww_log_encode.h
 * @brief Encode mode logging implementation (variadic function version)
 * @date 2025-12-04
 *
 * Encode mode features:
 * - Binary encoding for minimal code size
 * - No format strings stored in ROM (fmt parameter is discarded)
 * - Variadic function for minimal macro expansion
 * - All filtering done in function
 *
 * Encoding Layout (32 bits):
 * ┌─────────────┬─────────────┬──────────────┬────────┐
 * │ LOG_ID      │ LINE        │ DATA_LEN     │ LEVEL  │
 * │ (12 bits)   │ (12 bits)   │ (6 bits)     │(2 bits)│
 * │ 31       20 │ 19        8 │ 7          2 │ 1    0 │
 * └─────────────┴─────────────┴──────────────┴────────┘
 *
 * Code Size Optimization:
 * - Macro expansion is just a single function call
 * - No stack array creation at call site
 * - All filtering and parameter extraction done inside function
 */

#ifndef WW_LOG_ENCODE_H
#define WW_LOG_ENCODE_H

#include "file_id.h"
#include "ww_log_modules.h"
#include "ww_log_config.h"

/* ========== Module ID Extraction ========== */

/**
 * Extract module ID from LOG_ID
 * Since each module reserves 32 slots (DIR_BASE = module_id << 5),
 * we can extract module_id by shifting right by 5 bits
 */
#define WW_LOG_GET_MODULE_ID(log_id)  ((log_id) >> 5)

/* ========== Encoding Macros ========== */

/**
 * Encode a log entry into 32-bit format
 */
#define WW_LOG_ENCODE(log_id, line, data_len, level) \
    ((U32)( \
        (((U32)(log_id) & 0xFFF) << 20) | \
        (((U32)(line) & 0xFFF) << 8) | \
        (((U32)(data_len) & 0x3F) << 2) | \
        ((U32)(level) & 0x3) \
    ))

/**
 * Decode macros to extract fields from encoded log
 */
#define WW_LOG_DECODE_LOG_ID(encoded)      (((encoded) >> 20) & 0xFFF)
#define WW_LOG_DECODE_LINE(encoded)        (((encoded) >> 8) & 0xFFF)
#define WW_LOG_DECODE_DATA_LEN(encoded)    (((encoded) >> 2) & 0x3F)
#define WW_LOG_DECODE_LEVEL(encoded)       ((encoded) & 0x3)

/* ========== Output Function Declaration ========== */

/**
 * @brief Core encode mode output function (variadic version)
 * @param log_id Module/file identifier (12 bits, 0-4095)
 * @param line Source line number
 * @param level Log level (0-3)
 * @param param_count Number of parameters (0-16)
 * @param ... Variable parameters (each as U32)
 *
 * This function performs all filtering internally:
 * - Module enable check (via g_ww_log_module_mask)
 * - Level threshold check (via g_ww_log_level_threshold)
 *
 * Parameters are extracted via va_list inside the function,
 * eliminating the need to create arrays at each call site.
 */
void ww_log_encode_output(U16 log_id, U16 line, U8 level,
                          U8 param_count, ...);

/* ========== Argument Counting Macro ========== */

/**
 * Count number of variadic arguments (supports 0-16)
 * Uses ##__VA_ARGS__ GNU extension to handle empty case
 */
#define _WW_LOG_ARG_COUNT(...) \
    _WW_LOG_ARG_COUNT_IMPL(0, ##__VA_ARGS__, \
        16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0)

#define _WW_LOG_ARG_COUNT_IMPL( \
    _0,_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,_12,_13,_14,_15,_16,N,...) N

/* ========== Public API Macros ========== */

/**
 * Public logging macros for encode mode
 *
 * IMPORTANT: The 'tag' and 'fmt' parameters are COMPLETELY IGNORED
 * in encode mode - they are NOT compiled into the binary!
 *
 * Macro expansion is minimal - just a single function call:
 *   LOG_INF(tag, "val=%d", 123)
 *   -> ww_log_encode_output(CURRENT_LOG_ID, __LINE__, 2, 1, 123)
 *
 * Usage:
 *   LOG_ERR(tag, "message")              -> 0 params
 *   LOG_INF(tag, "val=%d", 123)          -> 1 param
 *   LOG_DBG(tag, "x=%d y=%d", 10, 20)    -> 2 params
 */
/* Static compile-time filtering for encode mode */
#if (WW_LOG_COMPILE_THRESHOLD >= WW_LOG_LEVEL_ERR)
    #define LOG_ERR(tag, fmt, ...) \
        ww_log_encode_output(CURRENT_LOG_ID, __LINE__, WW_LOG_LEVEL_ERR, \
                             _WW_LOG_ARG_COUNT(__VA_ARGS__), ##__VA_ARGS__)
#else
    #define LOG_ERR(tag, fmt, ...) do {} while (0)
#endif

#if (WW_LOG_COMPILE_THRESHOLD >= WW_LOG_LEVEL_WRN)
    #define LOG_WRN(tag, fmt, ...) \
        ww_log_encode_output(CURRENT_LOG_ID, __LINE__, WW_LOG_LEVEL_WRN, \
                             _WW_LOG_ARG_COUNT(__VA_ARGS__), ##__VA_ARGS__)
#else
    #define LOG_WRN(tag, fmt, ...) do {} while (0)
#endif

#if (WW_LOG_COMPILE_THRESHOLD >= WW_LOG_LEVEL_INF)
    #define LOG_INF(tag, fmt, ...) \
        ww_log_encode_output(CURRENT_LOG_ID, __LINE__, WW_LOG_LEVEL_INF, \
                             _WW_LOG_ARG_COUNT(__VA_ARGS__), ##__VA_ARGS__)
#else
    #define LOG_INF(tag, fmt, ...) do {} while (0)
#endif

#if (WW_LOG_COMPILE_THRESHOLD >= WW_LOG_LEVEL_DBG)
    #define LOG_DBG(tag, fmt, ...) \
        ww_log_encode_output(CURRENT_LOG_ID, __LINE__, WW_LOG_LEVEL_DBG, \
                             _WW_LOG_ARG_COUNT(__VA_ARGS__), ##__VA_ARGS__)
#else
    #define LOG_DBG(tag, fmt, ...) do {} while (0)
#endif

/* ========== RAM Buffer (Optional) ========== */

#ifdef WW_LOG_ENCODE_RAM_BUFFER_EN

#ifndef WW_LOG_RAM_BUFFER_SIZE
#define WW_LOG_RAM_BUFFER_SIZE  128
#endif

typedef struct {
    U32 magic;
    U16 head;
    U16 tail;
    U32 entries[WW_LOG_RAM_BUFFER_SIZE];
} WW_LOG_RAM_BUFFER_T;

#define WW_LOG_RAM_MAGIC  0x574C4F47

extern WW_LOG_RAM_BUFFER_T g_ww_log_ram_buffer;

U16 ww_log_ram_get_count(void);
void ww_log_ram_dump(void);
void ww_log_ram_clear(void);

#endif /* WW_LOG_ENCODE_RAM_BUFFER_EN */

#endif /* WW_LOG_ENCODE_H */
