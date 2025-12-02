/**
 * @file ww_log_encode.h
 * @brief Encode mode logging implementation
 * @date 2025-11-29
 *
 * Encode mode features:
 * - Binary encoding for minimal code size
 * - No format strings stored in ROM
 * - Encoding format: 32-bit header + parameter values
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
 * Output format:
 *   0x08302732 0x00000050 0x000000AB
 *   ^header    ^param1   ^param2
 *
 * Usage in module internal header (e.g., brom_in.h):
 *   #define CURRENT_MODULE_BASE   WW_LOG_DIR_BROM_BASE
 *   #ifndef CURRENT_FILE_OFFSET
 *   #define CURRENT_FILE_OFFSET   0
 *   #endif
 *   #define CURRENT_LOG_ID        (CURRENT_MODULE_BASE + CURRENT_FILE_OFFSET)
 *
 * Usage in source file:
 *   // Normal file (uses default offset 0)
 *   #include "brom_in.h"
 *   LOG_INF();  // Encoded as: LOG_ID=160, LINE=__LINE__, LEVEL=2, PARAM_CNT=0
 *
 *   // File with custom offset and parameters
 *   #define CURRENT_FILE_OFFSET  BROM_FILE_FLASH  // Defined in brom_in.h
 *   #include "brom_in.h"
 *   LOG_DBG(tag, "val=%d", 123);  // Encoded as: LOG_ID=163, LINE=__LINE__, LEVEL=3, PARAM_CNT=1
 *                                  // Output: 0xA3xxxxxy 0x0000007B (where y=1)
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
 *
 * Example:
 *   LOG_ID = 161 (BROM_BASE + 1)
 *   module_id = 161 >> 5 = 5 (WW_LOG_MODULE_BROM)
 */
#define WW_LOG_GET_MODULE_ID(log_id)  ((log_id) >> 5)

/* ========== Encoding Macros ========== */

/**
 * Encode a log entry into 32-bit format
 * @param log_id Module/file identifier (12 bits, 0-4095)
 * @param line Line number (12 bits, 0-4095)
 * @param data_len Number of data parameters (6 bits, 0-63)
 * @param level Log level (2 bits, 0-3: ERR/WRN/INF/DBG)
 * @return 32-bit encoded value
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
 * @brief Core encode mode output function
 * @param encoded_log 32-bit encoded log entry (contains LOG_ID, LINE, LEVEL, PARAM_CNT)
 * @param params Array of parameter values (each as U32)
 * @param param_count Number of parameters (0-8)
 *
 * This function outputs the encoded log to the configured target:
 * - UART (real-time output): prints header + all params
 * - RAM buffer (circular buffer for history)
 * - External storage (optional)
 *
 * Output format: 0xHHHHHHHH 0xPPPPPPPP 0xPPPPPPPP ...
 */
void ww_log_encode_output(U32 encoded_log, const U32 *params, U8 param_count);

/* ========== Argument Counting for Variadic Macros ========== */

/**
 * Count number of variadic arguments
 * This counts ALL arguments passed to the macro
 * Supports 1-10 arguments
 */
#define _WW_LOG_NARG(...) \
    _WW_LOG_NARG_IMPL(__VA_ARGS__, 10,9,8,7,6,5,4,3,2,1)

#define _WW_LOG_NARG_IMPL(_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,N,...) N

/* ========== Internal Macro Implementations ========== */

/**
 * Internal encode macros with different parameter counts
 * Argument count includes all parameters passed to LOG_XXX macro
 *
 * Pattern (all macros require module_tag parameter):
 *   2 args: (module_tag, "msg") - module + message, 0 data params
 *   3 args: (module_tag, "msg", p1) - module + message + 1 data param
 *   4 args: (module_tag, "msg", p1, p2) - module + message + 2 data params
 *   N args: (module_tag, "msg", p1, ..., pN-2) - module + message + (N-2) data params
 *
 * Note: _LOG_ENCODE_1 is intentionally omitted because all LOG calls now
 *       require a module parameter (CURRENT_LOG_PARAM), so minimum is 2 args.
 */

/* 2 args: module + message, 0 data params */
#define _LOG_ENCODE_2(level, module_tag, fmt) \
    do { \
        U8 _module_id = WW_LOG_GET_MODULE_ID(CURRENT_LOG_ID); \
        if (WW_LOG_MODULE_ENABLED(_module_id) && (WW_LOG_LEVEL_THRESHOLD >= level)) { \
            (void)module_tag; \
            U32 _encoded = WW_LOG_ENCODE(CURRENT_LOG_ID, __LINE__, 0, level); \
            ww_log_encode_output(_encoded, (const U32*)0, 0); \
        } \
    } while(0)

/* 3 args: module + message + 1 data param */
#define _LOG_ENCODE_3(level, module_tag, fmt, p1) \
    do { \
        U8 _module_id = WW_LOG_GET_MODULE_ID(CURRENT_LOG_ID); \
        if (WW_LOG_MODULE_ENABLED(_module_id) && (WW_LOG_LEVEL_THRESHOLD >= level)) { \
            (void)module_tag; \
            U32 _params[] = {(U32)(p1)}; \
            U32 _encoded = WW_LOG_ENCODE(CURRENT_LOG_ID, __LINE__, 1, level); \
            ww_log_encode_output(_encoded, _params, 1); \
        } \
    } while(0)

/* 4 args: module + message + 2 data params */
#define _LOG_ENCODE_4(level, module_tag, fmt, p1, p2) \
    do { \
        U8 _module_id = WW_LOG_GET_MODULE_ID(CURRENT_LOG_ID); \
        if (WW_LOG_MODULE_ENABLED(_module_id) && (WW_LOG_LEVEL_THRESHOLD >= level)) { \
            (void)module_tag; \
            U32 _params[] = {(U32)(p1), (U32)(p2)}; \
            U32 _encoded = WW_LOG_ENCODE(CURRENT_LOG_ID, __LINE__, 2, level); \
            ww_log_encode_output(_encoded, _params, 2); \
        } \
    } while(0)

/* 5 args: module + message + 3 data params */
#define _LOG_ENCODE_5(level, module_tag, fmt, p1, p2, p3) \
    do { \
        U8 _module_id = WW_LOG_GET_MODULE_ID(CURRENT_LOG_ID); \
        if (WW_LOG_MODULE_ENABLED(_module_id) && (WW_LOG_LEVEL_THRESHOLD >= level)) { \
            (void)module_tag; \
            U32 _params[] = {(U32)(p1), (U32)(p2), (U32)(p3)}; \
            U32 _encoded = WW_LOG_ENCODE(CURRENT_LOG_ID, __LINE__, 3, level); \
            ww_log_encode_output(_encoded, _params, 3); \
        } \
    } while(0)

/* 6 args: module + message + 4 data params */
#define _LOG_ENCODE_6(level, module_tag, fmt, p1, p2, p3, p4) \
    do { \
        U8 _module_id = WW_LOG_GET_MODULE_ID(CURRENT_LOG_ID); \
        if (WW_LOG_MODULE_ENABLED(_module_id) && (WW_LOG_LEVEL_THRESHOLD >= level)) { \
            (void)module_tag; \
            U32 _params[] = {(U32)(p1), (U32)(p2), (U32)(p3), (U32)(p4)}; \
            U32 _encoded = WW_LOG_ENCODE(CURRENT_LOG_ID, __LINE__, 4, level); \
            ww_log_encode_output(_encoded, _params, 4); \
        } \
    } while(0)

/* 7 args: module + message + 5 data params */
#define _LOG_ENCODE_7(level, module_tag, fmt, p1, p2, p3, p4, p5) \
    do { \
        U8 _module_id = WW_LOG_GET_MODULE_ID(CURRENT_LOG_ID); \
        if (WW_LOG_MODULE_ENABLED(_module_id) && (WW_LOG_LEVEL_THRESHOLD >= level)) { \
            (void)module_tag; \
            U32 _params[] = {(U32)(p1), (U32)(p2), (U32)(p3), (U32)(p4), (U32)(p5)}; \
            U32 _encoded = WW_LOG_ENCODE(CURRENT_LOG_ID, __LINE__, 5, level); \
            ww_log_encode_output(_encoded, _params, 5); \
        } \
    } while(0)

/* 8 args: module + message + 6 data params */
#define _LOG_ENCODE_8(level, module_tag, fmt, p1, p2, p3, p4, p5, p6) \
    do { \
        U8 _module_id = WW_LOG_GET_MODULE_ID(CURRENT_LOG_ID); \
        if (WW_LOG_MODULE_ENABLED(_module_id) && (WW_LOG_LEVEL_THRESHOLD >= level)) { \
            (void)module_tag; \
            U32 _params[] = {(U32)(p1), (U32)(p2), (U32)(p3), (U32)(p4), (U32)(p5), (U32)(p6)}; \
            U32 _encoded = WW_LOG_ENCODE(CURRENT_LOG_ID, __LINE__, 6, level); \
            ww_log_encode_output(_encoded, _params, 6); \
        } \
    } while(0)

/* 9 args: module + message + 7 data params */
#define _LOG_ENCODE_9(level, module_tag, fmt, p1, p2, p3, p4, p5, p6, p7) \
    do { \
        U8 _module_id = WW_LOG_GET_MODULE_ID(CURRENT_LOG_ID); \
        if (WW_LOG_MODULE_ENABLED(_module_id) && (WW_LOG_LEVEL_THRESHOLD >= level)) { \
            (void)module_tag; \
            U32 _params[] = {(U32)(p1), (U32)(p2), (U32)(p3), (U32)(p4), (U32)(p5), (U32)(p6), (U32)(p7)}; \
            U32 _encoded = WW_LOG_ENCODE(CURRENT_LOG_ID, __LINE__, 7, level); \
            ww_log_encode_output(_encoded, _params, 7); \
        } \
    } while(0)

/* 10 args: module + message + 8 data params (max supported) */
#define _LOG_ENCODE_10(level, module_tag, fmt, p1, p2, p3, p4, p5, p6, p7, p8) \
    do { \
        U8 _module_id = WW_LOG_GET_MODULE_ID(CURRENT_LOG_ID); \
        if (WW_LOG_MODULE_ENABLED(_module_id) && (WW_LOG_LEVEL_THRESHOLD >= level)) { \
            (void)module_tag; \
            U32 _params[] = {(U32)(p1), (U32)(p2), (U32)(p3), (U32)(p4), (U32)(p5), (U32)(p6), (U32)(p7), (U32)(p8)}; \
            U32 _encoded = WW_LOG_ENCODE(CURRENT_LOG_ID, __LINE__, 8, level); \
            ww_log_encode_output(_encoded, _params, 8); \
        } \
    } while(0)

/**
 * Macro concatenation helpers (need double expansion for correct behavior)
 */
#define _WW_LOG_CONCAT(a, b) _WW_LOG_CONCAT_IMPL(a, b)
#define _WW_LOG_CONCAT_IMPL(a, b) a##b

/**
 * Dispatcher macro to select correct _LOG_ENCODE_N based on argument count
 */
#define _LOG_ENCODE_DISPATCH(level, ...) \
    _WW_LOG_CONCAT(_LOG_ENCODE_, _WW_LOG_NARG(__VA_ARGS__))(level, __VA_ARGS__)

/* ========== Public API Macros ========== */

/**
 * Public logging macros for encode mode
 * These macros count the number of parameters and dispatch to appropriate handler
 *
 * Usage:
 *   LOG_ERR(tag, "message")              -> 0 params
 *   LOG_INF(tag, "val=%d", 123)          -> 1 param (123)
 *   LOG_DBG(tag, "x=%d y=%d", 10, 20)    -> 2 params (10, 20)
 */
#define LOG_ERR(...) _LOG_ENCODE_DISPATCH(WW_LOG_LEVEL_ERR, __VA_ARGS__)
#define LOG_WRN(...) _LOG_ENCODE_DISPATCH(WW_LOG_LEVEL_WRN, __VA_ARGS__)
#define LOG_INF(...) _LOG_ENCODE_DISPATCH(WW_LOG_LEVEL_INF, __VA_ARGS__)
#define LOG_DBG(...) _LOG_ENCODE_DISPATCH(WW_LOG_LEVEL_DBG, __VA_ARGS__)

/* ========== RAM Buffer (Optional) ========== */

/**
 * RAM buffer configuration
 * Enable this to store encoded logs in a circular buffer
 */
#ifdef WW_LOG_ENCODE_RAM_BUFFER_EN

#ifndef WW_LOG_RAM_BUFFER_SIZE
#define WW_LOG_RAM_BUFFER_SIZE  128  /* Number of 32-bit entries */
#endif

/**
 * Circular buffer structure
 */
typedef struct {
    U32 magic;                              /* Magic number for warm restart detection */
    U16 head;                               /* Read pointer */
    U16 tail;                               /* Write pointer */
    U32 entries[WW_LOG_RAM_BUFFER_SIZE];   /* Log data */
} WW_LOG_RAM_BUFFER_T;

/**
 * Magic number for warm restart detection
 * ASCII: "WLOG" = 0x574C4F47
 */
#define WW_LOG_RAM_MAGIC  0x574C4F47

/**
 * Global RAM buffer instance
 */
extern WW_LOG_RAM_BUFFER_T g_ww_log_ram_buffer;

/**
 * RAM buffer API
 */
U16 ww_log_ram_get_count(void);
void ww_log_ram_dump(void);
void ww_log_ram_clear(void);

#endif /* WW_LOG_ENCODE_RAM_BUFFER_EN */

#endif /* WW_LOG_ENCODE_H */
