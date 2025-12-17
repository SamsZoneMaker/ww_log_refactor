/**
 * @file ww_log_common.c
 * @brief Common log system utilities
 * @date 2025-11-29
 */

#include "ww_log.h"
#include <stdio.h>

/**
 * @brief Initialize log system (optional, for future extensions)
 *
 * Currently this is a placeholder. Future enhancements might include:
 * - UART initialization
 * - RAM buffer warm restart detection
 * - Output hook configuration
 */
void ww_log_init(void)
{
#ifdef WW_LOG_ENCODE_RAM_BUFFER_EN
    /* Check for warm restart (magic number detection) */
    if (g_ww_log_ram_buffer.magic != WW_LOG_RAM_MAGIC) {
        /* Cold start or first initialization */
        g_ww_log_ram_buffer.magic = WW_LOG_RAM_MAGIC;
        g_ww_log_ram_buffer.head = 0;
        g_ww_log_ram_buffer.tail = 0;

        /* Clear buffer contents */
        for (U16 i = 0; i < WW_LOG_RAM_BUFFER_SIZE; i++) {
            g_ww_log_ram_buffer.entries[i] = 0;
        }

        printf("LOG: Cold start - RAM buffer initialized\n");
    } else {
        /* Warm restart detected - preserve existing logs */
        printf("LOG: Warm restart - %u logs preserved\n", ww_log_ram_get_count());
    }
#endif

    printf("LOG: System initialized (mode: ");
#if defined(WW_LOG_MODE_ENCODE)
    printf("ENCODE)\n");
#elif defined(WW_LOG_MODE_STR)
    printf("STRING)\n");
#elif defined(WW_LOG_MODE_DISABLED)
    printf("DISABLED)\n");
#else
    printf("UNKNOWN)\n");
#endif
}
