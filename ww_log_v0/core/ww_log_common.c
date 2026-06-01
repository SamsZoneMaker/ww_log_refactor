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
