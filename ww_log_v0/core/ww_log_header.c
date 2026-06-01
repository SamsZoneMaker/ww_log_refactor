/**
 * @file ww_log_header.c
 * @brief LOG block header management implementation
 * @date 2026-01-05
 */

#include "ww_log_header.h"
#include "ww_log_storage.h"
#include <string.h>
#include <stdio.h>

/* ========== Global Variables ========== */

static U32 g_sequence_counter = 0;
static U8 g_header_initialized = 0;

/* ========== Private Functions ========== */

/**
 * @brief Get system timestamp (placeholder)
 */
static U32 get_timestamp(void)
{
    /* TODO: Implement actual timestamp from RTC or system tick */
    return 0;
}

/* ========== Public Functions ========== */

void log_header_init(void)
{
    if (g_header_initialized) {
        return;
    }

    /* Scan external storage to find maximum sequence number */
    U32 max_seq = log_header_scan_max_sequence();

    /* Start from next sequence */
    g_sequence_counter = max_seq + 1;

#ifdef LOG_DEBUG_VERBOSE
    printf("LOG_HEADER: Initialized, starting sequence=%u\n", g_sequence_counter);
#endif

    g_header_initialized = 1;
}

U32 log_header_get_next_sequence(void)
{
    return g_sequence_counter++;
}

U32 log_header_get_current_sequence(void)
{
    return g_sequence_counter;
}

int log_header_build(LOG_BLOCK_HEADER_T *header, U16 data_size,
                     U16 entry_count, U8 ram_overflow)
{
    if (header == NULL) {
        return -1;
    }

    /* Clear header */
    memset(header, 0, sizeof(LOG_BLOCK_HEADER_T));

    /* Fill header fields */
    header->magic = LOG_BLOCK_MAGIC;
    header->sequence = log_header_get_next_sequence();
    header->timestamp = get_timestamp();
    header->data_size = data_size;
    header->entry_count = entry_count;
    header->ram_overflow = ram_overflow;

    /* Calculate checksum (exclude checksum field itself) */
    header->checksum = log_header_calc_checksum(header);

    return 0;
}

U8 log_header_validate(const LOG_BLOCK_HEADER_T *header)
{
    if (header == NULL) {
        return 0;
    }

    /* Check magic number */
    if (header->magic != LOG_BLOCK_MAGIC) {
        return 0;
    }

    /* Check data size */
    if (header->data_size > LOG_STORAGE_PARTITION_SIZE - sizeof(LOG_BLOCK_HEADER_T)) {
        return 0;
    }

    /* Verify checksum */
    U32 calculated = log_header_calc_checksum(header);
    if (calculated != header->checksum) {
        return 0;
    }

    return 1;
}

U32 log_header_calc_checksum(const LOG_BLOCK_HEADER_T *header)
{
    U32 checksum = 0;
    const U8 *data = (const U8*)header;

    /* Calculate checksum for first 28 bytes (exclude checksum field) */
    for (U16 i = 0; i < (sizeof(LOG_BLOCK_HEADER_T) - sizeof(U32)); i++) {
        checksum += data[i];
    }

    return checksum;
}

U32 log_header_scan_max_sequence(void)
{
    U32 max_sequence = 0;
    LOG_BLOCK_HEADER_T header;

    /* Check if storage is available */
    if (!log_storage_is_available()) {
        return 0;
    }

    /* Read header from external storage */
    /* For simplicity, we only check the first block */
    /* In a full implementation, we would scan all possible blocks */
    if (log_storage_read(0, (U8*)&header, sizeof(header)) == 0) {
        if (log_header_validate(&header)) {
            max_sequence = header.sequence;
#ifdef LOG_DEBUG_VERBOSE
            printf("LOG_HEADER: Found existing block, sequence=%u\n", max_sequence);
#endif
        }
    }

    return max_sequence;
}
