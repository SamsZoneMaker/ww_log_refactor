/**
 * @file ww_log_flush.h
 * @brief LOG flush mechanism - RAM to external storage
 * @date 2026-01-05
 *
 * This module handles flushing LOG data from RAM to external storage.
 *
 * Key features:
 * - Asynchronous flush (non-blocking)
 * - Allows continued writing to remaining 1KB RAM during flush
 * - Automatic trigger at 3KB threshold
 * - Manual trigger support
 */

#ifndef WW_LOG_FLUSH_H
#define WW_LOG_FLUSH_H

#include "type.h"

/* ========== Flush Status ========== */

typedef enum {
    FLUSH_STATUS_IDLE = 0,      /**< No flush in progress */
    FLUSH_STATUS_PENDING,       /**< Flush requested but not started */
    FLUSH_STATUS_IN_PROGRESS,   /**< Flush in progress */
    FLUSH_STATUS_COMPLETED,     /**< Flush completed successfully */
    FLUSH_STATUS_FAILED         /**< Flush failed */
} FLUSH_STATUS_E;

/* ========== Public Functions ========== */

/**
 * @brief Initialize flush mechanism
 *
 * This should be called after log_storage_init() and log_header_init().
 */
void log_flush_init(void);

/**
 * @brief Request a flush operation
 *
 * This function is non-blocking. It marks that a flush is needed.
 * The actual flush will be performed by log_flush_process().
 *
 * @param force 1=force flush even if below threshold, 0=normal
 * @return 0=request accepted, -1=flush already in progress
 */
int log_flush_request(U8 force);

/**
 * @brief Process pending flush operations
 *
 * This function should be called periodically (e.g., in main loop or timer).
 * It performs the actual flush operation if one is pending.
 *
 * The flush process:
 * 1. Read data from RAM (up to 3KB)
 * 2. Build block header
 * 3. Write to external storage
 * 4. Clear flushed data from RAM
 *
 * During flush, the remaining 1KB RAM space can still accept new LOGs.
 *
 * @return 0=success or no flush needed, -1=error
 */
int log_flush_process(void);

/**
 * @brief Get current flush status
 *
 * @return Current flush status
 */
FLUSH_STATUS_E log_flush_get_status(void);

/**
 * @brief Check if flush is in progress
 *
 * @return 1=flush in progress, 0=idle
 */
U8 log_flush_is_busy(void);

/**
 * @brief Manual flush trigger (blocking)
 *
 * This function performs an immediate flush and waits for completion.
 * Use this for critical situations (e.g., before system reset).
 *
 * @return 0=success, -1=error
 */
int log_flush_now(void);

/**
 * @brief Get flush statistics
 *
 * @param total_flushes [out] Total number of flushes
 * @param failed_flushes [out] Number of failed flushes
 * @param last_flush_size [out] Size of last flush in bytes
 */
void log_flush_get_stats(U32 *total_flushes, U32 *failed_flushes, U16 *last_flush_size);

#endif /* WW_LOG_FLUSH_H */
