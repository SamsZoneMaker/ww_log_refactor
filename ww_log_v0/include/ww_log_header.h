/**
 * @file ww_log_header.h
 * @brief LOG block header management for external storage
 * @date 2026-01-05
 *
 * This module manages LOG block headers for external storage.
 * Each time RAM is flushed to external storage, a header is created.
 */

#ifndef WW_LOG_HEADER_H
#define WW_LOG_HEADER_H

#include "type.h"
#include "ww_log_config.h"

/* ========== LOG Block Header Structure ========== */

/**
 * External storage LOG block header (32 bytes)
 * Written before each LOG data block in external storage
 */
typedef struct {
    U32 magic;/**< Magic number: 0x4C4F4748 ('LOGH') */
    U32 sequence;           /**< Sequence number (incrementing) */
    U32 timestamp;          /**< Timestamp (optional, system tick) */
    U16 data_size;          /**< Data size in this block */
    U16 entry_count;        /**< Number of LOG entries */
    U8  ram_overflow;       /**< RAM overflow flag */
    U8  reserved[11];       /**< Reserved for future use */
    U32 checksum;           /**< Checksum of first 28 bytes */
} LOG_BLOCK_HEADER_T;

/* Magic number for LOG block header */
#define LOG_BLOCK_MAGIC  0x4C4F4748  /* 'LOGH' */

/* ========== Public Functions ========== */

/**
 * @brief Initialize header management
 *
 * This should be called after storage init.
 * It will scan external storage to find the maximum sequence number.
 */
void log_header_init(void);

/**
 * @brief Get next sequence number
 *
 * @return Next sequence number (auto-incrementing)
 */
U32 log_header_get_next_sequence(void);

/**
 * @brief Build a LOG block header
 *
 * @param header [out] Header structure to fill
 * @param data_size Size of data in this block
 * @param entry_count Number of LOG entries
 * @param ram_overflow RAM overflow flag
 *
 * @return 0=success, -1=error
 */
int log_header_build(LOG_BLOCK_HEADER_T *header, U16 data_size,
                     U16 entry_count,
                     U8 ram_overflow);

/**
 * @brief Validate a LOG block header
 *
 * @param header Header to validate
 * @return 1=valid, 0=invalid
 */
U8 log_header_validate(const LOG_BLOCK_HEADER_T *header);

/**
 * @brief Calculate checksum for header
 *
 * @param header Header structure
 * @return Calculated checksum
 */
U32 log_header_calc_checksum(const LOG_BLOCK_HEADER_T *header);

/**
 * @brief Scan external storage for maximum sequence number
 *
 * This is used during initialization to recover the sequence counter.
 *
 * @return Maximum sequence number found, 0 if none found
 */
U32 log_header_scan_max_sequence(void);

/**
 * @brief Get current sequence number (without incrementing)
 *
 * @return Current sequence number
 */
U32 log_header_get_current_sequence(void);

#endif /* WW_LOG_HEADER_H */
