/**
 * @file ww_log_output.h
 * @brief Log output: encoding macros, string formatting, and backend dispatch.
 *
 * Whichever mode is active, this header exposes the four LOG_xxx() macros and
 * the corresponding runtime output function. ww_log_backend_emit() is declared
 * unconditionally — it is the single fan-out point for all enabled backends in
 * encode mode.
 */

#ifndef WW_LOG_OUTPUT_H
#define WW_LOG_OUTPUT_H

#include "type.h"
#include "ww_log_ctrl.h"   /* g_ww_log_module_mask, g_ww_log_level_threshold */

/* ========== Backend dispatch (used in encode mode) ========== */

/**
 * @brief Emit one encoded entry to all enabled backends.
 * @param encoded     32-bit entry header (see WW_LOG_ENCODE)
 * @param params      array of param_count U32 values (may be NULL if 0)
 * @param param_count number of parameters
 */
void ww_log_backend_emit(U32 encoded, const U32 *params, U8 param_count);

/* ========== Per-file injected macros (defaults if not injected by build) ========== */

#ifndef CURRENT_FILE_ID
#define CURRENT_FILE_ID  0
#endif
#ifndef CURRENT_MODULE_ID
#define CURRENT_MODULE_ID  0
#endif
#ifndef CURRENT_MODULE_STATIC_EN
#define CURRENT_MODULE_STATIC_EN  0   /* unregistered file -> logs off */
#endif

/* ============================================================
 * ENCODE mode
 * ============================================================ */
#if defined(WW_LOG_MODE_ENCODE)

/* Entry header layout (32 bits), see CLAUDE.md §2:
 *  31                20 19              6 5         0
 * +--------------------+------------------+-----------+
 * |   file_id (12)     |    line (14)     | param_cnt |
 * +--------------------+------------------+-----------+
 *         |
 *         +-- file_id = [ module_id : 5 ][ offset : 7 ]
 */

#define WW_LOG_ENCODE(file_id, line, pcnt) \
    ( (((U32)(file_id) & 0xFFF) << 20) | \
      (((U32)(line)    & 0x3FFF) << 6) | \
      ( (U32)(pcnt)    & 0x3F) )

#define WW_LOG_FILEID_OF(encoded)  (((encoded) >> 20) & 0xFFF)
#define WW_LOG_LINE_OF(encoded)    (((encoded) >> 6)  & 0x3FFF)
#define WW_LOG_PCNT_OF(encoded)    ((encoded) & 0x3F)

#define N_WW_LOG_MODULE_OF(file_id)  (((file_id) >> 7) & 0x1F)
#define WW_LOG_OFFSET_OF(file_id)  ((file_id) & 0x7F)

void ww_log_encode_output(U16 file_id, U16 line, U8 level, U8 param_count, ...);

/* Variadic argument counter (0-16) */
#define _WW_LOG_ARG_COUNT(...) \
    _WW_LOG_ARG_COUNT_IMPL(0, ##__VA_ARGS__, \
        16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0)
#define _WW_LOG_ARG_COUNT_IMPL( \
    _0,_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,_12,_13,_14,_15,_16,N,...) N

/* Static-switch conditional expansion */
#define _WW_LOG_IF_0(...)
#define _WW_LOG_IF_1(...)  __VA_ARGS__
#define _WW_LOG_CAT(a, b)       _WW_LOG_CAT_IMPL(a, b)
#define _WW_LOG_CAT_IMPL(a, b)  a##b
#define _WW_LOG_IF(cond)  _WW_LOG_CAT(_WW_LOG_IF_, cond)

#define _WW_LOG_ENCODE_CALL(level, fmt, ...) \
    ww_log_encode_output(CURRENT_FILE_ID, __LINE__, level, \
                         _WW_LOG_ARG_COUNT(__VA_ARGS__), ##__VA_ARGS__)

#define _WW_LOG_STATIC_EXPAND(level, fmt, ...) \
    _WW_LOG_IF(CURRENT_MODULE_STATIC_EN)(_WW_LOG_ENCODE_CALL(level, fmt, ##__VA_ARGS__))

#if (WW_LOG_COMPILE_THRESHOLD >= WW_LOG_LEVEL_ERR)
    #define LOG_ERR(fmt, ...)  _WW_LOG_STATIC_EXPAND(WW_LOG_LEVEL_ERR, fmt, ##__VA_ARGS__)
#else
    #define LOG_ERR(fmt, ...)  do { } while (0)
#endif
#if (WW_LOG_COMPILE_THRESHOLD >= WW_LOG_LEVEL_WRN)
    #define LOG_WRN(fmt, ...)  _WW_LOG_STATIC_EXPAND(WW_LOG_LEVEL_WRN, fmt, ##__VA_ARGS__)
#else
    #define LOG_WRN(fmt, ...)  do { } while (0)
#endif
#if (WW_LOG_COMPILE_THRESHOLD >= WW_LOG_LEVEL_INF)
    #define LOG_INF(fmt, ...)  _WW_LOG_STATIC_EXPAND(WW_LOG_LEVEL_INF, fmt, ##__VA_ARGS__)
#else
    #define LOG_INF(fmt, ...)  do { } while (0)
#endif
#if (WW_LOG_COMPILE_THRESHOLD >= WW_LOG_LEVEL_DBG)
    #define LOG_DBG(fmt, ...)  _WW_LOG_STATIC_EXPAND(WW_LOG_LEVEL_DBG, fmt, ##__VA_ARGS__)
#else
    #define LOG_DBG(fmt, ...)  do { } while (0)
#endif

/* ============================================================
 * STRING mode
 * ============================================================ */
#elif defined(WW_LOG_MODE_STR)

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#ifdef __NOTDIR_FILE__
    #define _WW_LOG_FILENAME(path) __NOTDIR_FILE__
#else
    #define _WW_LOG_FILENAME(path) \
        (strrchr(path, '/') ? strrchr(path, '/') + 1 : \
         (strrchr(path, '\\') ? strrchr(path, '\\') + 1 : path))
#endif

void ww_log_str_output(U8 module_id, const char *filename, U32 line, U8 level,
                       const char *fmt, ...);

/* Static-switch conditional expansion */
#define _WW_LOG_STR_IF_0(...)  do {} while (0)
#define _WW_LOG_STR_IF_1(...)  __VA_ARGS__
#define _WW_LOG_STR_CAT(a, b)       _WW_LOG_STR_CAT_IMPL(a, b)
#define _WW_LOG_STR_CAT_IMPL(a, b)  a##b
#define _WW_LOG_STR_IF(cond)  _WW_LOG_STR_CAT(_WW_LOG_STR_IF_, cond)

#define _WW_LOG_STR_CALL(level, fmt, ...) \
    ww_log_str_output(CURRENT_MODULE_ID, _WW_LOG_FILENAME(__FILE__), __LINE__, \
                      level, fmt, ##__VA_ARGS__)

#define _WW_LOG_STR_STATIC_EXPAND(level, fmt, ...) \
    _WW_LOG_STR_IF(CURRENT_MODULE_STATIC_EN)(_WW_LOG_STR_CALL(level, fmt, ##__VA_ARGS__))

#if (WW_LOG_COMPILE_THRESHOLD >= WW_LOG_LEVEL_ERR)
    #define LOG_ERR(fmt, ...)  _WW_LOG_STR_STATIC_EXPAND(WW_LOG_LEVEL_ERR, fmt, ##__VA_ARGS__)
#else
    #define LOG_ERR(fmt, ...)  do {} while (0)
#endif
#if (WW_LOG_COMPILE_THRESHOLD >= WW_LOG_LEVEL_WRN)
    #define LOG_WRN(fmt, ...)  _WW_LOG_STR_STATIC_EXPAND(WW_LOG_LEVEL_WRN, fmt, ##__VA_ARGS__)
#else
    #define LOG_WRN(fmt, ...)  do {} while (0)
#endif
#if (WW_LOG_COMPILE_THRESHOLD >= WW_LOG_LEVEL_INF)
    #define LOG_INF(fmt, ...)  _WW_LOG_STR_STATIC_EXPAND(WW_LOG_LEVEL_INF, fmt, ##__VA_ARGS__)
#else
    #define LOG_INF(fmt, ...)  do {} while (0)
#endif
#if (WW_LOG_COMPILE_THRESHOLD >= WW_LOG_LEVEL_DBG)
    #define LOG_DBG(fmt, ...)  _WW_LOG_STR_STATIC_EXPAND(WW_LOG_LEVEL_DBG, fmt, ##__VA_ARGS__)
#else
    #define LOG_DBG(fmt, ...)  do {} while (0)
#endif

#endif /* mode selection */

#endif /* WW_LOG_OUTPUT_H */
