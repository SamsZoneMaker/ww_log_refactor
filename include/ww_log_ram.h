/**
 * @file ww_log_ram.h
 * @brief RAM buffer management for LOG module Phase2
 * @date 2025-12-24
 *
 * This module implements a Ring Buffer in the DLM memory region for LOG storage.
 * Features:
 * - 4KB total space (64B header + 4032B data)
 * - Ring buffer with automatic wrap-around
 * - Flush trigger at 3KB threshold
 * - Hot restart recovery support
 */

#ifndef WW_LOG_RAM_H
#define WW_LOG_RAM_H

#include "type.h"
#include "ww_log_config.h"

/* ========== Data Structures ========== */

/**
 * RAM LOG Header (64 bytes)
 * Located at DLM_MAINTAIN_LOG_BASE_ADDR
 */
typedef struct {
    U32 magic;              /**< Magic number: 0x574C4F47 ('WLOG') */
    U32 version;            /**< Version: 0x00020000 (Phase2) */
    U16 write_index;        /**< Write pointer (byte offset in data area) */
    U16 read_index;         /**< Read pointer (byte offset in data area) */
    U32 total_written;      /**< Total bytes written (累计) */
    U32 flush_count;        /**< Number of flushes to external storage */
    U32 last_flush_time;    /**< Last flush timestamp (optional) */
    U8  overflow_flag;      /**< Overflow flag: 1=wrapped around */
    U8  reserved[35];       /**< Reserved for future use */
    U32 checksum;           /**< Checksum of first 60 bytes */
} LOG_RAM_HEADER_T;

/**
 * Ring Buffer management structure
 */
typedef struct {
    LOG_RAM_HEADER_T *header;   /**< Pointer to header */
    U8 *data;                   /**< Pointer to data area */
    U16 data_size;              /**< Data area size (4032 bytes) */
    U16 threshold;              /**< Flush threshold (3008 bytes) */
} LOG_RAM_BUFFER_T;

/**
 * RAM buffer statistics (optional)
 */
#ifdef LOG_RAM_STATISTICS
typedef struct {
    U32 write_calls;            /**< Number of write calls */
    U32 write_bytes;            /**< Total bytes written */
    U32 flush_triggers;         /**< Number of flush triggers */
    U32 overflow_count;         /**< Number of overflows */
    U16 peak_usage;             /**< Peak usage in bytes */
} LOG_RAM_STATS_T;
#endif

/* ========== Public Functions ========== */

/**
 * @brief Initialize RAM buffer
 *
 * This function should be called during system initialization.
 * It will:
 * - Check magic number for hot restart recovery
 * - Initialize header if magic is invalid
 * - Clear data area if needed
 *
 * @param force_clear 1=force clear, 0=try to preserve data
 */
void log_ram_init(U8 force_clear);

/**
 * @brief Write LOG entry to RAM buffer
 *
 * @param encoded Encoded LOG entry (32-bit)
 * @param params Parameter array (can be NULL if param_count=0)
 * @param param_count Number of parameters (0-16)
 *
 * @return 0=success, -1=error, 1=need flush
 */
int log_ram_write(U32 encoded, U32 *params, U8 param_count);

/**
 * @brief Check if flush is needed
 *
 * @return 1=need flush, 0=no need
 */
U8 log_ram_need_flush(void);

/**
 * @brief Get current RAM usage in bytes
 *
 * @return Current usage (0 to data_size)
 */
U16 log_ram_get_usage(void);

/**
 * @brief Get available space in bytes
 *
 * @return Available space
 */
U16 log_ram_get_available(void);

/**
 * @brief Read data from RAM buffer
 *
 * This function reads data from read_index to write_index.
 * It handles ring buffer wrap-around automatically.
 *
 * @param buffer Output buffer
 * @param max_size Maximum size to read
 * @param actual_size [out] Actual size read
 *
 * @return 0=success, -1=error
 */
int log_ram_read(U8 *buffer, U16 max_size, U16 *actual_size);

/**
 * @brief Clear flushed data from RAM
 *
 * This function advances the read_index after data has been
 * successfully flushed to external storage.
 *
 * @param size Number of bytes to clear
 */
void log_ram_clear_flushed(U16 size);

/**
 * @brief Clear all RAM buffer data
 *
 * This resets write_index and read_index to 0.
 * Header statistics are preserved.
 */
void log_ram_clear_all(void);

/**
 * @brief Get RAM buffer header
 *
 * @return Pointer to header (read-only)
 */
const LOG_RAM_HEADER_T* log_ram_get_header(void);

/**
 * @brief Get RAM buffer statistics
 *
 * @param stats [out] Statistics structure
 */
#ifdef LOG_RAM_STATISTICS
void log_ram_get_stats(LOG_RAM_STATS_T *stats);
#endif

/**
 * @brief Dump RAM buffer content (for debugging)
 *
 * This function prints the RAM buffer content in hex format.
 * Useful for debugging and testing.
 */
void log_ram_dump_hex(void);

/**
 * @brief Validate RAM buffer integrity
 *
 * @return 1=valid, 0=invalid
 */
U8 log_ram_validate(void);

/**
 * @brief Calculate checksum for header
 *
 * @param header Pointer to header
 * @return Calculated checksum
 */
U32 log_ram_calc_checksum(const LOG_RAM_HEADER_T *header);

#endif /* WW_LOG_RAM_H */
