/* HERE_IS_TO_BE_REPLACED_BY_FILE_HEADER */

/*************************** description start ***************************/
/* ww_log shared definitions: log levels, compile-time thresholds, the encode
 * bit-field layout and per-file injected macros. This is the LOWEST layer of
 * the log module -- it depends on nothing but ww_type.h and is included by all
 * the other log headers, which is what keeps the include graph acyclic
 * (def <- output <- macro, def <- api; no header includes its dependents). */
/*************************** description end *****************************/

#ifndef __N_WW_LOG_DEF_H__
#define __N_WW_LOG_DEF_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include "ww_type.h"

/*************************** macro definition start ***************************/

/* ========= Log levels ========= */
#define N_WW_LOG_LEVEL_ERR    0  /* Error: failures, critical issues */
#define N_WW_LOG_LEVEL_WRN    1  /* Warning: potential problems */
#define N_WW_LOG_LEVEL_INF    2  /* Info: important state changes */
#define N_WW_LOG_LEVEL_DBG    3  /* Debug: detailed execution flow */

/**
 * Compile-time level threshold. Logs with level > threshold are compiled out
 * entirely (zero code size). Override via -DN_WW_LOG_COMPILE_THRESHOLD=n.
 */
#ifndef N_WW_LOG_COMPILE_THRESHOLD
#define N_WW_LOG_COMPILE_THRESHOLD    N_WW_LOG_LEVEL_DBG
#endif

/**
 * Storage-persist threshold (compile-time). Entries with level > this are NOT
 * written to the RAM ring / external storage; they still go to UART. level is
 * not encoded, so the filter is applied at emit time and RAM == storage content.
 *   = N_WW_LOG_LEVEL_INF : persist ERR/WRN/INF, drop DBG (default)
 *   = N_WW_LOG_LEVEL_DBG : persist everything
 */
#ifndef N_WW_LOG_STORAGE_THRESHOLD
#define N_WW_LOG_STORAGE_THRESHOLD    N_WW_LOG_LEVEL_INF
#endif

/* Number of runtime-maskable modules (g_ww_log_module_mask is a U32). */
#define N_WW_LOG_MODULE_MAX    32

/* ========= Per-file injected macros (defaults if not injected by build) =========
 * The Makefile injects CURRENT_FILE_ID / CURRENT_MODULE_ID /
 * CURRENT_MODULE_STATIC_EN per source file from ww_log_map.json. */
#ifndef CURRENT_FILE_ID
#define CURRENT_FILE_ID        0
#endif

#ifndef CURRENT_MODULE_ID
#define CURRENT_MODULE_ID      0
#endif

#ifndef CURRENT_MODULE_STATIC_EN
#define CURRENT_MODULE_STATIC_EN    0    /* unregistered file -> logs off */
#endif

/* ========= Encode bit-field layout (CLAUDE.md §2) =========
 *   31             20 19         6 5      0
 *   +-----------------+------------+--------+
 *   |   file_id (12)  |  line (14) |param_cnt|
 *   +-----------------+------------+--------+
 *                       file_id = [ module_id : 5 ][ offset : 7 ]
 *
 * The pack macro and the accessors are mode-independent: the storage layer
 * parses entries (param count, file_id) to walk the ring / validate data even
 * in STRING builds. */
#define N_WW_LOG_ENCODE(file_id, line, pcnt) \
    ( (((U32)(file_id)  & 0xFFF)  << 20) | \
      (((U32)(line)     & 0x3FFF) << 6)  | \
      (((U32)(pcnt)     & 0x3F)) )

#define N_WW_LOG_FILEID_OF(encoded)    (((encoded) >> 20) & 0xFFF)
#define N_WW_LOG_LINE_OF(encoded)      (((encoded) >> 6)  & 0x3FFF)
#define N_WW_LOG_PCNT_OF(encoded)      ((encoded) & 0x3F)
#define N_WW_LOG_MODULE_OF(file_id)    (((file_id) >> 7) & 0x1F)
#define N_WW_LOG_OFFSET_OF(file_id)    ((file_id) & 0x7F)

#define N_WW_LOG_ENCODE_MAX_PARAMS     16

/*************************** macro definition end *****************************/

#ifdef __cplusplus
}
#endif

#endif /* __N_WW_LOG_DEF_H__ */
