/**
 * @file ww_log_flush.c
 * @brief LOG flush mechanism implementation
 * @date 2026-01-05
 *
 * Key design:
 * - Non-blocking flush: allows continued writing during flush
 * - Two-stage process: request + process
 * - Flushes up to 3KB, leaving 1KB for new LOGs
 */

#include "ww_log_flush.h"
#include "ww_log_ram.h"
#include "ww_log_storage.h"
#include "ww_log_header.h"
#include <string.h>
#include <stdio.h>

/* ========== Global Variables ========== */

static FLUSH_STATUS_E g_flush_status = FLUSH_STATUS_IDLE;
static U8 g_flush_force = 0;

/* Statistics */
static U32 g_total_flushes = 0;
static U32 g_failed_flushes = 0;
static U16 g_last_flush_size = 0;

/* Flush buffer (static to avoid stack overflow) */
static U8 g_flush_buffer[4096];

/* ========== Private Functions ========== */

/**
 * @brief Count number of LOG entries in buffer
 *
 * This is a simplified version that counts U32 words.
 * A more accurate version would parse the encoded format.
 */
static U16 count_log_entries(const U8 *buffer, U16 size)
{
    /* Simplified: assume average entry is 8 bytes (1 encoded + 1 param) */
    return size / 8;
}

/**
 * @brief Perform the actual flush operation
 *
 * This function:
 * 1. Reads data from RAM (up to 3KB to leave 1KB free)
 * 2. Builds block header
 * 3. Writes to external storage
 * 4. Clears flushed data from RAM
 *
 * @return 0=success, -1=error
 */
static int perform_flush(void)
{
    U16 actual_size = 0;
    U16 to_flush;
    int ret;
    LOG_BLOCK_HEADER_T header;
    const LOG_RAM_HEADER_T *ram_header;

    /* Get current RAM usage */
    U16 usage = log_ram_get_usage();

    if (usage == 0 && !g_flush_force) {
        /* Nothing to flush */
        return 0;
    }

    /* Determine how much to flush */
    /* Strategy: Flush up to 3KB, leaving at least 1KB space for new LOGs */
    if (usage > 3072) {
        to_flush = 3072;  /* Flush 3KB */
    } else if (g_flush_force) {
        to_flush = usage;  /* Flush all if forced */
    } else {
        to_flush = usage;  /* Flush all if below threshold */
    }

#ifdef LOG_DEBUG_VERBOSE
    printf("LOG_FLUSH: Starting flush, usage=%u, to_flush=%u\n", usage, to_flush);
#endif

    /* Read data from RAM */
    ret = log_ram_read(g_flush_buffer, to_flush, &actual_size);
    if (ret != 0 || actual_size == 0) {
        printf("LOG_FLUSH: Failed to read from RAM\n");
        return -1;
    }

    /* Get RAM header for overflow flag */
    ram_header = log_ram_get_header();

    /* Build block header */
    ret = log_header_build(&header, actual_size,
                          count_log_entries(g_flush_buffer, actual_size),
                          ram_header->overflow_flag);
    if (ret != 0) {
        printf("LOG_FLUSH: Failed to build header\n");
        return -1;
    }

#ifdef LOG_DEBUG_VERBOSE
    printf("LOG_FLUSH: Block header - seq=%u, size=%u, entries=%u\n",
           header.sequence, header.data_size, header.entry_count);
#endif

    /* Write header to external storage */
    ret = log_storage_write(0, (U8*)&header, sizeof(header));
    if (ret != 0) {
        printf("LOG_FLUSH: Failed to write header to storage\n");
        return -1;
    }

    /* Write data to external storage */
    ret = log_storage_write(sizeof(header), g_flush_buffer, actual_size);
    if (ret != 0) {
        printf("LOG_FLUSH: Failed to write data to storage\n");
        return -1;
    }

    /* Clear flushed data from RAM */
    log_ram_clear_flushed(actual_size);

    /* Update statistics */
    g_last_flush_size = actual_size;

#ifdef LOG_DEBUG_VERBOSE
    printf("LOG_FLUSH: Flush completed, %u bytes written\n", actual_size);
    printf("LOG_FLUSH: RAM usage after flush: %u bytes\n", log_ram_get_usage());
#endif

    return 0;
}

/* ========== Public Functions ========== */

void log_flush_init(void)
{
    g_flush_status = FLUSH_STATUS_IDLE;
    g_flush_force = 0;
    g_total_flushes = 0;
    g_failed_flushes = 0;
    g_last_flush_size = 0;

#ifdef LOG_DEBUG_VERBOSE
    printf("LOG_FLUSH: Initialized\n");
#endif
}

int log_flush_request(U8 force)
{
    /* Check if flush is already in progress */
    if (g_flush_status == FLUSH_STATUS_IN_PROGRESS) {
        return -1;  /* Busy */
    }

    /* Mark flush as pending */
    g_flush_status = FLUSH_STATUS_PENDING;
    g_flush_force = force;

#ifdef LOG_DEBUG_VERBOSE
    printf("LOG_FLUSH: Flush requested (force=%u)\n", force);
#endif

    return 0;
}

int log_flush_process(void)
{
    int ret;

    /* Check if there's a pending flush */
    if (g_flush_status != FLUSH_STATUS_PENDING) {
        return 0;  /* Nothing to do */
    }

    /* Mark as in progress */
    g_flush_status = FLUSH_STATUS_IN_PROGRESS;

    /* Perform flush */
    ret = perform_flush();

    if (ret == 0) {
        /* Success */
        g_flush_status = FLUSH_STATUS_COMPLETED;
        g_total_flushes++;
        /* Reset to idle for next flush */
        g_flush_status = FLUSH_STATUS_IDLE;
        g_flush_force = 0;
    } else {
        /* Failed */
        g_flush_status = FLUSH_STATUS_FAILED;
        g_failed_flushes++;

        printf("LOG_FLUSH: Flush failed (total failures: %u)\n", g_failed_flushes);

        /* Reset to idle to allow retry */
        g_flush_status = FLUSH_STATUS_IDLE;
        g_flush_force = 0;
    }

    return ret;
}

FLUSH_STATUS_E log_flush_get_status(void)
{
    return g_flush_status;
}

U8 log_flush_is_busy(void)
{
    return (g_flush_status == FLUSH_STATUS_IN_PROGRESS) ? 1 : 0;
}

int log_flush_now(void)
{
    int ret;
    int timeout = 100;  /* 100 iterations timeout */

    /* Request flush */
    ret = log_flush_request(1);  /* Force flush */
    if (ret != 0) {
        /* Already in progress, wait for completion */
        while (log_flush_is_busy() && timeout > 0) {
            timeout--;
            /* In real system, add small delay here */
        }

        if (timeout == 0) {
            printf("LOG_FLUSH: Timeout waiting for flush\n");
            return -1;
        }
        return 0;
    }

    /* Process flush immediately */
    ret = log_flush_process();

    return ret;
}

void log_flush_get_stats(U32 *total_flushes, U32 *failed_flushes, U16 *last_flush_size)
{
    if (total_flushes != NULL) {
        *total_flushes = g_total_flushes;
    }

    if (failed_flushes != NULL) {
        *failed_flushes = g_failed_flushes;
    }

    if (last_flush_size != NULL) {
        *last_flush_size = g_last_flush_size;
    }
}
