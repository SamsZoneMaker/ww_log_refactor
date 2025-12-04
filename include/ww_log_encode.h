/**
 * @file ww_log_encode.h
 * @brief Encode mode logging implementation (refactored for minimal code size)
 * @date 2025-12-04
 *
 * Encode mode features:
 * - Binary encoding for minimal code size
 * - No format strings stored in ROM (fmt parameter is discarded)
 * - All filtering done in function to minimize macro expansion
 *
 * Encoding Layout (32 bits):
 * ┌─────────────┬─────────────┬──────────────┬────────┐
 * │ LOG_ID      │ LINE        │ DATA_LEN     │ LEVEL  │
 * │ (12 bits)   │ (12 bits)   │ (6 bits)     │(2 bits)│
 * │ 31       20 │ 19        8 │ 7          2 │ 1    0 │
 * └─────────────┴─────────────┴──────────────┴────────┘
 *
 * Where:
 * - LOG_ID: Module base ID + file offset (from CURRENT_LOG_ID)
 * - LINE: Source line number (__LINE__)
 * - DATA_LEN: Number of data parameters (0-63, currently support 0-8)
 * - LEVEL: Log level (ERR=0, WRN=1, INF=2, DBG=3)
 *
 * Code Size Optimization:
 * - All filtering (module mask, level threshold) is done inside function
 * - Macro only packs parameters and calls function - minimal expansion
 * - fmt string is completely ignored (not compiled into binary)
 */

#ifndef WW_LOG_ENCODE_H
#define WW_LOG_ENCODE_H

#include "file_id.h"
#include "ww_log_modules.h"

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
 * @brief Core encode mode output function (with internal filtering)
 * @param log_id Module/file identifier (12 bits, 0-4095)
 * @param line Source line number
 * @param level Log level (0-3)
 * @param params Array of parameter values (each as U32), NULL if no params
 * @param param_count Number of parameters (0-8)
 *
 * This function performs all filtering internally:
 * - Module enable check (via g_ww_log_module_mask)
 * - Level threshold check (via g_ww_log_level_threshold)
 *
 * Only outputs if both checks pass.
 */
void ww_log_encode_output(U16 log_id, U16 line, U8 level,
                          const U32 *params, U8 param_count);

/* ========== Argument Counting for Variadic Macros ========== */

/**
 * Count number of variadic arguments (supports 0-10)
 * Note: This counts ALL arguments after fmt
 */
#define _WW_LOG_ARG_COUNT(...) \
    _WW_LOG_ARG_COUNT_IMPL(0, ##__VA_ARGS__, 10,9,8,7,6,5,4,3,2,1,0)

#define _WW_LOG_ARG_COUNT_IMPL(_0,_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,N,...) N

/* ========== Internal Helper Macros ========== */

/**
 * Helper to create parameter array
 * For 0 params: pass NULL
 * For 1+ params: create array on stack
 */
#define _WW_LOG_ENCODE_CALL_0(log_id, line, level) \
    ww_log_encode_output((log_id), (line), (level), (const U32*)0, 0)

#define _WW_LOG_ENCODE_CALL_N(log_id, line, level, count, ...) \
    do { \
        U32 _params[] = {__VA_ARGS__}; \
        ww_log_encode_output((log_id), (line), (level), _params, (count)); \
    } while(0)

/**
 * Dispatch macro: choose between 0-param and N-param versions
 */
#define _WW_LOG_ENCODE_DISPATCH(log_id, line, level, count, ...) \
    _WW_LOG_ENCODE_SELECT(count)(log_id, line, level, count, ##__VA_ARGS__)

#define _WW_LOG_ENCODE_SELECT(count) _WW_LOG_ENCODE_SELECT_IMPL(count)
#define _WW_LOG_ENCODE_SELECT_IMPL(count) _WW_LOG_ENCODE_##count

/* Select between 0-param call and N-param call */
#define _WW_LOG_ENCODE_0(log_id, line, level, count, ...) \
    _WW_LOG_ENCODE_CALL_0(log_id, line, level)

#define _WW_LOG_ENCODE_1(log_id, line, level, count, ...) \
    _WW_LOG_ENCODE_CALL_N(log_id, line, level, count, __VA_ARGS__)
#define _WW_LOG_ENCODE_2(log_id, line, level, count, ...) \
    _WW_LOG_ENCODE_CALL_N(log_id, line, level, count, __VA_ARGS__)
#define _WW_LOG_ENCODE_3(log_id, line, level, count, ...) \
    _WW_LOG_ENCODE_CALL_N(log_id, line, level, count, __VA_ARGS__)
#define _WW_LOG_ENCODE_4(log_id, line, level, count, ...) \
    _WW_LOG_ENCODE_CALL_N(log_id, line, level, count, __VA_ARGS__)
#define _WW_LOG_ENCODE_5(log_id, line, level, count, ...) \
    _WW_LOG_ENCODE_CALL_N(log_id, line, level, count, __VA_ARGS__)
#define _WW_LOG_ENCODE_6(log_id, line, level, count, ...) \
    _WW_LOG_ENCODE_CALL_N(log_id, line, level, count, __VA_ARGS__)
#define _WW_LOG_ENCODE_7(log_id, line, level, count, ...) \
    _WW_LOG_ENCODE_CALL_N(log_id, line, level, count, __VA_ARGS__)
#define _WW_LOG_ENCODE_8(log_id, line, level, count, ...) \
    _WW_LOG_ENCODE_CALL_N(log_id, line, level, count, __VA_ARGS__)

/* ========== Public API Macros ========== */

/**
 * Public logging macros for encode mode
 *
 * IMPORTANT: The 'tag' and 'fmt' parameters are COMPLETELY IGNORED
 * in encode mode - they are NOT compiled into the binary!
 *
 * Usage:
 *   LOG_ERR(tag, "message")              -> outputs encoded log, 0 params
 *   LOG_INF(tag, "val=%d", 123)          -> outputs encoded log, 1 param
 *   LOG_DBG(tag, "x=%d y=%d", 10, 20)    -> outputs encoded log, 2 params
 *
 * Macro expansion is minimal:
 *   - Only creates param array (if needed) and calls function
 *   - All filtering is done inside function
 *   - fmt string is discarded (not in binary)
 */
#define LOG_ERR(tag, fmt, ...) \
    _WW_LOG_ENCODE_DISPATCH(CURRENT_LOG_ID, __LINE__, WW_LOG_LEVEL_ERR, \
                            _WW_LOG_ARG_COUNT(__VA_ARGS__), ##__VA_ARGS__)

#define LOG_WRN(tag, fmt, ...) \
    _WW_LOG_ENCODE_DISPATCH(CURRENT_LOG_ID, __LINE__, WW_LOG_LEVEL_WRN, \
                            _WW_LOG_ARG_COUNT(__VA_ARGS__), ##__VA_ARGS__)

#define LOG_INF(tag, fmt, ...) \
    _WW_LOG_ENCODE_DISPATCH(CURRENT_LOG_ID, __LINE__, WW_LOG_LEVEL_INF, \
                            _WW_LOG_ARG_COUNT(__VA_ARGS__), ##__VA_ARGS__)

#define LOG_DBG(tag, fmt, ...) \
    _WW_LOG_ENCODE_DISPATCH(CURRENT_LOG_ID, __LINE__, WW_LOG_LEVEL_DBG, \
                            _WW_LOG_ARG_COUNT(__VA_ARGS__), ##__VA_ARGS__)

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
