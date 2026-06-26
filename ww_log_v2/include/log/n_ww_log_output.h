/* HERE_IS_TO_BE_REPLACED_BY_FILE_HEADER */

/*************************** description start ***************************/
/* ww_log output backend: declarations of the functions the N_LOG_* call macros
 * expand into (string-mode printer, encode-mode emitter) and the backend fan-out
 * (UART / RAM / external storage). Implemented in n_ww_logoutput.c. */
/*************************** description end *****************************/

#ifndef __N_WW_LOG_OUTPUT_H__
#define __N_WW_LOG_OUTPUT_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include "n_ww_log_def.h"

/*************************** declaration start ***************************/

#if defined(CONFIG_N_LOG_MODE_STRING)
/* String mode: format + print directly (filename/line/level + fmt). */
void n_ww_log_str_output(U8 module_id, const char *filename, U32 line, U8 level,
                         const char *fmt, ...);
#endif

#if defined(CONFIG_N_LOG_MODE_ENCODE)
/* Encode mode: pack the entry header + U32 params and hand to the backends. */
void n_ww_log_encode_output(U16 file_id, U16 line, U8 level, U8 param_count, ...);
#endif

/**
 * @brief Emit one encoded entry to all enabled backends.
 * @param encoded     32-bit entry header (see N_WW_LOG_ENCODE)
 * @param params      array of param_count U32 values (may be NULL if 0)
 * @param param_count number of parameters
 * @param level       log level (N_WW_LOG_LEVEL_*); used by the RAM/storage
 *                    backend for the storage-persist threshold and ERR flag.
 *                    UART ignores it (already filtered upstream).
 */
void ww_log_backend_emit(U32 encoded, const U32 *params, U8 param_count, U8 level);

/*************************** declaration end *****************************/

#ifdef __cplusplus
}
#endif

#endif /* __N_WW_LOG_OUTPUT_H__ */
