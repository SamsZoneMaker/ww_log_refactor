/* HERE_IS_TO_BE_REPLACED_BY_FILE_HEADER */

/*************************** description start ***************************/
/* to add description for this file if needed */
/*************************** description end *****************************/

#ifndef __N_WW_LOG_OUTPUT_H__
#define __N_WW_LOG_OUTPUT_H__

#ifdef __cplusplus
extern "C"
{
#endif

// #include <>
// #include ""
#include "ww_type.h"
#include "autoconf.h"
#include "n_ww_log_control.h"

/*************************** macro definition start ***************************/
/* ==================== Per-file injected macros (defaults if not injected by build) ==================== */

#ifndef CURRENT_FILE_ID
#define CURRENT_FILE_ID        0
#endif

#ifndef CURRENT_MODULE_ID
#define CURRENT_MODULE_ID      0
#endif

#ifndef CURRENT_MODULE_STATIC_EN
#define CURRENT_MODULE_STATIC_EN    0    /* unregistered file -> logs off */
#endif

/*************************** macro definition end *****************************/


#if defined(CONFIG_N_LOG_MODE_STRING)

#ifndef _NOTDIR_FILE_
#define __WW_LOG_FILENAME__(path)    __NOTDIR_FILE__
#else
#define __WW_LOG_FILENAME__(path) \
    (strrchr(path, '/') ? strrchr(path, '/') + 1 : \
    (strrchr(path, '\\') ? strrchr(path, '\\') + 1 : path))
#endif

void n_ww_log_str_output(U8 module_id, const char *filename, U32 line, U8 level,
                         const char *fmt, ...);

/* Static-switch conditional expansion */
#define __WW_LOG_STR_IF_0(...)    do { } while (0)
#define __WW_LOG_STR_IF_1(...)    __VA_ARGS__
#define __WW_LOG_STR_CAT(a, b)    __WW_LOG_STR_CAT_IMPL(a, b)
#define __WW_LOG_STR_CAT_IMPL(a, b)    a##b
#define __WW_LOG_STR_IF(cond)     __WW_LOG_STR_CAT(__WW_LOG_STR_IF_, cond)

#define __WW_LOG_STR_CALL(level, fmt, ...) \
    n_ww_log_str_output(CURRENT_MODULE_ID, __WW_LOG_FILENAME__(__FILE__), __LINE__, \
                        level, fmt, ##__VA_ARGS__)

#define __WW_LOG_STR_STATIC_EXPAND(level, fmt, ...) \
    __WW_LOG_STR_IF(CURRENT_MODULE_STATIC_EN)(__WW_LOG_STR_CALL(level, fmt, ##__VA_ARGS__))

#if (N_WW_LOG_COMPILE_THRESHOLD >= N_WW_LOG_LEVEL_ERR)
#define N_LOG_ERR(fmt, ...)    __WW_LOG_STR_STATIC_EXPAND(N_WW_LOG_LEVEL_ERR, fmt, ##__VA_ARGS__)
#else
#define N_LOG_ERR(fmt, ...)    do { } while (0)
#endif

#if (N_WW_LOG_COMPILE_THRESHOLD >= N_WW_LOG_LEVEL_WRN)
#define N_LOG_WRN(fmt, ...)    __WW_LOG_STR_STATIC_EXPAND(N_WW_LOG_LEVEL_WRN, fmt, ##__VA_ARGS__)
#else
#define N_LOG_WRN(fmt, ...)    do { } while (0)
#endif

#if (N_WW_LOG_COMPILE_THRESHOLD >= N_WW_LOG_LEVEL_INF)
#define N_LOG_INF(fmt, ...)    __WW_LOG_STR_STATIC_EXPAND(N_WW_LOG_LEVEL_INF, fmt, ##__VA_ARGS__)
#else
#define N_LOG_INF(fmt, ...)    do { } while (0)
#endif

#if (N_WW_LOG_COMPILE_THRESHOLD >= N_WW_LOG_LEVEL_DBG)
#define N_LOG_DBG(fmt, ...)    __WW_LOG_STR_STATIC_EXPAND(N_WW_LOG_LEVEL_DBG, fmt, ##__VA_ARGS__)
#else
#define N_LOG_DBG(fmt, ...)    do { } while (0)
#endif


#elif defined(CONFIG_N_LOG_MODE_ENCODE)

/* Entry header layout (32 bits), see CLAUDE.md §2:
 * 31             20 19         6 5      0
 * +-----------------+------------+--------+
 * |   file_id (12)  |  line (14) |param_cnt|
 * +-----------------+------------+--------+
 * |
 * +-- file_id = [ module_id : 5 ][ offset : 7 ]
 */

#define N_WW_LOG_ENCODE(file_id, line, pcnt) \
    ( (((U32)(file_id)  & 0xFFF)  << 20) | \
      (((U32)(line)     & 0x3FFF) << 6)  | \
      (((U32)(pcnt)     & 0x3F)) )

#define N_WW_LOG_FILEID_OF(encoded)    (((encoded) >> 20) & 0xFFF)
#define N_WW_LOG_LINE_OF(encoded)      (((encoded) >> 6)  & 0x3FFF)
#define N_WW_LOG_PCNT_OF(encoded)      ((encoded) & 0x3F)

#define N_N_WW_LOG_MODULE_OF(file_id)    (((file_id) >> 7) & 0x1F)
#define N_WW_LOG_OFFSET_OF(file_id)    ((file_id) & 0x7F)

#define N_WW_LOG_ENCODE_MAX_PARAMS     16

void n_ww_log_encode_output(U16 file_id, U16 line, U8 level, U8 param_count, ...);

/* Variadic argument counter (0-16) */
#define __WW_LOG_ARG_COUNT(...) \
    __WW_LOG_ARG_COUNT_IMPL(0, ##__VA_ARGS__, \
        16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0)
#define __WW_LOG_ARG_COUNT_IMPL( \
    _0,_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,_12,_13,_14,_15,_16,N, ... ) N

/* Static-switch conditional expansion */
#define __WW_LOG_IF_0(...)
#define __WW_LOG_IF_1(...)        __VA_ARGS__
#define __WW_LOG_CAT(a, b)        __WW_LOG_CAT_IMPL(a, b)
#define __WW_LOG_CAT_IMPL(a, b)   a##b
#define __WW_LOG_IF(cond)         __WW_LOG_CAT(__WW_LOG_IF_, cond)

#define __WW_LOG_ENCODE_CALL(level, fmt, ...) \
    n_ww_log_encode_output(CURRENT_FILE_ID, __LINE__, level, \
                           __WW_LOG_ARG_COUNT(__VA_ARGS__), ##__VA_ARGS__)

#define __WW_LOG_STATIC_EXPAND(level, fmt, ...) \
    __WW_LOG_IF(CURRENT_MODULE_STATIC_EN)(__WW_LOG_ENCODE_CALL(level, fmt, ##__VA_ARGS__))

#if (N_WW_LOG_COMPILE_THRESHOLD >= N_WW_LOG_LEVEL_ERR)
#define N_LOG_ERR(fmt, ...)    __WW_LOG_STATIC_EXPAND(N_WW_LOG_LEVEL_ERR, fmt, ##__VA_ARGS__)
#else
#define N_LOG_ERR(fmt, ...)    do { } while (0)
#endif

#if (N_WW_LOG_COMPILE_THRESHOLD >= N_WW_LOG_LEVEL_WRN)
#define N_LOG_WRN(fmt, ...)    __WW_LOG_STATIC_EXPAND(N_WW_LOG_LEVEL_WRN, fmt, ##__VA_ARGS__)
#else
#define N_LOG_WRN(fmt, ...)    do { } while (0)
#endif

#if (N_WW_LOG_COMPILE_THRESHOLD >= N_WW_LOG_LEVEL_INF)
#define N_LOG_INF(fmt, ...)    __WW_LOG_STATIC_EXPAND(N_WW_LOG_LEVEL_INF, fmt, ##__VA_ARGS__)
#else
#define N_LOG_INF(fmt, ...)    do { } while (0)
#endif

#if (N_WW_LOG_COMPILE_THRESHOLD >= N_WW_LOG_LEVEL_DBG)
#define N_LOG_DBG(fmt, ...)    __WW_LOG_STATIC_EXPAND(N_WW_LOG_LEVEL_DBG, fmt, ##__VA_ARGS__)
#else
#define N_LOG_DBG(fmt, ...)    do { } while (0)
#endif

#endif /* mode selection */


/**
 * @brief Emit one encoded entry to all enabled backends.
 * @param encoded     32-bit entry header (see WW_LOG_ENCODE)
 * @param params      array of param_count U32 values (may be NULL if 0)
 * @param param_count number of parameters
 * @param sync        1 = bypass buffering / flush immediately (panic path)
 */
void ww_log_backend_emit(U32 encoded, const U32 *params, U8 param_count);

#ifdef __cplusplus
}
#endif

#endif /* __N_WW_LOG_OUTPUT_H__ */
