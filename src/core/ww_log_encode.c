/**
 * @file ww_log_encode.c
 * @brief Encode mode logging implementation
 * @date 2025-11-29
 */

#include "ww_log.h"
#include <stdio.h>

#ifdef WW_LOG_MODE_ENCODE

/* ========== RAM Buffer (Optional) ========== */

#ifdef WW_LOG_ENCODE_RAM_BUFFER_EN

/**
 * Global RAM buffer instance
 */
WW_LOG_RAM_BUFFER_T g_ww_log_ram_buffer = {
    .magic = WW_LOG_RAM_MAGIC,
    .head = 0,
    .tail = 0,
    .entries = {0}
};

/**
 * @brief Check if RAM buffer is full
 * @return 1 if full, 0 if not full
 */
static U8 ww_log_ram_is_full(void)
{
    U16 next_tail = (g_ww_log_ram_buffer.tail + 1) % WW_LOG_RAM_BUFFER_SIZE;
    return (next_tail == g_ww_log_ram_buffer.head);
}

/**
 * @brief Write one U32 entry to RAM buffer
 * @param data Data to write
 * @return 0 on success, -1 if buffer full
 */
static S8 ww_log_ram_write(U32 data)
{
    if (ww_log_ram_is_full()) {
        /* Buffer full - drop new entry */
        return -1;
    }

    /* Write data */
    g_ww_log_ram_buffer.entries[g_ww_log_ram_buffer.tail] = data;

    /* Advance tail pointer */
    g_ww_log_ram_buffer.tail = (g_ww_log_ram_buffer.tail + 1) % WW_LOG_RAM_BUFFER_SIZE;

    return 0;
}

/**
 * @brief Get number of entries in RAM buffer
 * @return Number of entries
 */
U16 ww_log_ram_get_count(void)
{
    if (g_ww_log_ram_buffer.tail >= g_ww_log_ram_buffer.head) {
        return g_ww_log_ram_buffer.tail - g_ww_log_ram_buffer.head;
    } else {
        return WW_LOG_RAM_BUFFER_SIZE - g_ww_log_ram_buffer.head + g_ww_log_ram_buffer.tail;
    }
}

/**
 * @brief Dump all entries in RAM buffer to stdout
 */
void ww_log_ram_dump(void)
{
    printf("\n===== LOG RAM BUFFER DUMP =====\n");
    printf("Magic: 0x%08X %s\n",
           g_ww_log_ram_buffer.magic,
           (g_ww_log_ram_buffer.magic == WW_LOG_RAM_MAGIC) ? "(VALID)" : "(INVALID)");
    printf("Head: %u, Tail: %u, Count: %u\n",
           g_ww_log_ram_buffer.head,
           g_ww_log_ram_buffer.tail,
           ww_log_ram_get_count());
    printf("-------------------------------\n");

    U16 idx = g_ww_log_ram_buffer.head;
    U16 count = 0;

    while (idx != g_ww_log_ram_buffer.tail) {
        U32 entry = g_ww_log_ram_buffer.entries[idx];

        /* Decode and print */
        U16 log_id = WW_LOG_DECODE_LOG_ID(entry);
        U16 line = WW_LOG_DECODE_LINE(entry);
        U8 level = WW_LOG_DECODE_LEVEL(entry);
        U8 msg_id = WW_LOG_DECODE_MSG_ID(entry);

        printf("[%04u] 0x%08X -> LogID:%3u Line:%4u Level:%u MsgID:%u\n",
               count++,
               entry,
               log_id,
               line,
               level,
               msg_id);

        idx = (idx + 1) % WW_LOG_RAM_BUFFER_SIZE;
    }

    printf("===============================\n\n");
}

/**
 * @brief Clear RAM buffer
 */
void ww_log_ram_clear(void)
{
    g_ww_log_ram_buffer.head = 0;
    g_ww_log_ram_buffer.tail = 0;
}

#endif /* WW_LOG_ENCODE_RAM_BUFFER_EN */

/* ========== Core Encoding Function ========== */

/**
 * @brief Core encode mode output function
 * @param encoded_log 32-bit encoded log entry
 *
 * This function:
 * 1. Outputs to RAM buffer (if enabled)
 * 2. Outputs to UART as hex (for debugging/decoding)
 */
void ww_log_encode_output(U32 encoded_log)
{
#ifdef WW_LOG_ENCODE_RAM_BUFFER_EN
    /* Write to RAM buffer */
    ww_log_ram_write(encoded_log);
#endif

    /* Output to UART as hex for debugging */
    printf("0x%08X\n", encoded_log);
    fflush(stdout);
}

#endif /* WW_LOG_MODE_ENCODE */
