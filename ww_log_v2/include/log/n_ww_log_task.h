/* HERE_IS_TO_BE_REPLACED_BY_FILE_HEADER */

/*************************** description start ***************************/
/* to add description for this file if needed */
/*************************** description end *****************************/

#ifndef __N_WW_LOG_TASK_H__
#define __N_WW_LOG_TASK_H__

#ifdef __cplusplus
extern "C"
{
#endif

// #include <>
// #include ""
#include "ww_type.h"

#ifdef CONFIG_N_LOG_BACKEND_RAM

/*************************** macro definition start ***************************/
/*************************** macro definition end *****************************/

/*************************** type definition start ***************************/
/*************************** type definition end *****************************/


/*************************** declaration start ***************************/
#ifdef CONFIG_N_LOG_BACKEND_EXT_MEM
/**
 * @brief Initialize flush task and timer
 * @return WW_OK->Success, WW_ERR->Failed
 * @details Invocation: System initialization
 */
WW_RTN log_flush_task_init(void);

/**
 * @brief Notify the flush task to perform flush.
 * @note Release the semaphore to wake up the flush task.
 * @details Triggered upon detection of pending_len >= threshold in log_ram_write()
 */
void log_flush_notify(void);

#endif /* CONFIG_N_LOG_BACKEND_EXT_MEM */

/**
 * @brief Create the log mutex. Must be called once at init whenever the RAM
 *        backend is enabled (logs may be emitted from multiple tasks).
 * @note  Previously the mutex was only created inside log_flush_task_init(),
 *        which is EXT_MEM-gated -> in RAM-only builds the mutex stayed NULL and
 *        log_mutex_lock() silently became a no-op (unprotected ring). Split out
 *        so locking works independent of the external-storage backend.
 * @return WW_OK on success, WW_ERR on failure.
 */
WW_RTN log_lock_init(void);

WW_RTN log_mutex_lock(void);
void log_mutex_lock_wait(void);
void log_mutex_unlock(void);
/*************************** declaration end *****************************/

#endif /* CONFIG_N_LOG_BACKEND_RAM */

#ifdef __cplusplus
}
#endif

#endif /* __N_WW_LOG_TASK_H__ */
