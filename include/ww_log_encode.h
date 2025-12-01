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
 * ┌─────────────┬─────────────┬─────────┬────────────┐
 * │ LOG_ID      │ LINE        │ LEVEL   │ PARAM_CNT  │
 * │ (12 bits)   │ (12 bits)   │ (4 bits)│ (4 bits)   │
 * │ 31       20 │ 19        8 │ 7     4 │ 3       0  │
 * └─────────────┴─────────────┴─────────┴────────────┘
 *
 * Where:
 * - LOG_ID: Module base ID + file offset (from CURRENT_LOG_ID)
 * - LINE: Source line number (__LINE__)
 * - LEVEL: Log level (ERR=0, WRN=1, INF=2, DBG=3)
 * - PARAM_CNT: Number of parameters (0-15, max 8 supported)
 *
 * Output format:
 *   0x08302732 0x00000050 0x000000AB
 *   ^header    ^param1   ^param2
 *
 * Usage in module internal header (e.g., brom_in.h):
 *   #define CURRENT_MODULE_BASE   WW_LOG_MOD_BROM_BASE
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

/* ========== Encoding Macros ========== */

/**
 * Encode a log entry into 32-bit format
 * @param log_id Module/file identifier (12 bits, 0-4095)
 * @param line Line number (12 bits, 0-4095)
 * @param level Log level (4 bits, 0-15)
 * @param param_cnt Number of parameters (4 bits, 0-15)
 * @return 32-bit encoded value
 */
#define WW_LOG_ENCODE(log_id, line, level, param_cnt) \
    ((U32)( \
        (((U32)(log_id) & 0xFFF) << 20) | \
        (((U32)(line) & 0xFFF) << 8) | \
        (((U32)(level) & 0xF) << 4) | \
        ((U32)(param_cnt) & 0xF) \
    ))

/**
 * Decode macros to extract fields from encoded log
 */
#define WW_LOG_DECODE_LOG_ID(encoded)      (((encoded) >> 20) & 0xFFF)
#define WW_LOG_DECODE_LINE(encoded)        (((encoded) >> 8) & 0xFFF)
#define WW_LOG_DECODE_LEVEL(encoded)       (((encoded) >> 4) & 0xF)
#define WW_LOG_DECODE_PARAM_CNT(encoded)   ((encoded) & 0xF)

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
 * Pattern:
 *   1 arg: ("msg") - message only, 0 data params
 *   2 args: ("[TAG]", "msg") - module + message, 0 data params
 *   3 args: ("[TAG]", "msg", p1) - module + message + 1 data param
 *   N args: ("[TAG]", "msg", p1, ..., pN-2) - module + message + (N-2) data params
 */

/* 1 arg: message only, use default tag, 0 data params */
#define _LOG_ENCODE_1(level, fmt) \
    do { \
        if (WW_LOG_LEVEL_THRESHOLD >= level) { \
            U32 _encoded = WW_LOG_ENCODE(CURRENT_LOG_ID, __LINE__, level, 0); \
            ww_log_encode_output(_encoded, (const U32*)0, 0); \
        } \
    } while(0)

/* 2 args: module + message, 0 data params */
#define _LOG_ENCODE_2(level, module_tag, fmt) \
    do { \
        if (WW_LOG_LEVEL_THRESHOLD >= level) { \
            (void)module_tag; \
            U32 _encoded = WW_LOG_ENCODE(CURRENT_LOG_ID, __LINE__, level, 0); \
            ww_log_encode_output(_encoded, (const U32*)0, 0); \
        } \
    } while(0)

/* 3 args: module + message + 1 data param */
#define _LOG_ENCODE_3(level, module_tag, fmt, p1) \
    do { \
        if (WW_LOG_LEVEL_THRESHOLD >= level) { \
            (void)module_tag; \
            U32 _params[] = {(U32)(p1)}; \
            U32 _encoded = WW_LOG_ENCODE(CURRENT_LOG_ID, __LINE__, level, 1); \
            ww_log_encode_output(_encoded, _params, 1); \
        } \
    } while(0)

/* 4 args: module + message + 2 data params */
#define _LOG_ENCODE_4(level, module_tag, fmt, p1, p2) \
    do { \
        if (WW_LOG_LEVEL_THRESHOLD >= level) { \
            (void)module_tag; \
            U32 _params[] = {(U32)(p1), (U32)(p2)}; \
            U32 _encoded = WW_LOG_ENCODE(CURRENT_LOG_ID, __LINE__, level, 2); \
            ww_log_encode_output(_encoded, _params, 2); \
        } \
    } while(0)

/* 5 args: module + message + 3 data params */
#define _LOG_ENCODE_5(level, module_tag, fmt, p1, p2, p3) \
    do { \
        if (WW_LOG_LEVEL_THRESHOLD >= level) { \
            (void)module_tag; \
            U32 _params[] = {(U32)(p1), (U32)(p2), (U32)(p3)}; \
            U32 _encoded = WW_LOG_ENCODE(CURRENT_LOG_ID, __LINE__, level, 3); \
            ww_log_encode_output(_encoded, _params, 3); \
        } \
    } while(0)

/* 6 args: module + message + 4 data params */
#define _LOG_ENCODE_6(level, module_tag, fmt, p1, p2, p3, p4) \
    do { \
        if (WW_LOG_LEVEL_THRESHOLD >= level) { \
            (void)module_tag; \
            U32 _params[] = {(U32)(p1), (U32)(p2), (U32)(p3), (U32)(p4)}; \
            U32 _encoded = WW_LOG_ENCODE(CURRENT_LOG_ID, __LINE__, level, 4); \
            ww_log_encode_output(_encoded, _params, 4); \
        } \
    } while(0)

/* 7 args: module + message + 5 data params */
#define _LOG_ENCODE_7(level, module_tag, fmt, p1, p2, p3, p4, p5) \
    do { \
        if (WW_LOG_LEVEL_THRESHOLD >= level) { \
            (void)module_tag; \
            U32 _params[] = {(U32)(p1), (U32)(p2), (U32)(p3), (U32)(p4), (U32)(p5)}; \
            U32 _encoded = WW_LOG_ENCODE(CURRENT_LOG_ID, __LINE__, level, 5); \
            ww_log_encode_output(_encoded, _params, 5); \
        } \
    } while(0)

/* 8 args: module + message + 6 data params */
#define _LOG_ENCODE_8(level, module_tag, fmt, p1, p2, p3, p4, p5, p6) \
    do { \
        if (WW_LOG_LEVEL_THRESHOLD >= level) { \
            (void)module_tag; \
            U32 _params[] = {(U32)(p1), (U32)(p2), (U32)(p3), (U32)(p4), (U32)(p5), (U32)(p6)}; \
            U32 _encoded = WW_LOG_ENCODE(CURRENT_LOG_ID, __LINE__, level, 6); \
            ww_log_encode_output(_encoded, _params, 6); \
        } \
    } while(0)

/* 9 args: module + message + 7 data params */
#define _LOG_ENCODE_9(level, module_tag, fmt, p1, p2, p3, p4, p5, p6, p7) \
    do { \
        if (WW_LOG_LEVEL_THRESHOLD >= level) { \
            (void)module_tag; \
            U32 _params[] = {(U32)(p1), (U32)(p2), (U32)(p3), (U32)(p4), (U32)(p5), (U32)(p6), (U32)(p7)}; \
            U32 _encoded = WW_LOG_ENCODE(CURRENT_LOG_ID, __LINE__, level, 7); \
            ww_log_encode_output(_encoded, _params, 7); \
        } \
    } while(0)

/* 10 args: module + message + 8 data params (max supported) */
#define _LOG_ENCODE_10(level, module_tag, fmt, p1, p2, p3, p4, p5, p6, p7, p8) \
    do { \
        if (WW_LOG_LEVEL_THRESHOLD >= level) { \
            (void)module_tag; \
            U32 _params[] = {(U32)(p1), (U32)(p2), (U32)(p3), (U32)(p4), (U32)(p5), (U32)(p6), (U32)(p7), (U32)(p8)}; \
            U32 _encoded = WW_LOG_ENCODE(CURRENT_LOG_ID, __LINE__, level, 8); \
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
