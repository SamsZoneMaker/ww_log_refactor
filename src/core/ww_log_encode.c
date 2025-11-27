/**
 * @file ww_log_encode.c
 * @brief Encode mode implementation
 * @date 2025-11-18
 */

#include "ww_log.h"

#ifdef CONFIG_WW_LOG_ENCODE_MODE

/* ========== Output Hook ========== */

/**
 * Custom output hook (NULL = no real-time UART output)
 */
static ww_log_encode_hook_t g_encode_output_hook = NULL;

/**
 * @brief Install custom output hook for encode mode
 */
void ww_log_encode_hook_install(ww_log_encode_hook_t fn)
{
    g_encode_output_hook = fn;
}

/* ========== RAM Buffer Management ========== */

#ifdef CONFIG_WW_LOG_RAM_BUFFER_EN

/**
 * @brief Check if RAM buffer is full
 * @return 1 if full, 0 if not full
 */
static U8 ww_log_ram_is_full(void)
{
    U16 next_tail = (g_ww_log_ram.tail + 1) % CONFIG_WW_LOG_RAM_ENTRY_NUM;
    return (next_tail == g_ww_log_ram.head);
}

/**
 * @brief Write one U32 entry to RAM buffer
 * @param data Data to write
 * @return 0 on success, -1 if buffer full
 */
static S8 ww_log_ram_write(U32 data)
{
    if (ww_log_ram_is_full()) {
        /* Buffer full - could overwrite oldest entry or drop new entry */
        /* Here we choose to drop new entry */
        return -1;
    }

    /* Write data */
    g_ww_log_ram.entries[g_ww_log_ram.tail] = data;

    /* Advance tail pointer */
    g_ww_log_ram.tail = (g_ww_log_ram.tail + 1) % CONFIG_WW_LOG_RAM_ENTRY_NUM;

    return 0;
}

#endif /* CONFIG_WW_LOG_RAM_BUFFER_EN */

/* ========== UART Output ========== */

#ifdef CONFIG_WW_LOG_OUTPUT_UART

/**
 * @brief Output encoded log entry to UART
 * @param header Encoded header (file_id, line, level, param_count)
 * @param params Parameter array
 * @param param_count Number of parameters
 */
static void ww_log_uart_output(U32 header, const U32 *params, U8 param_count)
{
    /* Use hook if installed */
    if (g_encode_output_hook) {
        /* Output header */
        g_encode_output_hook(header);

        /* Output parameters */
        for (U8 i = 0; i < param_count; i++) {
            g_encode_output_hook(params[i]);
        }
    } else {
        /* Fallback: print as hex (for debugging) */
        printf("0x%08X", header);
        for (U8 i = 0; i < param_count; i++) {
            printf(" 0x%08X", params[i]);
        }
        printf("\n");
    }
}

#endif /* CONFIG_WW_LOG_OUTPUT_UART */

/* ========== Core Encoding Functions ========== */

/**
 * @brief Internal logging function with N parameters
 * @param module_id Module ID
 * @param level Log level
 * @param file_id File ID
 * @param line Line number
 * @param param_count Number of parameters
 * @param params Parameter array
 */
static void ww_log_encode_internal(WW_LOG_MODULE_E module_id, WW_LOG_LEVEL_E level,
                                   U16 file_id, U16 line, U8 param_count, const U32 *params)
{
    /* Check if module is enabled */
    if (!ww_log_is_mod_enabled(module_id)) {
        return;
    }

    /* Check if log level is enabled */
    if (!ww_log_is_level_enabled(level)) {
        return;
    }

    /* Encode header */
    U32 header = WW_LOG_ENCODE_HEADER(file_id, line, level, param_count);

#ifdef CONFIG_WW_LOG_RAM_BUFFER_EN
    /* Write to RAM buffer */
    /* Critical section start (TODO: add mutex if RTOS is used) */

    /* Write header */
    if (ww_log_ram_write(header) != 0) {
        /* Buffer full - could not write */
        /* In production, might want to set an overflow flag */
    } else {
        /* Write parameters */
        for (U8 i = 0; i < param_count; i++) {
            if (ww_log_ram_write(params[i]) != 0) {
                /* Buffer full during parameter write */
                break;
            }
        }
    }

    /* Critical section end */
#endif

#ifdef CONFIG_WW_LOG_OUTPUT_UART
    /* Output to UART in real-time */
    ww_log_uart_output(header, params, param_count);
#endif
}

/**
 * @brief Encode mode logging with 0 parameters
 */
void ww_log_encode_0(WW_LOG_MODULE_E module_id, WW_LOG_LEVEL_E level,
                     U16 file_id, U16 line)
{
    ww_log_encode_internal(module_id, level, file_id, line, 0, NULL);
}

/**
 * @brief Encode mode logging with 1 parameter
 */
void ww_log_encode_1(WW_LOG_MODULE_E module_id, WW_LOG_LEVEL_E level,
                     U16 file_id, U16 line, U32 param1)
{
    U32 params[1] = { param1 };
    ww_log_encode_internal(module_id, level, file_id, line, 1, params);
}

/**
 * @brief Encode mode logging with 2 parameters
 */
void ww_log_encode_2(WW_LOG_MODULE_E module_id, WW_LOG_LEVEL_E level,
                     U16 file_id, U16 line, U32 param1, U32 param2)
{
    U32 params[2] = { param1, param2 };
    ww_log_encode_internal(module_id, level, file_id, line, 2, params);
}

/**
 * @brief Encode mode logging with 3 parameters
 */
void ww_log_encode_3(WW_LOG_MODULE_E module_id, WW_LOG_LEVEL_E level,
                     U16 file_id, U16 line, U32 param1, U32 param2, U32 param3)
{
    U32 params[3] = { param1, param2, param3 };
    ww_log_encode_internal(module_id, level, file_id, line, 3, params);
}

/* ========== Buffer Read/Dump Functions ========== */

#ifdef CONFIG_WW_LOG_RAM_BUFFER_EN

/**
 * @brief Get number of entries in RAM buffer
 * @return Number of entries
 */
U16 ww_log_ram_get_count(void)
{
    if (g_ww_log_ram.tail >= g_ww_log_ram.head) {
        return g_ww_log_ram.tail - g_ww_log_ram.head;
    } else {
        return CONFIG_WW_LOG_RAM_ENTRY_NUM - g_ww_log_ram.head + g_ww_log_ram.tail;
    }
}

/**
 * @brief Dump all entries in RAM buffer to UART
 */
void ww_log_ram_dump(void)
{
    printf("\n===== LOG RAM BUFFER DUMP =====\n");
    printf("Head: %u, Tail: %u, Count: %u\n",
           g_ww_log_ram.head, g_ww_log_ram.tail, ww_log_ram_get_count());
    printf("-------------------------------\n");

    U16 idx = g_ww_log_ram.head;
    U16 count = 0;

    while (idx != g_ww_log_ram.tail) {
        U32 entry = g_ww_log_ram.entries[idx];

        /* Check if this is a header (by checking if it's a valid encoded header) */
        /* A simple heuristic: if the next_len field makes sense */
        U8 param_count = WW_LOG_GET_PARAM_COUNT(entry);

        if (param_count <= 63) {  /* Valid param count */
            /* This looks like a header */
            printf("[%04u] Header: 0x%08X (File:%u Line:%u Level:%u Params:%u)\n",
                   count++,
                   entry,
                   WW_LOG_GET_FILE_ID(entry),
                   WW_LOG_GET_LINE_NO(entry),
                   WW_LOG_GET_LEVEL(entry),
                   param_count);

            /* Print parameters */
            for (U8 i = 0; i < param_count; i++) {
                idx = (idx + 1) % CONFIG_WW_LOG_RAM_ENTRY_NUM;
                if (idx == g_ww_log_ram.tail) {
                    printf("  [WARNING] Incomplete parameters\n");
                    break;
                }
                printf("       Param%u: 0x%08X (%u)\n",
                       i + 1,
                       g_ww_log_ram.entries[idx],
                       g_ww_log_ram.entries[idx]);
            }
        } else {
            /* Possibly corrupted or a parameter */
            printf("[%04u] Data: 0x%08X\n", count++, entry);
        }

        idx = (idx + 1) % CONFIG_WW_LOG_RAM_ENTRY_NUM;
    }

    printf("===============================\n\n");
}

/**
 * @brief Clear RAM buffer
 */
void ww_log_ram_clear(void)
{
    g_ww_log_ram.head = 0;
    g_ww_log_ram.tail = 0;
}

#endif /* CONFIG_WW_LOG_RAM_BUFFER_EN */

#endif /* CONFIG_WW_LOG_ENCODE_MODE */
